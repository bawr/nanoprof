#include "_sampler.h"
#include "_sampler_time.h"

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


void EvalState_save(EvalState *estate)
{
    PyErr_Fetch(&estate->exc_type, &estate->exc_value, &estate->exc_trace);
    estate->gc_enabled = PyGC_Disable();
    estate->switch_interval = _PyEval_GetSwitchInterval();
    _PyEval_SetSwitchInterval(10e6);
}

void EvalState_load(EvalState *estate)
{
    PyErr_Restore(estate->exc_type, estate->exc_value, estate->exc_trace);
    if (estate->gc_enabled) {
        PyGC_Enable();
    }
    _PyEval_SetSwitchInterval(estate->switch_interval);
}

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
    write_typed(uint16_t, 0x7777);
    write_typed(uint64_t, pstr);
    write_typed(uint16_t, size);
    write_buffer(text, size);
}

void write_string_maybe(PyObject *pstr)
{
    int text_new = 0;
    khint_t text_pos = pmap_put(written_text, (uint64_t) pstr, &text_new);
    if (text_new) {
        kh_val(written_text, text_pos) = kh_size(written_text);
        write_string(pstr);
    }
}


void write_code(PyCode code)
{
    write_string_maybe(code->co_filename);
    write_string_maybe(code->co_qualname);
    write_typed(uint16_t, 0xDEC0);
    write_typed(uint64_t, code);
    write_typed(uint64_t, code->co_filename);
    write_typed(uint64_t, code->co_qualname);
    write_typed(uint16_t, code->co_firstlineno);
}

void write_node(NodeID node_id)
{
    FrameNode node = FRAMES[node_id];
    int code_new = 0;
    khint_t code_pos = pmap_put(written_code, (uint64_t) node.code_object, &code_new);
    if (code_new) {
        kh_val(written_code, code_pos) = node_id;
        write_code(node.code_object);
    }
    write_typed(uint16_t, 0xDED0);
    write_typed(uint64_t, node.code_object);
    write_typed(NodeID, node_id);
    write_typed(NodeID, node.node_caller);
}

void write_mark(NTicks time)
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
    state->stack[state->stack_depth] = (FrameCopy) {};
    Py_DECREF(code);
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
/*
    if ((tdict = _PyThreadState_GetDict(tstate)) == NULL) {
        goto fail;
    }
    if ((plong = PyLong_FromVoidPtr(state)) == NULL) {
        goto fail;
    }
    if (PyDict_SetItem(tdict, sentinel, plong) != 0) {
        goto fail;
    }
*/
    if (STATE_HEAD == NULL) {
        STATE_HEAD = state;
        STATE_TAIL = state;
    } else {
        state->sprev = STATE_TAIL;
        STATE_TAIL->snext = state;
        STATE_TAIL = state;
    }
    state->active = 1;
    state->pthread_id = (pthread_t) tstate->thread_id;
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

#ifdef __APPLE__
#include <libproc.h>

void SamplerThreadState_times(SamplerThreadState *sts)
{
    struct proc_threadinfo pth = {};
    if (proc_pidinfo(getpid(), PROC_PIDTHREADID64INFO, sts->native_thread_id, &pth, PROC_PIDTHREADINFO_SIZE)) {
        sts->time_user = pth.pth_user_time;
        sts->time_sys = pth.pth_system_time;
    }
}
#else
void SamplerThreadState_times(SamplerThreadState *sts)
{
    clockid_t tclk;
    if (pthread_getcpuclockid(sts->pthread_id, &tclk)) {
        sts->time_user = clock_gettime_nsec_np(tclk);
    }
}
#endif

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
    return ticks > profile_tprev + sample_period_ns;
}

static NodeID force_node_for_coro(SamplerThreadState *sts)
{
    NodeID node = 0;
    FrameCopy *frame;
    for (unsigned int i = 0; i < sts->stack_depth; i++) {
        frame = &sts->stack[i];
        node = frame->node = FrameNode_upsert(node, frame->code);
    }
    return node;
}

static void collect_sample(SamplerThreadState *sts, NTicks time_now, int is_active) {
    NodeID node = 0;
    FrameCopy *frame;
    for (unsigned int i = 0; i < sts->stack_depth; i++) {
        frame = &sts->stack[i];
        // We only entree stacks up to min_period, the idea is to avoid tree churn.
        if ((frame->time < time_now) && (time_now - frame->time > stack_minimum_ns)) {
            if (frame->node) {
                node = frame->node;
            } else {
                node = frame->node = FrameNode_upsert(node, frame->code);
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
    return ticks > profile_eprev + buffer_period_ns;
}

static void emit_thread(SamplerThreadState *sts) {
    write_typed(uint16_t, 0x2222);
    write_typed(uint64_t, sts->native_thread_id);
    write_typed(uint64_t, sts->active);
    write_typed(uint64_t, sts->time_user);
    write_typed(uint64_t, sts->time_sys);
}

static void emit_samples(PyThreadState *pts, NTicks time_now) {
    // TODO: compact the tree?
    for (unsigned int i = 1; i < NEXT_FREE_NODE; i++) {
        if (LAST_SEEN_NODE < i) {
            LAST_SEEN_NODE = i;
            write_node(i);
        }
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

    write_mark(time_now);
    flush_buffer();

//  PyThread_release_lock((runtime)->interpreters.mutex);
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
        for (SamplerThreadState *tts = STATE_HEAD; tts; tts = tts->snext) {
            tts->active = 0;
            for (PyThreadState *qts = PyInterpreterState_ThreadHead(pts->interp); qts; qts = PyThreadState_Next(qts)) {
                if (tts->native_thread_id == qts->native_thread_id) {
                    tts->active = 1;
                    SamplerThreadState_times(tts);
                    break;
                }
            }
            emit_thread(tts);
        }

        emit_samples(pts, ticks);

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

/*
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
*/
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
    &&  (frame->f_code->co_flags & (CO_COROUTINE | CO_ASYNC_GENERATOR))) {
        force_node_for_coro(sts);
    }
#endif

/*
    if (profile_debug) {
        show_frame(frame, "EVY");
        printf("\n");
    }

    set_frame_tag(frame, 0x0000);
*/

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
PyCFunction orig_set_result = NULL;

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
    double arg1 = sample_period_ns / 1e9;
    double arg2 = stack_minimum_ns / 1e9;
    if (!PyArg_ParseTuple(args, "p|dd", &profile_debug, &arg1, &arg2)) {
        return NULL;
    }
    stack_minimum_ns = arg1 * 1e9;
    sample_period_ns = arg2 * 1e9;
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

PyObject *sampler_cstack(PyObject * Py_UNUSED(module), PyObject * Py_UNUSED(args))
{
    PyThreadState *tstate = PyThreadState_GET();
    PyObject *list = PyList_New(0);
    PyObject *ctx = tstate->context;
    while (ctx) {
        PyList_Append(list, ctx);
        ctx = (PyObject*) ((PyContext*) ctx)->ctx_prev;
    }
    PyList_Reverse(list);
    return list;
}

PyMethodDef sampler_funcs[] = {
    { "inject", sampler_inject, METH_VARARGS, NULL, },
	{ "enable", sampler_enable, METH_VARARGS, NULL, },
	{ "finish", sampler_finish, METH_NOARGS,  NULL, },
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
    sentinel = PyUnicode_InternFromString("qndp");
    PyThread_tss_create(&STATE_KEY);
    PyThread_tss_create(&THREAD_KEY);
    tick_init();
    profile_buffer = PyMem_RawMalloc(profile_buffer_limit);
    profile_fileno = open("/tmp/profile.qnd", O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, 00600);
    write_typed(uint32_t, 0x70646E71);
    write_typed(uint64_t, tick_to_ns_mul);
    write_typed(uint64_t, tick_to_ns_div);

    pthread_atfork(NULL, NULL, sampler_atfork);

    PyGetSetDef* getset = PyGen_Type.tp_getset;
    while (getset && getset->name) {
        if (strcmp(getset->name, "gi_yieldfrom") == 0) {
            _PyGen_yf_or_none = (PyCFunction) getset->get;
            break;
        }
        getset++;
    }

//  PyTypeObject* asyncio_future_type = (PyTypeObject*) PyObject_GetAttrString(PyImport_ImportModule("_asyncio"), "Future");

    written_code = pmap_init();
    written_text = pmap_init();
    pmap_resize(written_code, 0xFFFF);
    pmap_resize(written_text, 0xFFFF);

	return PyModule_Create(&sampler_module);
}

#pragma endregion

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
