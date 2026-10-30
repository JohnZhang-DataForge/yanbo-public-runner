# Applet startup runtime

`appletOE` is now available through the service manager. OpenApplicationProxy
requires the PID descriptor, one copied valid current-process handle and the
reserved input value. The input process handle remains valid; it is not moved.
The returned proxy and its child interfaces work as moved sessions or domain
objects and use the existing bounded handle/object lifetime rules.

The headless startup profile describes one foreground application: Handheld
operation mode, Normal performance mode, InFocus state and resource user ID 1.
It does not create a Windows window or establish game boot/playability.

Implemented child behavior:

- CommonStateGetter supplies a copied message-event handle, receives queued
  messages, and reports operation/performance/focus state.
- SelfController retains exit-lock, screenshot policy, mode-change notification,
  focus-handling, restart-message and out-of-focus suspension settings. It
  reports zero suspended ticks and supplies a separate suspended-tick event.
- WindowController reports the resource user ID and records foreground rights.
- ApplicationFunctions records NotifyRunning, reports the running state and
  supplies a separate GPU-error notification event.
- Proxy getters create typed library-applet, audio, display and debug objects.
  Their operation methods remain unsupported and stop explicitly. A returned
  interface is not a working library-applet, audio or display backend.

The message event has shared backing state across copied handles. Closing an
IPC session or one event handle does not invalidate another copied event handle.
The queue is bounded to 16 messages and preserves order; ReceiveMessage on an
empty queue returns AM NoMessages (0x680). The serialized host-thread API
`hos_applet_post_message` posts a message and signals the event. It returns zero
when full. The current runtime and this injection API are not thread-safe.
`hos_applet_signal_gpu_error` is a separate serialized host hook for GPU-error
notifications; it does not implement rendering or GPU error recovery.

ClearEvent and ResetSignal support these events. WaitSynchronization validates
all handles before returning the first signaled index; zero-timeout polling
returns TimedOut when nothing is ready. Zero-handle polling is also supported.
An unsignaled wait with a nonzero timeout still stops explicitly, because the
runtime has no guest scheduler or asynchronous host event loop. Event handles
cannot be used as IPC endpoints.

Native tests execute generated SVC wrappers and check proxy PID/handle
validation, domain routing, state queries, policy argument checks, 64-bit replies,
shared-event clear/reset behavior, signaled/polling waits, queue order/capacity,
empty-message errors, stale handles and independent copied-handle lifetimes.
They also check that unimplemented child methods and blocking waits stop rather
than report success.

Protocol references (consulted, not copied):

- https://github.com/switchbrew/libnx/blob/master/nx/source/services/applet.c
- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/applet.h
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_synchronization.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/kern_k_readable_event.cpp
