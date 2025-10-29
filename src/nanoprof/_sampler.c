#pragma region defines

#define SUPER_UNSAFE 0
#define INC_CODE_REF 1

#define MAX_STACK_DEPTH 1024
#define COUNTER_MAX 7

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#pragma endregion

#pragma region headers

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <Python.h>
#include <opcode.h>

#define Py_BUILD_CORE
#include <internal/pycore_frame.h>
#if PY_MINOR_VERSION >= 13
#include <internal/pycore_ceval.h>
#include <internal/pycore_interp.h>
#include <internal/pycore_pystate.h>
#include <internal/pycore_setobject.h>
#endif
#if PY_MINOR_VERSION == 14
#include <internal/pycore_genobject.h>
#include <internal/pycore_interpframe.h>
#include <internal/pycore_interpframe_structs.h>
#include <internal/pycore_runtime.h>
#endif
#undef Py_BUILD_CORE

#include "_sampler_time.h"

#if SUPER_UNSAFE

#define Py_BUILD_CORE
#include <internal/pycore_gil.h>
#include <internal/pycore_interp.h>
#include <internal/pycore_runtime.h>
static struct _gil_runtime_state *GIL = NULL;
#endif


#pragma endregion

#pragma region globals

static uint64_t DEPTH = 0;
static Py_ssize_t SIZE[MAX_STACK_DEPTH];
static const char* STACK[MAX_STACK_DEPTH];

static uint64_t COUNTERS[COUNTER_MAX];

static Py_tss_t STATE_KEY = Py_tss_NEEDS_INIT;
static Py_tss_t THREAD_KEY = Py_tss_NEEDS_INIT;

int profile_debug = 0;
uint64_t profile_setup = 0;
uint64_t profile_start = 0;
uint64_t profile_tprev = 0;
uint64_t profile_eprev = 0;
uint64_t profile_close = 0;

uint64_t min_period = 1e6;
uint64_t rec_period = 1e6;
uint64_t agg_period = 1e9;

uint64_t write_every_ns = 1e7;
uint64_t flush_every_ns = 1e9;

PyObject* sentinel = NULL;

int profile_fileno = -1;
int depth = 0;

void* profile_buffer = NULL;
size_t profile_buffer_dirty = 0;
size_t profile_buffer_limit = 16 * 1024 * 1024;

#pragma endregion

#pragma region stacker

#define COROUTINES 0

#define STACKS_MAX 1024
#define FRAMES_MAX 1024 * 1024
#define TIMERS_MAX 1024 * 1024

typedef uint32_t TimeID;
typedef uint32_t NodeID;
typedef uint32_t Weight;
typedef uint64_t NTicks;
typedef PyCodeObject* PyCode;
typedef _PyInterpreterFrame* PyFrame;

PyObject *asyncio_tasks = NULL;

typedef struct {
    PyCode code;
    NodeID node;
    NTicks time;
} FrameCopy;

typedef struct {
    PyCode code;
    void * func;
} FrameCopyAsync;

typedef struct {
    PyCode code_object;
    NodeID node_caller;
    NodeID head_callee;
    NodeID next_callee;
    NodeID prev_callee;
    TimeID head_thread;
    Weight node_weight;
} FrameNode;

typedef struct {
    uint64_t native_thread_id;
    NTicks time_active;
    NTicks time_paused;
    NTicks time_waited;
    TimeID next;
    NodeID node;
} FrameTime;

typedef struct SamplerThreadState SamplerThreadState;

typedef struct SamplerThreadState {
    uint64_t native_thread_id;
    uint64_t active;
    uint64_t stack_depth;
    SamplerThreadState *sprev;
    SamplerThreadState *snext;
    FrameCopy stack[STACKS_MAX];
} SamplerThreadState;

FrameNode FRAMES[FRAMES_MAX];
FrameTime TIMERS[TIMERS_MAX];

NodeID NEXT_FREE_NODE = 1;
NodeID LAST_SEEN_NODE = 0;
NodeID LAST_FREE_NODE = FRAMES_MAX - 1;
TimeID NEXT_FREE_TIME = 1;
TimeID LAST_FREE_TIME = TIMERS_MAX - 1;

FrameCopyAsync TEMP_ASYNC_FRAMES[FRAMES_MAX];

PyCode async_frame_separator = (void*)-1;

SamplerThreadState* STATE_HEAD = NULL;
SamplerThreadState* STATE_TAIL = NULL;
SamplerThreadState STATE_ERROR = {};

#include "_sampler_util.h"

#pragma region buffer

#define write_typed(T, D) ({ T data = (T) (D); write_buffer(&data, sizeof(T)); })

void write_buffer(const void *data, size_t size)
{
    memcpy(profile_buffer + profile_buffer_dirty, data, size);
    profile_buffer_dirty += size;
}

void write_string(PyObject *pstr)
{
    Py_ssize_t size = 0;
    const char *text = PyUnicode_AsUTF8AndSize(pstr, &size);
    write_typed(uint64_t, text);
    write_typed(uint16_t, size);
    write_buffer(text, size);
}

void write_code(PyCode code)
{
    write_typed(uint16_t, 0xDEC0);
    write_typed(uint64_t, code);
    write_string(code->co_filename);
    write_string(code->co_qualname);
    write_typed(uint16_t, code->co_firstlineno);
}

void write_node(NodeID node_id)
{
    FrameNode node = FRAMES[node_id];
    write_typed(uint16_t, 0xDED0);
    write_typed(uint64_t, node.code_object);
    write_typed(NodeID, node_id);
    write_typed(NodeID, node.node_caller);
}

void write_emit(NTicks time)
{
    write_typed(uint16_t, 0x1010);
    write_typed(uint64_t, time - profile_start);
}

void write_time(TimeID time_id)
{
    FrameTime time = TIMERS[time_id];
    write_typed(uint16_t, 0xFFFF);
    write_typed(uint64_t, time.native_thread_id);
    write_typed(uint32_t, time.node);
    write_typed(uint64_t, time.time_active);
    write_typed(uint64_t, time.time_paused);
    write_typed(uint64_t, time.time_waited);
}

void flush_buffer(void)
{
    size_t count = profile_buffer_dirty;
    const void * start = profile_buffer;
    ssize_t ret = 0;
    while (count) {
        ret = write(profile_fileno, start, count);
        if (ret >= 0) {
            start += ret;
            count -= ret;
        } else {
            break;
        }
    }
    profile_buffer_dirty = 0;
}

#pragma endregion

static inline uint64_t stack_depth(PyFrame frame)
{
    uint64_t depth = 0;
    while (frame) {
        depth++;
        frame = frame->previous;
    }
    return depth;
}

static inline void stack_fill(SamplerThreadState *state, PyFrame frame, uint64_t tick)
{
    // TODO: depth exceeded / code NULL
    PyCode code = _PyFrame_GetCode(frame);
    Py_INCREF(code);
    state->stack[--state->stack_depth] = (FrameCopy) {
        .code = code,
        .node = 0,
        .time = tick,
    };
}

static inline void stack_push(SamplerThreadState *state, PyFrame frame, uint64_t tick)
{
    // TODO: depth exceeded / code NULL
    PyCode code = _PyFrame_GetCode(frame);
    if (__builtin_expect(state == &STATE_ERROR, 0)) {
        return;
    }
    Py_INCREF(code);
    state->stack[state->stack_depth++] = (FrameCopy) {
        .code = code,
        .node = 0,
        .time = tick,
    };
}

static inline void stack_drop(SamplerThreadState *state)
{
    // TODO: depth exceeded
    if (__builtin_expect(state == &STATE_ERROR, 0)) {
        return;
    }
    PyCode code = state->stack[--state->stack_depth].code;
    Py_DECREF(code);
    state->stack[state->stack_depth] = (FrameCopy) {};
}

SamplerThreadState *SamplerThreadState_alloc(PyThreadState *tstate, PyFrame frame, NTicks ticks)
{
    SamplerThreadState *state = NULL;
    PyObject *tdict = NULL;
    PyObject *plong = NULL;

    if (PyThread_tss_set(&THREAD_KEY, &STATE_ERROR) != 0) {
        goto fail;
    }    
    if ((state = PyMem_Calloc(1, sizeof(SamplerThreadState))) == NULL) {
        goto fail;
    }
    if (PyThread_tss_set(&THREAD_KEY, state) != 0) {
        goto fail;
    }
    if ((tdict = _PyThreadState_GetDict(tstate)) == NULL) {
        goto fail;
    }
    if ((plong = PyLong_FromVoidPtr(state)) == NULL) {
        goto fail;
    }
    if (PyDict_SetItem(tdict, sentinel, plong) != 0) {
        goto fail;
    }
    if (STATE_HEAD == NULL) {
        STATE_HEAD = state;
        STATE_TAIL = state;
    } else {
        state->sprev = STATE_TAIL;
        STATE_TAIL->snext = state;
        STATE_TAIL = state;
    }
    state->active = 1;
    state->native_thread_id = tstate->native_thread_id;
    state->stack_depth = 0;
    state->sprev = state->sprev;

    uint64_t depth = stack_depth(frame);
    state->stack_depth = depth;
    while (frame) {
        stack_fill(state, frame, ticks);
        frame = frame->previous;
    }
    state->stack_depth = depth;
    return state;

    fail:
    PyErr_Clear();
    if (state != NULL) {
        PyMem_Free(state);
    }
    Py_XDECREF(plong);
    return &STATE_ERROR;
}

SamplerThreadState *SamplerThreadState_free(SamplerThreadState* state)
{
    if (state == NULL) {
        return NULL;
    }
    if (state == &STATE_ERROR) {
        return NULL;
    }
    if (state->sprev) {
        state->sprev->snext = state->snext;
    }
    if (state->snext) {
        state->snext->sprev = state->sprev;
    }
    if (state == STATE_HEAD) {
        STATE_HEAD = state->snext;
    }
    if (state == STATE_TAIL) {
        STATE_TAIL = state->sprev;
    }
    while (state->stack_depth) {
        PyCode code = state->stack[--state->stack_depth].code;
        Py_DECREF(code);
    }
    while (state->stack_depth) {
        stack_drop(state);
    }
    PyMem_Free(state);
    return NULL;
}

PyObject *sampler_code_pending = NULL;
PyObject *sampler_code_written = NULL;

FrameTime FrameNode_total_time(NodeID node_id) {
    FrameTime total = {};
    for (SamplerThreadState *tts = STATE_HEAD; tts; tts = tts->snext) {
        if (tts->active) {
            TimeID time_id = FRAMES[node_id].head_thread;
            while (time_id) {
                FrameTime timer = TIMERS[time_id];
                total.time_active += timer.time_active;
                total.time_paused += timer.time_paused;
                total.time_waited += timer.time_waited;
                time_id = timer.next;
            }
        }
    }
    return total;
}

void FrameNode_debug(NodeID node_id, int level) {
    int spaces = level * 4;
    FrameNode node = FRAMES[node_id];
    FrameTime total = FrameNode_total_time(node_id);
    const char *name = PyUnicode_AsUTF8(node.code_object->co_qualname);
    printf(
        "TIME: %4d %4u %.9f %.9f %.9f %*s %s\n",
        level,
        node.node_weight,
        total.time_active * 1e-9,
        total.time_paused * 1e-9,
        total.time_waited * 1e-9,
        spaces,
        "",
        name
    );

    NodeID next_id = node.head_callee;
    while (next_id) {
        FrameNode_debug(next_id, level + 1);
        next_id = FRAMES[next_id].next_callee;
    }
}

NodeID FrameNode_upsert(NodeID node_caller, PyCode code_callee)
{
    FrameNode *node = &FRAMES[node_caller];
    NodeID next = NEXT_FREE_NODE;
    NodeID prev = 0;
    if (node->head_callee) {
        node = &FRAMES[node->head_callee];
        while ((node->code_object != code_callee) && (node->next_callee)) {
            node = &FRAMES[node->next_callee];
        }
        if (node->code_object == code_callee) {
            node->node_weight++;
            return node - &FRAMES[0];
        }
        prev = node - &FRAMES[0];
        node->next_callee = next = NEXT_FREE_NODE++;
    } else {
        node->head_callee = next = NEXT_FREE_NODE++;
    }
    FRAMES[next] = (FrameNode){
        .code_object = code_callee,
        .node_caller = node_caller,
        .prev_callee = prev,
        .node_weight = 1,
    };
    node = &FRAMES[next];
    if (next == LAST_FREE_NODE) {
        __builtin_trap();
    }
    return next;
}

void FrameTime_upsert(NodeID node_id, NTicks ticks, SamplerThreadState *sts, int is_active)
{
    TimeID time_id = FRAMES[node_id].head_thread;
    while (time_id && TIMERS[time_id].native_thread_id != sts->native_thread_id) {
        time_id = TIMERS[time_id].next;
    }
    if (time_id == 0) {
        time_id = NEXT_FREE_TIME++;
        TIMERS[time_id].next = FRAMES[node_id].head_thread;
        TIMERS[time_id].node = node_id;
        TIMERS[time_id].native_thread_id = sts->native_thread_id;
        FRAMES[node_id].head_thread = time_id;
    }
    FrameTime *timer = &TIMERS[time_id];
    if (is_active == 1) {
        timer->time_active += ticks;
    }
    if (is_active == 0) {
        timer->time_paused += ticks;
    }
    if (is_active == -1) {
        timer->time_waited += ticks;
    }
}


static int should_sample(uint64_t ticks) {
    return ticks > profile_tprev + rec_period;
}

static NodeID force_node_for_coro(SamplerThreadState *sts)
{
    NodeID node = 0;
    FrameCopy *frame;
    for (unsigned int i = 0; i < sts->stack_depth; i++) {
        frame = &sts->stack[i];
        node = frame->node = FrameNode_upsert(node, frame->code);
//      PySet_Add(sampler_code_pending, (PyObject*) frame->code);
    }
    return node;
}

static void collect_sample(SamplerThreadState *sts, NTicks time_now, int is_active) {
    NodeID node = 0;
    FrameCopy *frame;
    for (unsigned int i = 0; i < sts->stack_depth; i++) {
        frame = &sts->stack[i];
        // We only entree stacks up to min_period, the idea is to avoid tree churn.
        if ((frame->time < time_now) && (time_now - frame->time > min_period)) {
            if (frame->node) {
                node = frame->node;
            } else {
                node = frame->node = FrameNode_upsert(node, frame->code);
                PySet_Add(sampler_code_pending, (PyObject*) frame->code);  // TODO: are refs correct?
            }
        } else {
            break;
        }
    }
    if (node) {
        FrameTime_upsert(node, time_now - profile_tprev, sts, is_active);
    }
}

static int should_emit(uint64_t ticks) {
    return ticks > profile_eprev + agg_period;
}

static void emit_samples(PyThreadState *pts, NTicks time_now) {
    write_emit(time_now);
    // TODO: compact the tree.
    PyObject *updated_code_pending = PyNumber_InPlaceSubtract(sampler_code_pending, sampler_code_written);
    Py_DECREF(sampler_code_pending);
    PyObject *code = NULL;
    while ((code = PySet_Pop(updated_code_pending))) {
        if (profile_debug) {
            const char *text = PyUnicode_AsUTF8(((PyCode) code)->co_name);
            printf("CODE: %p %zu %s\n", code, Py_REFCNT(code), text);
        }
        write_code((PyCode) code);
        PySet_Add(sampler_code_written, code);
    }
    PyErr_Clear();
    sampler_code_pending = updated_code_pending;

    // TODO: just keep an index of last written node?
    for (unsigned int i = 1; i < NEXT_FREE_NODE; i++) {
        if (LAST_SEEN_NODE < i) {
            LAST_SEEN_NODE = i;
            write_node(i);
        }
        if (FRAMES[i].node_caller == 0) {
            if (profile_debug) {
                FrameNode_debug(i, 0);
            }
        }
    }
    if (profile_debug) {
        printf("\n");
    }

    for (unsigned int i = 1; i < NEXT_FREE_TIME; i++) {
        write_time(i);
        FRAMES[TIMERS[i].node].head_thread = 0;
    }
    memset(TIMERS, 0, NEXT_FREE_TIME * sizeof(FrameTime));
    NEXT_FREE_TIME = 1;

    for (SamplerThreadState *tts = STATE_HEAD; tts; tts = tts->snext) {
        NTicks t0 = tick_time();
        COUNTERS[5] += 1;
    //  memset(tts->times, 0, NEXT_FREE_NODE * sizeof(FrameTime));
    //  memset(tts->times, 0, sizeof(tts->times));
        NTicks t1 = tick_time();
        COUNTERS[6] += t1 - t0;
    }

    flush_buffer();

//  PyThread_release_lock((runtime)->interpreters.mutex);
}

PyObject *sampler_atasks(PyObject *self, PyObject *args)
{
    EvalState estate;
    EvalState_save(&estate);

    Py_ssize_t pos = 0;
    PyObject *wref;
    Py_hash_t hash;

    FrameCopyAsync *stacks = TEMP_ASYNC_FRAMES;

    // We're using raw set functions, because we don't want to get back to EvalFrameEx.
    while (_PySet_NextEntry(asyncio_tasks, &pos, &wref, &hash)) {
        PyObject *task = PyWeakref_GET_OBJECT(wref);
        PyObject *coro = PyObject_GetAttrString(task, "_coro"); // TODO: hoist this up?
        PyErr_Clear();
        if (coro != NULL) {
            show_coro_stack(coro, &stacks);
            if (profile_debug) {
                printf("\n");
            }
        }
        Py_XDECREF(coro);
    }
    if (profile_debug) {
        printf("\n");
    }
    stacks->code = NULL;
    EvalState_load(&estate);
    Py_RETURN_NONE;
}

static void collect_atasks(SamplerThreadState *sts, NTicks time_now)
{
    sampler_atasks(NULL, NULL);
    FrameCopyAsync *frame = TEMP_ASYNC_FRAMES;

    NodeID node = 0;

    while (frame->code) {
        if (frame->code != async_frame_separator) {
            node = FrameNode_upsert(node, frame->code);
        } else {
            if (node) {
                FrameTime_upsert(node, time_now - profile_eprev, sts, -1);
            }
            node = 0;
        }
        frame++;
    }
}

static inline void periodic(PyThreadState *pts, SamplerThreadState *sts, uint64_t ticks) {
    // TODO: should we do this twice, or just once in EvalFrameEx?
    // if just once... is the current place more, or less correct?
    NTicks t0 = tick_time();

    if (__builtin_expect(should_sample(ticks) > 0, 0)) {
        for (SamplerThreadState *tts = STATE_HEAD; tts; tts = tts->snext) {
            collect_sample(tts, ticks, sts == tts);
        }
        profile_tprev = ticks;
    }
    if (__builtin_expect(should_emit(ticks) > 0, 0)) {
#if COROUTINES
        collect_atasks(sts, ticks);
#endif
        emit_samples(pts, ticks);
        for (SamplerThreadState *tts = STATE_HEAD; tts; tts = tts->snext) {
            tts->active = 0;
        }
        for (PyThreadState *qts = PyInterpreterState_ThreadHead(pts->interp); qts; qts = PyThreadState_Next(qts)) {
            PyObject *tdict = _PyThreadState_GetDict(qts);
            if (tdict == NULL) {
                continue;
            }
            PyObject *plong = PyDict_GetItem(tdict, sentinel);
            if (plong == NULL) {
                PyErr_Clear();
                continue;
            }
            SamplerThreadState *tts = (SamplerThreadState*) PyLong_AsVoidPtr(plong);
            tts->active = 1;
        }
        for (SamplerThreadState *ttn, *tts = STATE_HEAD; tts; tts = ttn) {
            ttn = tts->snext;
            if (tts->active == 0) {
                SamplerThreadState_free(tts);
            }
        }
        profile_eprev = ticks;
#if COROUTINES
        PyObject *ret = sampler_atasks(NULL, NULL);
#endif
    }
    NTicks t1 = tick_time();
    COUNTERS[0] += t1 - t0;
}

#pragma endregion

#pragma region sampler

PyObject *sampler_evalex(PyThreadState *tstate, PyFrame frame, int throwflag)
{
    PyFrame fprev = frame->previous = _PyThreadState_CurrentFrame(tstate);

    if (__builtin_expect(profile_start == 0, 0)) {
        return _PyEval_EvalFrameDefault(tstate, frame, throwflag);
    }

    // TODO: somewhere else?
    EvalState estate;

    if (profile_debug) {
        EvalState_save(&estate);

        add_frame_tag(frame, &DEFAULT_TAG);
        show_stack(frame, "EVX");
        printf("\n");

        uint64_t profile_saved = profile_start;
        profile_start = 0;
#if COROUTINES
        sampler_atasks(NULL, NULL);
#endif
        profile_start = profile_saved;

        EvalState_load(&estate);
    }

    uint64_t ticks0 = tick_time();
    PyObject *value = NULL;

    SamplerThreadState *sts = PyThread_tss_get(&THREAD_KEY);
    if (__builtin_expect(sts == NULL, 0)) {
        sts = SamplerThreadState_alloc(tstate, fprev, ticks0);
    }
    uint64_t ticks1 = tick_time();
    // periodic(tstate, sts, ticks1);
    uint64_t ticks2 = tick_time();
    stack_push(sts, frame, ticks2);
    value = _PyEval_EvalFrameDefault(tstate, frame, throwflag);

#if COROUTINES && (PY_MINOR_VERSION < 13)
    // Sneaky coroutine origin tracking
    if ((_Py_OPCODE(*frame->prev_instr) == RETURN_GENERATOR)
    &&  (frame->prev_instr == _PyCode_CODE(frame->f_code))
    &&  (frame->f_code->co_flags & (CO_COROUTINE | CO_ASYNC_GENERATOR)) {
        force_node_for_coro(sts);
    }
#endif

    if (profile_debug) {
        show_frame(frame, "EVY");
        printf("\n");
    }

    set_frame_tag(frame, 0x0000);
    uint64_t ticks3 = tick_time();
    periodic(tstate, sts, ticks3);
    stack_drop(sts);
    uint64_t ticks4 = tick_time();

    COUNTERS[1] += ticks1 - ticks0;
    COUNTERS[2] += ticks2 - ticks1;
    COUNTERS[3] += ticks3 - ticks2;
    COUNTERS[4] += ticks4 - ticks3;

    return value;
}

destructor orig_code_dealloc = NULL;

void hack_code_dealloc(PyObject *code_raw)
{
    if (_Py_IsFinalizing()) {
        orig_code_dealloc(code_raw);
        return;
    }
    PyCodeObject *code = (PyCodeObject*) code_raw;
    Py_ssize_t name_rc = code->co_name ? code->co_name->ob_refcnt : 0;
    Py_ssize_t file_rc = code->co_filename ? code->co_filename->ob_refcnt : 0;
    const Py_ssize_t weird_rc = 1e9;
    printf(
        "BOOM : %p : %p %3zd : %p %3zd : %d\n",
        code,
        code->co_filename,
        file_rc,
        code->co_name,
        name_rc < weird_rc ? name_rc : name_rc - weird_rc,
        name_rc > weird_rc
    );
    orig_code_dealloc(code_raw);
}

PyObject *sampler_inject(PyObject * module, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "O", &asyncio_tasks)) {
        return NULL;
    }

    Py_INCREF(asyncio_tasks);

    PyInterpreterState *istate = PyInterpreterState_Get();
    PyThreadState* tstate;
    int nthreads = 0;
    int maxdepth = 0;
    
    tstate = PyInterpreterState_ThreadHead(istate);
    while (tstate) {
        nthreads++;
        tstate = PyThreadState_Next(tstate);
    }

    if (nthreads > 1) {
        PyErr_SetString(PyExc_RuntimeError, "inject must happen with only one thread");
        return NULL;
    }

    tstate = PyInterpreterState_ThreadHead(istate);
    PyFrame frame = _PyThreadState_CurrentFrame(tstate);
    while (frame) {
        maxdepth++;
        frame = frame->previous;
    }

    if (maxdepth > 1) {
        PyErr_SetString(PyExc_RuntimeError, "inject must happen at the topmost level");
        return NULL;
    }

    orig_code_dealloc = PyCode_Type.tp_dealloc;
//  PyCode_Type.tp_dealloc = hack_code_dealloc;

    _PyInterpreterState_SetEvalFrameFunc(istate, sampler_evalex);
    profile_setup = tick_time();

	PyObject *r = PyLong_FromUnsignedLongLong(profile_setup);
	return r;
}


PyObject *sampler_enable(PyObject * Py_UNUSED(module), PyObject *args)
{
    if (!PyArg_ParseTuple(args, "p", &profile_debug)) {
        return NULL;
    }
    profile_start = tick_time();
    profile_tprev = profile_start;
    profile_eprev = profile_start;
	PyObject *r = PyLong_FromUnsignedLongLong(profile_start);
	return r;
}


PyObject *sampler_finish(PyObject * Py_UNUSED(module), PyObject * Py_UNUSED(args))
{
    flush_buffer();

    profile_start = 0;
    profile_close = tick_time();

//  PyThreadState *tstate = PyThreadState_GET();
//  _PyInterpreterState_SetEvalFrameFunc(tstate->interp, _PyEval_EvalFrameDefault);

	PyObject *r = PyList_New(COUNTER_MAX);
	for (int i = 0; i < COUNTER_MAX; i++) {
	    PyList_SET_ITEM(r, i, PyLong_FromUnsignedLongLong(COUNTERS[i]));
	}

    Py_XDECREF(asyncio_tasks); // TODO: multiple cycles

	return r;
}

PyMethodDef sampler_funcs[] = {
    { "inject", sampler_inject, METH_VARARGS, NULL, },
	{ "enable", sampler_enable, METH_VARARGS, NULL, },
	{ "finish", sampler_finish, METH_NOARGS,  NULL, },
	{ "atasks", sampler_atasks, METH_NOARGS,  NULL, },
    {},
};

PyModuleDef sampler_module = {
	PyModuleDef_HEAD_INIT,
	"_sampler",
	NULL,
	-1,
	sampler_funcs,
	NULL,
	NULL,
	NULL,
	NULL
};

void sampler_atfork(void) {
    // TODO: make sure we close cleanly, this will suffice for now:
    profile_setup = 0;
    profile_start = 0;
    profile_tprev = 0;
    profile_eprev = 0;
    profile_close = 0;
    profile_fileno = 0;
}

PyMODINIT_FUNC PyInit__sampler(void) {
    sampler_code_pending = PySet_New(NULL);
    sampler_code_written = PySet_New(NULL);
    sentinel = PyUnicode_InternFromString("qndp");
    PyThread_tss_create(&STATE_KEY);
    PyThread_tss_create(&THREAD_KEY);
    tick_init();
    profile_buffer = PyMem_RawMalloc(profile_buffer_limit);
    profile_fileno = open("./profile.qnd", O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, 00600);
    write_typed(uint32_t, 0x70646E71);
    write_typed(uint64_t, tick_to_ns_mul);
    write_typed(uint64_t, tick_to_ns_div);

    pthread_atfork(NULL, NULL, sampler_atfork);

    PyGetSetDef* getset = PyGen_Type.tp_getset;
    while (getset && getset->name) {
        if (strcmp(getset->name, "gi_yieldfrom") == 0) {
            _gen_getyieldfrom = getset->get;
            break;
        }
        getset++;
    }

#if SUPER_UNSAFE
    PyInterpreterState* istate = PyInterpreterState_Get();
    GIL = &istate->runtime->ceval.gil;
#endif
	return PyModule_Create(&sampler_module);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#pragma endregion
