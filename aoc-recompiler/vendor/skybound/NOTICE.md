# Source provenance and licensing

Imported verbatim from the user-supplied `SkyboundTotk-main(1).zip`:

- `a64-kit/lift/a64emit.cpp` -> `a64emit.cpp`.
- `a64-kit/lift/suyu/arm64_to_c.h` -> `suyu/arm64_to_c.h`.
- Both supplied license texts in `a64-kit/lift/suyu/` retained.

The emitter identifies suyu Emulator Project, Copyright 2026, GPL-2.0-or-later.
The wrapper identifies GPL-2.0-or-later. This is an isolated GPL backend;
its source is provided with the code. New adapter and check script are also
provided under GPL-2.0-or-later. No game code or assets are included here.

This import is an experiment, not a complete game runtime. Generated helpers
must use this emitter's GuestContext contract. Do not substitute the small
independent prototype's GuestState or claim that successful code generation
proves a game can boot.

Additional source import for offline module analysis:

- `a64-kit/tools/nso.py` -> `tools/nso_metadata.py`.
- Original Skybound MIT license retained as `SKYBOUND_LICENSE` (NotRexed, 2026).
- Local changes: use our bounded LZ4 decoder instead of external lz4; retain
  symbol visibility/section and relocation symbol indexes for correct binding.

`aoc_image.py` implements the offline image/analysis adapter under GPL-2.0-or-later.
It verifies the exact AoC module profile and hashes before invoking the metadata
parser. No upstream game-specific runtime hooks are imported.
