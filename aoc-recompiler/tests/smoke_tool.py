"""Check the packaged unified tool without redistributing game data."""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile
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
    nso=root/'main'
    nso.write_bytes(fixture())
    path=root/'exefs.zip'
    with zipfile.ZipFile(path,'w') as z:z.write(nso,'exefs/main')
    report=json.loads(subprocess.check_output([str(exe),'inspect-exefs',str(path)],text=True))
    assert len(report['modules']['exefs/main']['segments'])==3
    result=subprocess.run([str(exe),'translate-nso',str(nso),str(root/'out.hpp'),'--address','0','--size','4'],capture_output=True,text=True)
    assert result.returncode!=0 and 'Unsupported instruction' in result.stderr
print('Packaged unified tool smoke test passed')
