#define PY_VERSION_RANGE(A, B, C) ((A == PY_MAJOR_VERSION) && (B <= PY_MINOR_VERSION) && (PY_MINOR_VERSION <= C))
#define PY_VERSION_EXACT(A, B)    ((A == PY_MAJOR_VERSION) && (B == PY_MINOR_VERSION))

#if PY_VERSION_RANGE(3, 11, 12)
static inline PyCodeObject *
_PyFrame_GetCode(_PyInterpreterFrame *f)
{
    return f->f_code;
}
static inline _PyInterpreterFrame *
_PyThreadState_CurrentFrame(PyThreadState *tstate)
{
    return tstate->cframe->current_frame;
}
#endif
#if PY_VERSION_RANGE(3, 13, 14)
static inline _PyInterpreterFrame *
_PyThreadState_CurrentFrame(PyThreadState *tstate)
{
    return tstate->current_frame;
}
static inline int
_Py_IsFinalizing(void)
{
    return Py_IsFinalizing();
}
#endif

#if PY_VERSION_RANGE(3, 11, 13)
static inline _PyInterpreterFrame *
_PyGen_GetFrame(PyGenObject *gen)
{
    return (_PyInterpreterFrame *) gen->gi_iframe;
}
#endif
#if PY_VERSION_EXACT(3, 14)
static inline _PyInterpreterFrame *
_PyGen_GetFrame(PyGenObject *gen)
{
    return &gen->gi_iframe;
}
#endif

#if PY_VERSION_EXACT(3, 11)
static inline PyCodeObject *
_PyGen_GetCode(PyGenObject *gen)
{
    return gen->gi_code;
}
#endif
#if PY_VERSION_RANGE(3, 12, 14)
static inline PyCodeObject *
_PyGen_GetCode(PyGenObject *gen)
{
    return _PyFrame_GetCode(_PyGen_GetFrame(gen));
}
#endif

#if PY_VERSION_RANGE(3, 11, 13)
#define PYFRAME_SPACE_OFFSET (offsetof(_PyInterpreterFrame, owner) + 1)
#define PYFRAME_FINAL_OFFSET (offsetof(_PyInterpreterFrame, localsplus))
#endif
#if PY_VERSION_EXACT(3, 14)
#define PYFRAME_SPACE_OFFSET (offsetof(_PyInterpreterFrame, visited) + 1)
#define PYFRAME_FINAL_OFFSET (offsetof(_PyInterpreterFrame, localsplus))
#endif

#if PY_VERSION_EXACT(3, 11)
typedef uint16_t FrameTag;
#endif
#if PY_VERSION_RANGE(3, 12, 13)
typedef uint8_t  FrameTag;
#endif
#if PY_VERSION_EXACT(3, 14)
typedef uint32_t FrameTag;
#endif

#define PYFRAME_SPACE_LENGTH (PYFRAME_FINAL_OFFSET - PYFRAME_SPACE_OFFSET)
static_assert(PYFRAME_SPACE_LENGTH >= sizeof(FrameTag), "ENOSPC");

FrameTag DEFAULT_TAG = 0xD0;
FrameTag ASYNCIO_TAG = 0xE0;

// From genobject.c:
// gen_getyieldfrom
// PyAsyncGenASend
// PyAsyncGenAThrow

static PyCFunction _gen_getyieldfrom = NULL;

typedef enum {
    AWAITABLE_STATE_INIT,
    AWAITABLE_STATE_ITER,
    AWAITABLE_STATE_CLOSED,
} AwaitableState;

typedef struct {
    PyObject_HEAD
    PyAsyncGenObject *agx_gen;
    PyObject *argx_sendval_or_args;
    AwaitableState agx_state;
} PyAsyncGenASendOrThrow;


typedef struct EvalState {
    PyObject *exc_type;
    PyObject *exc_value;
    PyObject *exc_trace;
    int gc_enabled;
    unsigned long switch_interval;
} EvalState;

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

static inline FrameTag set_frame_tag(_PyInterpreterFrame *frame, FrameTag tag)
{
    if (frame) {
        FrameTag *ptr = (void*) frame + PYFRAME_SPACE_OFFSET;
        FrameTag prev = *ptr;
        *ptr = tag;
        return prev;
    } else {
        return -1;
    }
}

static inline FrameTag get_frame_tag(_PyInterpreterFrame *frame)
{
    if (frame) {
        FrameTag *ptr = (void*) frame + PYFRAME_SPACE_OFFSET;
        return *ptr;
    } else {
        return -1;
    }
}

static inline FrameTag add_frame_tag(_PyInterpreterFrame *frame, FrameTag *source)
{
    FrameTag tag = get_frame_tag(frame);
    if (tag) {
        return tag;
    } else {
        tag = (*source)++;
        set_frame_tag(frame, tag);
        return tag;
    }
}

void show_frame(_PyInterpreterFrame *frame, char *prefix)
{
    PyCode code = _PyFrame_GetCode(frame);
    Py_ssize_t size;
    const char *name = PyUnicode_AsUTF8AndSize(code->co_qualname, &size);
    FrameTag tag = get_frame_tag(frame);
    int addr = _PyInterpreterFrame_LASTI(frame) * sizeof(_Py_CODEUNIT);
    int line = PyCode_Addr2Line(code, addr);
    printf(
        "%s : F%p %04x : %d : C%p:%08x : %s:%04d\n",
        prefix,
        frame,
        tag,
        frame->owner,
        code,
        addr,
        name,
        line
    );
}

void show_stack(_PyInterpreterFrame *frame, char *prefix)
{
    while (frame) {
        show_frame(frame, prefix);
        frame = frame->previous;
    }
}

int coro_type_check(PyObject *coro)
{
    int c0 = PyGen_Check(coro);
    int c1 = PyGen_CheckExact(coro);
    int c2 = PyAsyncGen_CheckExact(coro);
    int c3 = PyCoro_CheckExact(coro);
    return (
        c0 * 0x1000 +
        c1 * 0x0100 +
        c2 * 0x0010 +
        c3 * 0x0001
    );
}

void show_coro(PyObject *coro, int type, char* prefix)
{
    if (type) {
        PyGenObject *cgen = (PyGenObject*) coro;
        const char *gname = PyUnicode_AsUTF8(cgen->gi_qualname);
        const PyCode code = _PyGen_GetCode(cgen);
        printf("%s : G%p %04x :         : C%p          : %s\n", prefix, coro, type, code, gname);
    } else if (Py_IS_TYPE(coro, &_PyAsyncGenASend_Type)) {
        printf("%s : #%p %04d\n", prefix, coro, type);
    } else if (Py_IS_TYPE(coro, &_PyAsyncGenAThrow_Type)) {
        printf("%s : #%p %04d\n", prefix, coro, type);
    } else if (coro != Py_None) {
        printf("%s :  %p %04d\n", prefix, coro, type);
    }
}

typedef enum {
    STATE_PENDING,
    STATE_CANCELLED,
    STATE_FINISHED
} fut_state;

#define FutureObj_HEAD(prefix)                                              \
    PyObject_HEAD                                                           \
    PyObject *prefix##_loop;                                                \
    PyObject *prefix##_callback0;                                           \
    PyObject *prefix##_context0;                                            \
    PyObject *prefix##_callbacks;                                           \
    PyObject *prefix##_exception;                                           \
    PyObject *prefix##_exception_tb;                                        \
    PyObject *prefix##_result;                                              \
    PyObject *prefix##_source_tb;                                           \
    PyObject *prefix##_cancel_msg;                                          \
    fut_state prefix##_state;                                               \
    int prefix##_log_tb;                                                    \
    int prefix##_blocking;                                                  \
    PyObject *dict;                                                         \
    PyObject *prefix##_weakreflist;                                         \
    PyObject *prefix##_cancelled_exc;

typedef struct {
    FutureObj_HEAD(task)
    PyObject *task_fut_waiter;
    PyObject *task_coro;
    PyObject *task_name;
    PyObject *task_context;
    int task_must_cancel;
    int task_log_destroy_pending;
    int task_num_cancels_requested;
} TaskObj;

typedef struct {
    FutureObj_HEAD(fut)
} FutureObj;

typedef struct {
    PyObject_HEAD
    FutureObj *future;
} futureiterobject;

void show_coro_stack(PyObject *coro, FrameCopyAsync **stacks)
{
    FrameCopyAsync *stack = *stacks;
    int c0 = coro_type_check(coro);
    if (c0 == 0) {
        return;
    }

    PyObject *next = coro;
    int c1 = c0;
    while (1) {
        c1 = coro_type_check(next);
        if (profile_debug) {
        // show_coro(next, c1, "AIC");
        }
        if (c1) {
            __auto_type frame = _PyGen_GetFrame((PyGenObject *)next);
            stack->func = next;
            stack->code = _PyFrame_GetCode(frame);
            stack++;
//          add_frame_tag(frame, &ASYNCIO_TAG);
            if (profile_debug) {
                show_frame(frame, "AIO");
            }
            next = _gen_getyieldfrom(next, NULL);
        } else if (Py_IsNone(next)) {
            break;
        } else if (Py_IS_TYPE(next, &_PyAsyncGenASend_Type)) {
            next = (PyObject*) ((PyAsyncGenASendOrThrow *) next)->agx_gen;
        } else if (Py_IS_TYPE(next, &_PyAsyncGenAThrow_Type)) {
            next = (PyObject*) ((PyAsyncGenASendOrThrow *) next)->agx_gen;
        } else {
            // At this point, "next" is not a coroutine, options:
            // 1. A Python object with a custom __await__ method. (No stack frame.)
            // 2. Something in C with a .tp_as_async slot filled. (No code object.)
            //    In asyncio, usually this would be a FutureIter.
            printf("%p %p %s\n", next, ((futureiterobject*) next)->future, next->ob_type->tp_name);
            // context from thread state?
//          __builtin_trap();
            break;
        }
    }
    stack->code = async_frame_separator;
    stack++;
    *stacks = stack;
}
