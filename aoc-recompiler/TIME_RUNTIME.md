# Read-only clock service

`time:u` supplies user/local clocks, a steady clock, a UTC timezone object, and
an explicitly uninitialized network clock. Local clock time is sampled from
host UTC once, then advances with the host monotonic counter. There is no
network synchronization, automatic correction, host timezone change, or host
clock mutation. Unsupported setters and timezone-rule buffer operations stop.

The CPU CNTPCT hook and SVC GetSystemTick now use the same 19.2 MHz host counter
and epoch. Previously the default CPU hook used a process-relative epoch while
the kernel used host monotonic uptime, which was incompatible with shared
clock context calculations.

The time manager supports domain child factories and a copied native shared
memory handle. The one-page layout contains sequence-protected steady/local
contexts and an uninitialized network context, following libnx's reader ABI.
Both copies of each immutable context agree; time advances through the counter.
The network source ID remains zero and differs from the steady source ID, so
clients detect that network time has not been initialized.

MapSharedMemory (0x13) maps this object read-only in the guest alias region.
UnmapSharedMemory (0x14) removes an exact one-page view. Closing a public handle
does not invalidate an existing view; another copied handle can unmap the same
service-owned clock object. This is a dedicated immutable clock-page profile,
not a general writable shared-memory or GPU/DMA alias implementation.

UTC calendar conversion is available through the timezone object's own rule.
Other location/rule loading and time-setting operations remain unsupported.
Native tests cover clock/counter agreement, UTC values, network failure,
domain factories, calendar conversion, shared layout, permissions, handle/view
lifetime and exact unmapping.

Primary ABI references consulted (not copied):

- https://github.com/switchbrew/libnx/blob/master/nx/source/services/time.c
- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/time.h
