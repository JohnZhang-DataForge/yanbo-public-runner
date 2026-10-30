# Bootstrap OS runtime

This is a single-main-thread host implementation, not a complete Switch OS.
It reserves guest virtual memory, maps the linked image, stack and TLS, manages
heap size and permissions, and provides memory queries, main-thread/process
handles and IDs, priorities, clocks, debug output, and exit handling.
Unsupported SVCs and CPU traps stop with the guest PC; they do not report success.

Run `python tests/check_os.py` for generated guest SVC execution and native
OS diagnostic build checks. After `build_lift.py`, run
`python build_os.py OUTPUT_DIRECTORY`, then
`OUTPUT_DIRECTORY/aoc-os-diagnostic OUTPUT_DIRECTORY/image.bin [entry-address]`.
New lifts generate `startup.json` and `hos_startup.h` from exported symbols and
module DT_INIT addresses. Without an explicit entry override, the diagnostic runs
an experimental host bootstrap sequence: libc0, OS initialization, libc1, SDK
service initialization, game startup, libc2, module initializers, then nnMain.
SDK service initialization is not skipped. Every stage must return normally
before the next can run; an unsupported service, trap, exit or missing lifted
function stops the sequence. This order is adapted from the supplied Skybound
bootstrap as a hypothesis for this SDK; full retail initialization is not verified.
Builds without a manifest require an explicit entry address.

The previous default 0x08000000 was module metadata, not an executable entry.
The image-derived startup exports avoid that erroneous diagnostic jump.
Twenty unit tests and synthetic guest tests cover symbol validation, explicit
entry execution, startup sequencing and stopping before nnMain on an early failure.
No successful game boot or gameplay is claimed.

Thread creation/scheduling, synchronization, IPC and service sessions, filesystem,
GPU, audio, and input remain unimplemented. Image pages currently use a coarse
read/write mapping instead of per-segment permissions. The heap uses a bounded
1 GiB diagnostic profile. Firmware and production keys are not loaded or needed
for these tests. Do not commit keys, firmware, game images, or generated game code.

## Private base-game startup check

A bounded 348-function diagnostic slice compiled and ran locally with the exact
base-game image. It returned from `_init_libc0`, entered `nnosInitialize`, and
executed QueryMemory, GetInfo and GetThreadId service paths. It then stopped at
unsupported SVC 0x1d at guest PC 0x0b2fca30. This identifies a synchronization
service barrier. The slice is a startup experiment, not a new full-game build;
indirect targets were added explicitly as diagnostics exposed them. It does not
establish that later startup stages work. Game-derived output stays private.

Windows source/synthetic CI passed:
https://github.com/JohnZhang-DataForge/yanbo-public-runner/actions/runs/40984726004
