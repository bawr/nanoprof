/*

_PyRuntimeState *runtime = pts->interp->runtime;
PyThread_acquire_lock((runtime)->interpreters.mutex, WAIT_LOCK); // HEAD_LOCK

_PyFrame_IsIncomplete();
Py_AddPendingCall();
_PyFrame_Clear();
_PyThreadState_PopFrame();

PyInterpreterState* istate = PyInterpreterState_Get();
static struct _gil_runtime_state *GIL = &istate->runtime->ceval.gil;
*/
