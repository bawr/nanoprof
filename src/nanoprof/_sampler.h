#ifndef SAMPLER_H
#define SAMPLER_H

#pragma region headers

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <Python.h>
#include <opcode.h>

#define PY_VERSION_RANGE(A, B, C) ((A == PY_MAJOR_VERSION) && (B <= PY_MINOR_VERSION) && (PY_MINOR_VERSION <= C))
#define PY_VERSION_EXACT(A, B)    ((A == PY_MAJOR_VERSION) && (B == PY_MINOR_VERSION))

#define Py_BUILD_CORE
#include <internal/pycore_context.h>
#include <internal/pycore_frame.h>
#if PY_VERSION_RANGE(3, 13, 14)
#include <internal/pycore_ceval.h>
#include <internal/pycore_interp.h>
#include <internal/pycore_pystate.h>
#include <internal/pycore_setobject.h>
#endif
#if PY_VERSION_EXACT(3, 14)
#include <internal/pycore_genobject.h>
#include <internal/pycore_interpframe.h>
#include <internal/pycore_interpframe_structs.h>
#include <internal/pycore_runtime.h>
#endif
#undef Py_BUILD_CORE

#pragma endregion

#pragma region version

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

static PyCFunction _PyGen_yf_or_none = NULL;

#pragma endregion

#pragma region defines

#define COUNTER_MAX 7

#define COROUTINES 0

#define STACKS_MAX 1024
#define FRAMES_MAX 1024 * 1024
#define TIMERS_MAX 1024 * 1024

#pragma endregion

#pragma region globals

static uint64_t COUNTERS[COUNTER_MAX];

static Py_tss_t STATE_KEY = Py_tss_NEEDS_INIT;
static Py_tss_t THREAD_KEY = Py_tss_NEEDS_INIT;

int profile_debug = 0;
uint64_t profile_setup = 0;
uint64_t profile_start = 0;
uint64_t profile_tprev = 0;
uint64_t profile_eprev = 0;
uint64_t profile_close = 0;

uint64_t buffer_period_ns = 1e9;
uint64_t sample_period_ns = 1e6;
uint64_t stack_minimum_ns = 1e6;

PyObject* sentinel = NULL;

int profile_fileno = -1;
int depth = 0;

void* profile_buffer = NULL;
size_t profile_buffer_dirty = 0;
size_t profile_buffer_limit = 16 * 1024 * 1024;

#pragma endregion

#pragma region types

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
    pthread_t pthread_id;
    uint64_t native_thread_id;
    uint64_t active;
    uint64_t stack_depth;
    uint64_t time_user;
    uint64_t time_sys;
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

typedef struct EvalState {
    PyObject *exc_type;
    PyObject *exc_value;
    PyObject *exc_trace;
    int gc_enabled;
    unsigned long switch_interval;
} EvalState;

#pragma endregion

#endif
