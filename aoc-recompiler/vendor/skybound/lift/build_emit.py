# SPDX-License-Identifier: GPL-2.0-or-later
"""Local adapter: build the imported emitter with the project's native compiler."""
import hashlib
import os
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[3]))
from skybound_backend import build as build_backend


def build(force=False):
    root=Path(__file__).resolve().parents[1]
    out=root/'build'
    library=out/('a64emit.dll' if os.name=='nt' else 'liba64emit.so')
    key=hashlib.sha256((root/'a64emit.cpp').read_bytes()+(root/'suyu/arm64_to_c.h').read_bytes()).hexdigest()
    stamp=out/'source.sha256'
    if force or not library.exists() or not stamp.exists() or stamp.read_text()!=key:
        build_backend(out)
        stamp.write_text(key)
    return str(library)
