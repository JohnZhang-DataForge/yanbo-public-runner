import hashlib
import struct
import unittest
from nso import load_nso, decompress_lz4


def fixture():
    raw = bytearray(256)
    raw[:4] = b'NSO0'
    struct.pack_into('<I', raw, 12, 0x38)
    for i in range(3):
        payload = bytes([i + 1]) * 4
        struct.pack_into('<III', raw, 16 + 16*i, len(raw), i * 4096, len(payload))
        raw[160+32*i:192+32*i] = hashlib.sha256(payload).digest()
        raw.extend(payload)
    return raw


class NsoTests(unittest.TestCase):
    def test_load_and_hash(self):
        module = load_nso(fixture())
        self.assertEqual(module['segments'][2]['payload'], b'\x03' * 4)
        self.assertTrue(all(s['hash_verified'] for s in module['segments']))

    def test_corruption(self):
        raw = fixture()
        raw[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            load_nso(raw)

    def test_bounds_and_overlap(self):
        for offset, value in ((16, 0), (20, 4096), (24, 0xffffffff)):
            raw = fixture()
            struct.pack_into('<I', raw, offset, value)
            with self.assertRaises(ValueError):
                load_nso(raw)

    def test_lz4_literals_and_overlap(self):
        self.assertEqual(decompress_lz4(b'\x50hello', 5), b'hello')
        self.assertEqual(decompress_lz4(b'\x14a\x01\x00\x50hello', 14), b'a'*9+b'hello')
        for data, size in ((b'\xf0', 1), (b'\x00\x00\x00', 4), (b'\x50hi', 5), (b'\x50hello', 4)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                decompress_lz4(data, size)
