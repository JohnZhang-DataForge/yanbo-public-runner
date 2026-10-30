# Age of Calamity recompiler research

Status: **CPU translation prototype, not a playable Windows port.** The NSO loader has been validated against the supplied base-game ExeFS.
Experimental native execution reaches SDK startup and module constructors.
A rendered boot screen and playability have not been established. See
`STARTUP_PROGRESS.md` for the current private diagnostic and reproducible probe.

The Python translator emits C++17 for a single, explicitly supplied raw
little-endian ARM64 block. Supported: MOVZ (32/64-bit), ADD/SUB immediate without
flags, NOP, and terminal RET. Unknown instructions fail with an offset instead
of silently producing incorrect code. RET updates the guest PC and exits the
block; it does not jump to a host address. Guest register state is explicit.

```
python recompile.py block.bin block.hpp
python -m unittest discover -s tests -v
```

Include `block.hpp` in a C++17 project and call `run_block(GuestState&)`.
The generated code must be compiled with MSVC, Clang, or GCC. Tests compile and
execute a synthetic block and check its results, SP, W-register zero extension,
and return PC. No Nintendo code or assets are included.

## Windows build

The `AoC Recompiler Windows Prototype` GitHub Actions workflow runs on
`windows-latest`, tests generated code with MSVC, and packages the **translator**
as `aoc-recompile.exe`. Download its artifact after a successful run:

```
aoc-recompile.exe block.bin block.hpp
```

This executable generates C++; it does not launch the game. Packaging the
translator is separate from translating and linking a complete game.

## Remaining port work

1. Identify the exact game version/build ID and examine a user-supplied,
   decrypted executable dump. NSO segment loading/decompression is implemented;
   NSP/XCI loading is absent.
2. Discover functions, relocations, control flow, jump tables, and indirect calls.
3. Implement remaining ARM64 instructions, flags, SIMD, guest memory, and ABI.
4. Implement Switch service calls, threading, filesystems, timing, and input.
5. Implement the graphics/shader/audio runtime and connect the game assets.
6. Validate against the selected executable build; add regression coverage.

The sections below describe the original translator and its individual tools.
Current kernel and service behavior is documented in `KERNEL_BOOTSTRAP.md`,
`APPLET_RUNTIME.md`, and `STARTUP_SERVICES.md`. Supplying RomFS alone cannot
replace executable translation or the missing graphics runtime.

Instruction semantics reference: Arm Instruction Set Reference Guide,
https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab

## ExeFS analysis

```
python analyze_exefs.py exefs.zip
```

Reads NSO0 modules directly from a ZIP, decompresses LZ4 segments, validates
bounds and enabled SHA-256 hashes, and prints a JSON inventory. It does not
extract archive paths or execute guest code. No external Python dependencies.
The analyzer runs from source; the Windows packaged translator currently only
includes the raw-block translation command.

See `BASE_GAME_ANALYSIS.md` for the first real input validation.

Format references:
- https://github.com/switchbrew/switch-tools/blob/master/src/elf2nso.c
- https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md

## Translate a selected NSO block

```
python extract_block.py main selected.hpp --address 0x1000 --size 0x20
```

Address is module-relative, not a host pointer or NSO file offset. The caller
must supply a known block. This command bounds-checks the range, loads and
verifies the NSO, and invokes the existing strict translator. It does not yet
discover functions; unsupported instructions still fail.

The public Windows validation mirror is
https://github.com/JohnZhang-DataForge/yanbo-public-runner/tree/aoc-windows-tests .
It contains original tool source and synthetic fixtures only.

## Unified Windows command-line tool

The Windows workflow now packages two executables:

- `aoc-recompile.exe`: original raw ARM64 block translator.
- `aoc-tool.exe`: ExeFS inspection, asset inventory/CRC verification, and NSO block translation.

```powershell
.\aoc-tool.exe inspect-exefs .\exefs.zip
.\aoc-tool.exe inspect-assets .\dlc.zip --verify-crc
.\aoc-tool.exe translate-nso .\main .\block.hpp --address 0x1000 --size 0x20
```

The last example needs a known, supported block; the example address is not a
verified game function. Generated C++ must still be compiled into a host test or
runtime. Neither executable launches the game.

Windows CI exercises packaged commands using synthetic NSO and asset archives,
then translates a synthetic NSO block, compiles its emitted C++ with MSVC, and
executes the host test to verify its result. Full game binaries and assets are
kept out of the public test repository.

