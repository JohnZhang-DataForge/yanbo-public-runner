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
The entry address is diagnostic input; the default is the image base and does
not establish that the retail startup contract has been reconstructed.

Linux synthetic tests pass. The full base-game output compiles and links with
this layer. Running the default entry stops immediately at PC 0x08000000 with
an explicit branch-to-non-function trap, before any SVC. Retail entry discovery
and startup initialization still require work. This is not a successful boot.

Thread creation/scheduling, synchronization, IPC and service sessions, filesystem,
GPU, audio, and input remain unimplemented. Image pages currently use a coarse
read/write mapping instead of per-segment permissions. The heap uses a bounded
1 GiB diagnostic profile. Firmware and production keys are not loaded or needed
for these tests. Do not commit keys, firmware, game images, or generated game code.
