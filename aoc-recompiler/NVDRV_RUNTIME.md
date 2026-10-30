# nvdrv startup transport

The runtime registers `nvdrv` and its `:a`, `:s`, `:t` aliases as typed service
sessions. Initialize (3) validates a process handle, a copied None-permission
transfer object and its requested size. The driver retains the object after the
application closes its handle. The kernel slot cannot be reused while pinned;
RW ownership returns only after the last application/driver reference closes.

HIPC CloneCurrentObject/CloneCurrentObjectEx (2/4) shares a driver client and its
FD namespace. SetAruid (8) validates PID framing and the single foreground applet
resource ID. SetGraphicsFirmwareMemoryMarginEnabled (13) stores the u64 request;
there is no graphics firmware allocator or host clock change. Initialize and
SetAruid return the NVIDIA status word in addition to CMIF status.

Open (0) accepts a bounded input descriptor and creates a typed device FD for
nvmap, nvhost-as-gpu, nvhost-gpu, nvhost-ctrl or nvhost-ctrl-gpu. Close (2) retires
that FD across cloned sessions; stale FDs fail. Unknown devices report NVIDIA
NotSupported, and unsupported GPU commands stop explicitly. Domain service
factories/conversion remain unsupported. This is not a GPU command processor,
shader compiler or renderer; opening a device does not execute GPU work.

The supplied game passes driver initialization and next requests `time:u`.
Native synthetic tests cover invalid initialization inputs, transfer lifetime,
allocator-slot reuse while pinned, clone/FD sharing, peer close, aliases, bad
device names, configuration framing and unsupported operation diagnostics.

Primary ABI sources consulted, not copied:

- https://github.com/switchbrew/libnx/blob/master/nx/source/services/nv.c
- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h
- https://switchbrew.org/wiki/NV_services
