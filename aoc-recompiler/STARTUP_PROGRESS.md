# Private base-game startup execution

This is an experimental CPU/OS diagnostic, not a bootable Windows game port.
The supplied ExeFS is checked against all four exact NSO build IDs before use.
No game executable bytes, generated translations, ROMFS, firmware or keys belong
in the public validation repository.

The current private run completes all six exported SDK stages:
`_init_libc0`, `nnosInitialize`, `_init_libc1`,
`nninitInitializeSdkModule`, `nninitStartup`, and `_init_libc2`.
It reserves a 3.25 GiB guest heap, completes all three module constructors
(`main`, `subsdk0`, `sdk`), and enters `nnMain`. The 6,861-function private
probe passes CPU boost, retained transfer-memory ownership, nvdrv initialization,
service cloning, applet resource-ID registration and firmware-margin setup.
The clock service supplies a read-only shared page using the same counter epoch
as guest CPU instructions. The enlarged heap fixes an early normal return caused
by failed backing allocation for the game's fixed startup pools (~3097 MiB).

The current explicit stop is `SM GetService 'fsp-srv': service backend is not
implemented`. The supplied Drive ZIP is accessible and its index contains both
base `romfs/` and `dlcfs/`; it has not been mounted into a filesystem backend.
There is no rendered boot screen or established playability. The GPU command
processor, shader translation and renderer are still absent. Private game
execution was tested on Linux; public Windows runs use synthetic fixtures only.

## Reproduce locally

Use a dedicated private output directory. The output contains translated game
code and must not be uploaded to public CI. The source tool contains addresses
and source code only. It requires a native C compiler and the existing lifter
backend dependencies.

```sh
python probe_startup.py /private/exefs.zip --out /private/startup-probe --stages 10
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

Public Windows verification for nvdrv, time and the expanded memory profile passed native
CPU/OS tests and packaged tools:
https://github.com/JohnZhang-DataForge/yanbo-public-runner/actions/runs/41131148309
These are synthetic tests, not a public run of the game.
