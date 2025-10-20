/*

_PyRuntimeState *runtime = pts->interp->runtime;
PyThread_acquire_lock((runtime)->interpreters.mutex, WAIT_LOCK); // HEAD_LOCK

_PyFrame_IsIncomplete();
Py_AddPendingCall();
_PyFrame_Clear
_PyThreadState_PopFrame

*/
