"""Check the packaged unified tool without redistributing game data."""
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from test_nso import fixture

exe = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory)
    path=root/'assets.zip'
    with zipfile.ZipFile(path,'w') as z:
        z.writestr('romfs/test.bin',b'abc')
        z.writestr('dlcfs/1/test.bin',b'def')
    report=json.loads(subprocess.check_output([str(exe),'inspect-assets',str(path),'--verify-crc'],text=True))
    assert report['crc_verified'] and report['uncompressed_bytes']==6
    report=json.loads(subprocess.check_output([str(exe),'build-romfs',str(path),str(root/'base.romfs')],text=True))
    assert report['files']==1
    data=(root/'base.romfs').read_bytes();header=struct.unpack_from('<10Q',data)
    assert header[0]==80 and data[header[9]:]==b'abc'
    nso=root/'main'
    nso.write_bytes(fixture())
    path=root/'exefs.zip'
    with zipfile.ZipFile(path,'w') as z:z.write(nso,'exefs/main')
    report=json.loads(subprocess.check_output([str(exe),'inspect-exefs',str(path)],text=True))
    assert len(report['modules']['exefs/main']['segments'])==3
    result=subprocess.run([str(exe),'translate-nso',str(nso),str(root/'out.hpp'),'--address','0','--size','4'],capture_output=True,text=True)
    assert result.returncode!=0 and 'Unsupported instruction' in result.stderr
    # Construct a valid NSO with a synthetic MOVZ X0,#42; RET X30 block.
    raw=bytearray(256)
    raw[:4]=b'NSO0'
    struct.pack_into('<I',raw,12,0x38)
    for i,payload in enumerate((struct.pack('<II',0xD2800540,0xD65F03C0),b'RO',b'DATA')):
        struct.pack_into('<III',raw,16+16*i,len(raw),i*4096,len(payload))
        raw[160+32*i:192+32*i]=hashlib.sha256(payload).digest()
        raw.extend(payload)
    nso.write_bytes(raw)
    subprocess.run([str(exe),'translate-nso',str(nso),str(root/'out.hpp'),'--address','0','--size','8'],check=True)
    (root/'test.cpp').write_text('#include "out.hpp"\nint main(){GuestState s; s.x[30]=1234; run_block(s); return s.x[0]==42 && s.pc==1234 ? 0 : 1;}\n')
    subprocess.run(['cl','/nologo','/std:c++17','/EHsc','test.cpp','/Fe:test.exe'],cwd=root,check=True)
    subprocess.run([str(root/'test.exe')],check=True)
print('Packaged unified tool smoke test passed')

