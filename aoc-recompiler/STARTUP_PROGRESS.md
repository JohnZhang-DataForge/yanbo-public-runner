# Private base-game startup execution

This is an experimental CPU/OS diagnostic, not a bootable Windows game port.
The supplied ExeFS is checked against all four exact NSO build IDs before use.
No game executable bytes, generated translations, ROMFS, firmware or keys belong
in the public validation repository.

The current private run completes all six exported SDK stages:
`_init_libc0`, `nnosInitialize`, `_init_libc1`,
`nninitInitializeSdkModule`, `nninitStartup`, and `_init_libc2`.
It reserves a 3.25 GiB guest heap, completes all three module constructors
(`main`, `subsdk0`, `sdk`), and enters `nnMain`. The 6,943-function private
probe passes CPU boost, retained transfer-memory ownership, nvdrv initialization,
service cloning, applet resource-ID registration and firmware-margin setup.
The clock service supplies a read-only shared page using the same counter epoch
as guest CPU instructions. The enlarged heap fixes an early normal return caused
by failed backing allocation for the game's fixed startup pools (~3097 MiB).

The filesystem service now registers the process, shares domain objects across
cloned sessions, and opens the supplied base-game RomFS as read-only IStorage.
The archive was downloaded privately and converted to a 11384795826-byte image
with 1086 files. The real guest successfully read its RomFS header, directory
hash/table and file hash/table (six storage reads). The filesystem worker thread
also runs. DLC remains excluded from the base mount.

The current explicit stop is `SM GetService 'vi:m': service backend is not
implemented`. This is the display-service boundary. A rendered boot frame and
playability are not established; GPU command processing, shaders and rendering
remain absent. Private game execution was tested on Linux; public Windows
runs use synthetic fixtures only. See `FS_RUNTIME.md` for the filesystem ABI.

## Reproduce locally

Use a dedicated private output directory. The output contains translated game
code and must not be uploaded to public CI. The source tool contains addresses
and source code only. It requires a native C compiler and the existing lifter
backend dependencies.

```sh
python probe_startup.py /private/exefs.zip --out /private/startup-probe --stages 10 --romfs /private/base.romfs
```

`--stages` selects how many manifest entries to compile, not how many startup
stages to execute or skip. The executable always attempts the complete ordered
manifest and stops if an entry is absent. Dependencies and constructor-array
entries are included. The default function bound is 8192; exceeding it fails
before generation. `--seed 0xADDRESS` includes an additional observed indirect
call target. Missing functions and unsupported OS/service operations stop with
an explicit diagnostic. There is no service bypass or generic success fallback.

The dedicated output directory has an ownership marker. Re-running preserves
cached objects but removes stale generated translation units. The log is saved
as `startup-run.log`; its exit status is propagated by the tool.

Public Windows verification for the filesystem, 64-bit reads and packaged RomFS builder passed native
CPU/OS tests and packaged tools:
https://github.com/JohnZhang-DataForge/yanbo-public-runner/actions/runs/41137313824
These are synthetic tests, not a public run of the game.
