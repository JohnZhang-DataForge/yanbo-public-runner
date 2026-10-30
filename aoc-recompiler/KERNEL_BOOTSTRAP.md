# Cooperative kernel bootstrap

The runtime implements stack mapping, thread creation/start/exit, thread IDs and
priorities, single-core affinity, sleeping/yielding, object waits, cancellation,
mutex arbitration and process-wide condition variables. This is a CPU bootstrap
profile, not a complete kernel or successful game boot.

MapMemory transfers ownership from a writable source to a stack-region view.
The source becomes inaccessible and reports the borrowed attribute. Exact
UnmapMemory copies modifications back, restores source permissions and releases
the view. Locked sources cannot be reprotected or removed by heap shrink.
There are 64 mappings. Sources must be uniform writable normal memory or the
loader's coarse writable image mapping. Partial unmaps, simultaneous physical
aliases, DMA and shared-memory mappings are not implemented. Only one guest
view owns the bytes at a time; this is not a physical alias implementation.

Guest threads use Windows fibers or POSIX ucontext on one host thread, with
separate registers, TLS pages and 8 MiB native C stacks. TLS has ThreadLocal
memory state and the kernel handle at offset 0x110. The main thread plus 63
additional thread objects are supported, subject to the shared handle limit.
Guest stacks remain caller-owned. Closing a handle does not destroy a running
or referenced thread. Terminated and never-started closed threads are reclaimed.
Child faults propagate to the main diagnostic rather than disappearing.

Scheduling is cooperative at waits, starts, synchronization and sleep/yield
calls. It is not preemptive or parallel; the logical core is 0. Selection uses
priority and round-robin ties. Mutex waiters donate priority through owner
chains. Object waits capture references, validate every handle, return the first
signaled index and support finite timeouts. Cancellation interrupts object waits
or records a one-shot cancellation for the next blocking object wait. Ready
objects and zero-timeout polling take precedence over a pending cancellation.
An indefinite wait without a runnable producer or host event source stops with
a diagnostic instead of being silently completed or hanging forever.

Mutex/condition-variable queues retain ownership tags, wait bits and FIFO ties.
Condition waits atomically release the mutex and enqueue before switching.
Signals wake a bounded count or all, acquire a free mutex or move waiters onto
its queue, and maintain the guest key flag. Timeouts return without falsely
reacquiring the mutex. Waiter references protect object lifetimes.

Run `python tests/check_os.py` for generated-guest semantic tests: mapping
copy-back/protection/limits, register/TLS isolation, yield/join/timeouts,
cancellation, handle lifetimes, child faults, lock handoff, priority donation,
CV wake counts/FIFO/key flags and timeout/relock behavior. Existing OS/IPC/APM/
applet checks also run. Public CI uses source and synthetic instructions only.

Protocol references (consulted, not copied):

- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_memory.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/kern_k_page_table_base.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_thread.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/kern_k_condition_variable.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/kern_k_synchronization_object.cpp
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/svc/arch/arm64/svc_thread_local_region.hpp
