# Bootstrap IPC and logging

The runtime now handles ConnectToNamedPort (SVC 0x1f), SendSyncRequest (0x21),
and CloseHandle for IPC sessions. `sm:` is the supported named port. Unknown
ports return Kernel NotFound. Ports are read with a 12-byte bound and checked
guest permissions. A 64-slot session table uses monotonically allocated handle
values so a closed handle cannot silently refer to a later session.

The supported transport is HIPC/CMIF over the main thread's 128-byte TLS message
area. Request/RequestWithContext and Control/ControlWithContext headers are
parsed with declared-size bounds. PID placeholders are not trusted as host
process identities. Input handle transfer beyond the validated process-handle copy used by
OpenApplicationProxy, receive/exchange buffers, receive
lists, and arbitrary descriptor combinations remain unsupported.

Implemented commands:

- SM RegisterClient with a PID descriptor, and GetService for `lm`, `apm`, and `appletOE`.
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

## Verified startup result

Windows source/synthetic tests and packaging passed:
https://github.com/JohnZhang-DataForge/yanbo-public-runner/actions/runs/40986741877

A private 871-function startup slice completes OS/libc initialization, connects
to and registers with `sm:`, obtains `lm`, queries its pointer-buffer size,
converts it to a domain and opens a logger. The next explicit stop is SM
GetService `apm` (power management), at SVC 0x21 / guest PC 0x0b2fca50.
The IPC transport and logging barriers are passed. `apm` and other remaining
service backends are not implemented by this change. This bounded test does
not establish full-game execution correctness, boot or gameplay.

## APM performance service

The `apm` manager supports OpenSession (command 0) and GetPerformanceMode
(command 1). The native diagnostic profile reports Normal (0). Its sessions
support SetPerformanceConfiguration (0) and GetPerformanceConfiguration (1)
for Normal (0) and Boost (1), including domain-object forms of these calls.

Configuration IDs are retained as opaque guest-selected values, independently
for the two modes and shared across this process's sessions. No Windows clock
or power setting is changed, and these values are not a model of Switch clock
frequencies. Reading a mode before the guest selects a configuration stops
explicitly rather than inventing a default ID. Invalid mode values return a
CMIF error; malformed payloads are rejected without modifying configuration.
Other APM commands remain unsupported.

Native tests cover mode reporting, moved sessions, configuration readback and
isolation, independent manager/session lifetimes, sharing across sessions,
domain calls, malformed input, invalid modes, and configured zero values.
The private 873-function startup diagnostic now obtains `apm` and proceeds to
SM GetService `appletOE`, the next unimplemented service. This private run does
not exercise every performance operation; those are checked synthetically.

APM protocol reference (consulted, not copied):
https://github.com/switchbrew/libnx/blob/master/nx/source/services/apm.c
https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/apm.h

Applet proxy/state/message-event support is described in
[APPLET_RUNTIME.md](APPLET_RUNTIME.md). Historical stops above describe prior
startup checkpoints, not the current service registry.
