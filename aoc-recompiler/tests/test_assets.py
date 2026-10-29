import pathlib
import tempfile
import unittest
import zipfile
from aoc_tool import inspect_assets


class AssetsTests(unittest.TestCase):
    def test_inventory_and_crc(self):
        with tempfile.TemporaryDirectory() as directory:
            path=pathlib.Path(directory)/'assets.zip'
            with zipfile.ZipFile(path, 'w') as z:
                z.writestr('romfs/test.bin', b'abc')
                z.writestr('dlcfs/1/test.bin', b'defg')
            report=inspect_assets(path, True)
            self.assertEqual(report['files_by_root'], {'romfs':1,'dlcfs':1})
            self.assertEqual(report['uncompressed_bytes'], 7)
            self.assertTrue(report['crc_verified'])

    def test_unsafe_names(self):
        for name in ('../bad', '/bad', 'C:/bad', 'romfs/../../bad', '..\\bad'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                path=pathlib.Path(directory)/'assets.zip'
                with zipfile.ZipFile(path,'w') as z:z.writestr(name,b'bad')
                with self.assertRaisesRegex(ValueError,'Unsafe'):inspect_assets(path)
