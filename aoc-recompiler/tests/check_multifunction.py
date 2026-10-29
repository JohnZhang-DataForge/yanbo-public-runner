# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile and execute lifted indirect calls with the matching CPU runtime."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(Path(__file__).resolve().parent))
from aoc_image import Image, BASE
from aoc_lift import lift
from test_image import module,symbol

with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary)
    # BLR X8; RET; ADD X0,X0,#8; RET. Two candidate function entries.
    m=module('synthetic',[symbol(bind='local'),symbol('callee',True,8)],
             words=(0xD63F0100,0xD65F03C0,0x91002000,0xD65F03C0))
    out=root/'generated'
    report=lift(Image([m]),out)
    assert report['functions']==2 and not report['unhandled_patterns'],report
    (out/'test.c').write_text('''#include "a64.h"
extern void f_08000000(Ctx *);
int main(void) { Ctx c={0}; c.x[0]=42; c.x[8]=0x08000008; f_08000000(&c);
return c.x[0]==50 && !c.halted ? 0 : 1; }
''')
    compiler=shutil.which('clang') or shutil.which('gcc')
    executable=out/('test.exe' if os.name=='nt' else 'test')
    sources=sorted(out.glob('*.c'))
    command=[compiler,'-std=gnu11','-O0','-mcx16','-DA64_NO_MUSTTAIL','-D_POSIX_C_SOURCE=200809L',
             '-I',str(out),*[str(p) for p in sources],'-o',str(executable)]
    if os.name!='nt':command+=['-lm','-latomic']
    subprocess.run(command,check=True)
    subprocess.run([str(executable)],check=True,timeout=15)
    print('Multi-function lift + native indirect dispatch passed')
