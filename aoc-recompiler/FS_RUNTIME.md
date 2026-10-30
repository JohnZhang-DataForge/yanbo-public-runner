# Read-only filesystem bootstrap

`fsp-srv` supports domain conversion, PID-validated SetCurrentProcess (1), and
OpenDataStorageByCurrentProcess (200). Storage objects expose GetSize (4) and
Read (0) through one bounded receive-buffer descriptor. Reads use 64-bit file
positions on both Windows and POSIX, validate the entire destination/range,
and report genuine I/O failures. Writes and unsupported operations stop.

Mount a raw, decrypted RomFS image before startup using `HOS_ROMFS_PATH` or the
private probe's `--romfs` argument. Missing input is an explicit stop, not an
empty successful filesystem. Images are opened read-only, structurally checked,
and bound once for the lifetime of the host process. Guest paths never become
host filesystem paths.

```sh
python build_romfs.py /private/assets.zip --out /private/base.romfs
python probe_startup.py /private/exefs.zip --out /private/probe --romfs /private/base.romfs
```

The builder selects only `romfs/` by default, excludes DLC, creates directory and
file hash tables, preserves UTF-8 names and streams file contents while ZIP CRCs
are checked. Output is committed only after all input reads succeed. Duplicate,
traversing, conflicting and symlink paths are rejected; existing outputs and
unrelated temporary files are not overwritten or deleted. Built images contain
game assets and belong only in a private directory, never public CI.

FSP's cloned domain sessions share object IDs and registration state. Closing
one client does not destroy the table while another clone still references it.
Closed owner slots cannot be recycled prematurely. Object creation and explicit
domain close affect all clones; stale IDs remain invalid. The global immutable
RomFS backing survives storage-object close and is released when Hos is destroyed.
Other service-domain cloning remains unsupported.

Native synthetic tests cover registration, clone sharing, owner-slot reuse,
object lifetime, image validation, reads beyond 4 GiB, short buffers, out-of-range
and negative offsets, read-only guest destinations and rejected writes. The
Windows fixture uses a sparse file rather than allocating 4 GiB of physical data.
Python tests independently traverse emitted RomFS hash chains and read files,
including nested UTF-8 names, and exercise unsafe/ambiguous archive input.

The supplied archive was downloaded privately and its base image was generated
with 1086 files in 6 directories (11384795826 bytes). Public tests use synthetic
archives only. Service/data access does not establish rendered boot or gameplay.

Primary ABI/layout sources consulted (not copied):

- https://github.com/switchbrew/libnx/blob/master/nx/source/services/fs.c
- https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/devices/romfs_dev.c
- https://github.com/switchbrew/libnx/blob/master/nx/include/switch/runtime/devices/romfs_dev.h
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/fs_results.hpp
