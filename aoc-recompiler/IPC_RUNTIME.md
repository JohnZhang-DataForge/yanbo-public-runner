# Bootstrap IPC and logging

The runtime now handles ConnectToNamedPort (SVC 0x1f), SendSyncRequest (0x21),
and CloseHandle for IPC sessions. `sm:` is the supported named port. Unknown
ports return Kernel NotFound. Ports are read with a 12-byte bound and checked
guest permissions. A 64-slot session table uses monotonically allocated handle
values so a closed handle cannot silently refer to a later session.

The supported transport is HIPC/CMIF over the main thread's 128-byte TLS message
area. Request/RequestWithContext and Control/ControlWithContext headers are
parsed with declared-size bounds. PID placeholders are not trusted as host
process identities. Input handle transfer, receive/exchange buffers, receive
lists, and arbitrary descriptor combinations remain unsupported.

Implemented commands:

- SM RegisterClient with a PID descriptor, and GetService for `lm`.
- QueryPointerBufferSize returns zero (no server pointer buffer).
- ConvertCurrentObjectToDomain, bounded domain object allocation/lookup/close.
- `lm` OpenLogger, returning a moved handle or domain object as appropriate.
- Logger Log with one send-static and/or one send-buffer descriptor in the
  auto-select layout (only one may carry a nonempty packet).
- Logger SetDestination; destination zero disables output, other destinations
  route to the host diagnostic sink.

The logging sink prints bounded raw packets as hexadecimal to stderr. This
provides a working host sink rather than Nintendo target-manager/UART routing.
Packets are limited to 4096 bytes and checked against readable guest memory.
It does not yet decode the log packet's text chunks. Domain sessions support
up to 15 object IDs over their lifetime. Unsupported services/commands stop
with a diagnostic; they are not given successful dummy sessions.

`python tests/check_os.py` executes generated ARM64 SVC wrappers and native
checks covering registration, malformed frames, PID requirements, CMIF replies,
context tokens, moved handles, handle exhaustion/reuse, unknown services,
logger packets, domain object creation/dispatch/close, and invalid memory.
The existing 20 Python unit tests also pass locally.

Behavior references (consulted, not copied):

- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h
- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/cmif.h
- https://github.com/switchbrew/libnx/blob/master/nx/source/services/sm.c
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/source/lm/sf/lm_i_log_service.hpp

This is still a partial runtime, not a successful retail game boot.
