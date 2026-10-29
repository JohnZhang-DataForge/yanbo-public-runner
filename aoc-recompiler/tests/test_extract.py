import struct
import unittest
from extract_block import extract
from test_nso import fixture


class ExtractTests(unittest.TestCase):
    def test_exact_block(self):
        self.assertEqual(extract(fixture(), 0, 4), b'\x01'*4)

    def test_rejects_bad_range(self):
        for address, size in ((-4, 4), (1, 4), (0, 0), (0, 8), (4096, 4)):
            with self.subTest(address=address, size=size), self.assertRaises(ValueError):
                extract(fixture(), address, size)


if __name__ == '__main__':
    unittest.main()
