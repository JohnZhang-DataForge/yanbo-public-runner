# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile and execute OS services called by synthetic lifted guest functions."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(Path(__file__).resolve().parent))
from aoc_image import Image
from build_lift import build as build_cpu_probe
from build_os import build as build_os_probe
from aoc_lift import lift,ROOT
from test_image import module,symbol

with tempfile.TemporaryDirectory() as temporary:
    out=Path(temporary)/'generated'
    # SetHeapSize; ExitThread; unreachable MOVZ; RET; unknown SVC; unreachable MOVZ; RET.
    words=(0xD4000021,0xD4000141,0xD2801FE0,0xD65F03C0,0xD4001FE1,0xD2801FE0,0xD65F03C0)
    image=Image([module('synthetic',[symbol(bind='local'),symbol('unknown',True,16)],words=words)])
    lift(image,out)
    compiler=shutil.which('clang') or shutil.which('gcc')
    executable=out/('hos-test.exe' if os.name=='nt' else 'hos-test')
    sources=sorted(out.glob('*.c'))+[ROOT/'os/hos.c',ROOT/'tests/hos_test.c']
    command=[compiler,'-std=gnu11','-O0','-mcx16','-DA64_NO_MUSTTAIL','-D_POSIX_C_SOURCE=200809L',
        '-Werror=implicit-function-declaration','-I',str(out),'-I',str(ROOT/'os'),
        *[str(p) for p in sources],'-o',str(executable)]
    if os.name!='nt':command+=['-lm','-latomic']
    subprocess.run(command,check=True)
    subprocess.run([str(executable),str(out/'image.bin')],check=True,timeout=30)
    build_cpu_probe(out,jobs=2)
    os_probe=build_os_probe(out)
    result=subprocess.run([str(os_probe),str(out/'image.bin')],check=True,capture_output=True,text=True,timeout=30)
    assert 'stop=2' in result.stderr,result.stderr
    print('OS diagnostic build and synthetic guest entry execution passed')
