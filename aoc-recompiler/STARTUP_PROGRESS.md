# Private base-game startup execution

This is an experimental CPU/OS diagnostic, not a bootable Windows game port.
The supplied ExeFS is checked against all four exact NSO build IDs before use.
No game executable bytes, generated translations, ROMFS, firmware or keys belong
in the public validation repository.

The current private run completes all six exported SDK stages:
`_init_libc0`, `nnosInitialize`, `_init_libc1`,
`nninitInitializeSdkModule`, `nninitStartup`, and `_init_libc2`.
It allocates a 1 GiB guest heap, completes all three module constructors
(`main`, `subsdk0`, `sdk`), and enters `nnMain`. The 6,809-function private diagnostic also
passes the application's CPU boost request and creates an 8 MiB transfer-memory
object. The next explicit stop is `SM GetService 'nvdrv': service backend is not
implemented`. This is not a rendered boot screen or proof of asset loading or
playability. The private game execution was tested on Linux; public Windows
runs execute synthetic fixtures only.

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

Public Windows verification for the transfer-memory and CPU-boost checkpoint passed native
CPU/OS tests and packaged tools:
https://github.com/JohnZhang-DataForge/yanbo-public-runner/actions/runs/41058357603
These are synthetic tests, not a public run of the game.
