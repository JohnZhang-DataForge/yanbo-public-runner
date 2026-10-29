import pathlib
import shutil
import struct
import subprocess
import tempfile
import unittest
from recompile import translate


def words(*values):
    return struct.pack('<' + 'I' * len(values), *values)


class RecompilerTests(unittest.TestCase):
    def test_rejects_unknown_and_malformed(self):
        for data in (b'', b'abc', words(0), words(0xD503201F), words(0x52C00000, 0xD65F03C0)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                translate(data)

    def test_compiled_semantics(self):
        # MOVZ X0,#42; ADD X0,X0,#8; SUB SP,SP,#16;
        # MOVZ W1,#1; MOVZ XZR,#9; NOP; RET X30.
        source = translate(words(0xD2800540, 0x91002000, 0xD10043FF,
                                 0x52800021, 0xD280013F, 0xD503201F, 0xD65F03C0))
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / 'block.hpp').write_text(source)
            (root / 'test.cpp').write_text('''#include "block.hpp"
int main() { GuestState s; s.sp=256; s.x[1]=~0ULL; s.x[30]=1234;
run_block(s); return (s.x[0]==50 && s.x[1]==1 && s.sp==240 && s.pc==1234) ? 0 : 1; }
''')
            if shutil.which('cl'):
                command = ['cl', '/nologo', '/std:c++17', '/EHsc', 'test.cpp', '/Fe:test.exe']
                exe = root / 'test.exe'
            else:
                command = ['c++', '-std=c++17', 'test.cpp', '-o', 'test']
                exe = root / 'test'
            subprocess.run(command, cwd=root, check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
