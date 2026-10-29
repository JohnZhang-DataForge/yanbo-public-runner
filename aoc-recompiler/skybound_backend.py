"""Isolated GPL ARM64 emitter backend from the supplied Skybound source archive."""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'vendor/skybound/a64emit.cpp'


def build(output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    library = output / ('a64emit.dll' if os.name == 'nt' else 'liba64emit.so')
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise RuntimeError('C++20 compiler required')
    command = [compiler, '-std=c++20', '-O1', '-shared', str(SOURCE), '-o', str(library)]
    if os.name != 'nt':
        command.append('-fPIC')
    subprocess.run(command, check=True)
    return library


class Backend:
    def __init__(self, library):
        self.lib = ctypes.CDLL(str(Path(library).resolve()))
        self.lib.a64_config.argtypes = [ctypes.c_int, ctypes.c_int]
        self.lib.a64_config(1, 0)  # Exact FP mode; no JIT fallback request.
        self.lib.a64_translate_batch.restype = ctypes.c_longlong
        self.lib.a64_translate_batch.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int,
            ctypes.c_void_p, ctypes.c_longlong, ctypes.c_void_p, ctypes.c_void_p]
        self.lib.a64_runtime_text.restype = ctypes.c_char_p
        self.lib.a64_runtime_text.argtypes = [ctypes.c_int]

    def translate(self, words, base=0):
        if not words or len(words) > 4096:
            raise ValueError('Supply 1..4096 instructions per batch')
        count = len(words)
        instructions = (ctypes.c_uint32 * count)(*words)
        addresses = (ctypes.c_uint64 * count)(*(base + 4*i for i in range(count)))
        offsets = (ctypes.c_longlong * (count+1))()
        flags = (ctypes.c_int * count)()
        capacity = 16 * 1024 * 1024
        buffer = ctypes.create_string_buffer(capacity)
        size = self.lib.a64_translate_batch(instructions, addresses, count, buffer, capacity, offsets, flags)
        if not 0 <= size <= capacity:
            raise ValueError('Generated output exceeds batch capacity')
        return buffer.raw[:size].decode('latin-1'), list(flags)

    def header(self):
        return self.lib.a64_runtime_text(0).decode('utf-8')
