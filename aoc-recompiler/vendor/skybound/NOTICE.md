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
