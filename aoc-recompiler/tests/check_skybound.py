"""Build emitter and compile/execute its generated synthetic integer block."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from skybound_backend import Backend, build

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    backend = Backend(build(root))
    # MOVZ X0,#42; ADD X0,X0,#8; SUBS X1,X0,#50; RET X30.
    code, flags = backend.translate([0xD2800540,0x91002000,0xF100C801,0xD65F03C0])
    assert flags == [0,0,0,2], flags
    (root/'recomp_runtime.h').write_text(backend.header(), encoding='utf-8')
    source = '#include "recomp_runtime.h"\nvoid block(GuestContext* c){\n'+code+'\n}\n'
    source += 'int main(){GuestContext c{};c.x[30]=1234;block(&c);return c.x[0]==50 && c.x[1]==0 && c.z==1 && c.c==1 && c.n==0 && c.v==0 && c.pc==1234 ? 0 : 1;}\n'
    (root/'test.cpp').write_text(source, encoding='utf-8')
    compiler = shutil.which('clang++') or shutil.which('g++')
    executable = root/('test.exe' if os.name=='nt' else 'test')
    subprocess.run([compiler,'-std=c++20',str(root/'test.cpp'),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
    print('Skybound emitter: built from source; emitted C++ compiled and executed; integer, NZCV and RET checks passed')
