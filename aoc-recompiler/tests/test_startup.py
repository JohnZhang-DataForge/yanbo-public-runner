# SPDX-License-Identifier: GPL-2.0-or-later
import unittest
from aoc_image import Image, BASE
from startup import STAGES, plan
from test_image import module, symbol


class StartupTests(unittest.TestCase):
    def image(self):
        names=STAGES+('nnMain',)
        m=module('main',[symbol(bind='local')]+[symbol(n,True,4+4*i) for i,n in enumerate(names)],
                 words=(0,)+(0xd65f03c0,)*8)
        m.dynamic={'INIT':[32]}
        return Image([m])

    def test_symbol_entries_and_initializer(self):
        result=plan(self.image())['stages']
        self.assertEqual(result[0]['address'],BASE+4)
        self.assertEqual(result[1]['main_thread_argument'],True)
        self.assertEqual(result[-2]['name'],'main DT_INIT')
        self.assertEqual(result[-1]['name'],'nnMain')
        self.assertIn('nninitInitializeSdkModule',[s['name'] for s in result])

    def test_missing_or_noncode_export_rejected(self):
        image=self.image();del image.exports['nnMain']
        with self.assertRaises(ValueError):plan(image)
        image=self.image();image.exports['nnMain'][1]['value']=0x10000
        with self.assertRaises(ValueError):plan(image)


if __name__=='__main__':unittest.main()
