"""Exercise the packaged translator CLI using synthetic instructions only."""
import pathlib
import struct
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    source, output = root / 'block.bin', root / 'block.hpp'
    source.write_bytes(struct.pack('<II', 0xD2800540, 0xD65F03C0))
    subprocess.run([str(exe), str(source), str(output)], check=True)
    assert 's.x[0] = uint64_t(42ULL);' in output.read_text()
    invalid_output = root / 'invalid.hpp'
    source.write_bytes(bytes(4))
    result = subprocess.run([str(exe), str(source), str(invalid_output)], capture_output=True, text=True)
    assert result.returncode != 0 and 'Unsupported instruction' in result.stderr
    assert not invalid_output.exists()
print('Packaged translator CLI smoke test passed')
