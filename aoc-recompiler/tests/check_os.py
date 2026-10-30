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
    image=Image([module('synthetic',[symbol(bind='local'),symbol('unknown',True,16),symbol('return',True,12)],words=words)])
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
    result=subprocess.run([str(os_probe),str(out/'image.bin'),'0x08000000'],check=True,capture_output=True,text=True,timeout=30)
    assert 'stop=2' in result.stderr,result.stderr
    print('OS diagnostic build and synthetic guest entry execution passed')

    missing=subprocess.run([str(os_probe),str(out/'image.bin')],capture_output=True,text=True,timeout=30)
    assert missing.returncode==1 and 'No startup manifest' in missing.stderr

    # An early-stage failure must prevent every subsequent initializer/nnMain.
    from startup import header
    (out/'hos_startup.h').write_text(header(dict(stages=[
        dict(name='libc stage',address=0x0800000c,main_thread_argument=False),
        dict(name='SDK service stage',address=0x08000010,main_thread_argument=True),
        dict(name='nnMain',address=0x08000000,main_thread_argument=False)])))
    os_probe=build_os_probe(out)
    result=subprocess.run([str(os_probe),str(out/'image.bin')],capture_output=True,text=True,timeout=30)
    assert result.returncode==2,result.stderr
    assert 'libc stage' in result.stderr and 'SDK service stage' in result.stderr,result.stderr
    assert 'nnMain' not in result.stderr and 'stop=3' in result.stderr,result.stderr
    print('Startup stage sequencing and fail-stop execution passed')
