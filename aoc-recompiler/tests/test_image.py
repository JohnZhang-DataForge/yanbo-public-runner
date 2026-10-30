# SPDX-License-Identifier: GPL-2.0-or-later
import struct
from types import SimpleNamespace
import unittest
from aoc_image import Image, BASE, TRAMP


def symbol(name='', defined=False, value=0, bind='global', visibility=0, section=1):
    return dict(name=name,defined=defined,value=value,bind=bind,visibility=visibility,section=section,type='func')


def module(name, symbols=None, relocs=(), words=(0xd65f03c0,)):
    image=bytearray(max(256,4*len(words)))
    for i,w in enumerate(words):struct.pack_into('<I',image,4*i,w)
    return SimpleNamespace(name=name,image=image,symbols=symbols or [symbol(bind='local')],
        relocs=list(relocs),eh_funcs=[],dynamic={},build_id='synthetic',code_range=lambda:(0,len(words)*4))


def relocation(kind,index=0,addend=0,offset=128):
    return dict(type=kind,symbol_index=index,addend=addend,offset=offset)


class ImageTests(unittest.TestCase):
    def test_cross_module_binding_and_addend(self):
        a=module('main',[symbol(bind='local'),symbol('function')],[relocation('JUMP_SLOT',1,7)])
        b=module('sdk',[symbol(bind='local'),symbol('function',True,12)])
        image=Image([a,b])
        self.assertEqual(struct.unpack_from('<Q',image.mem,128)[0],b.base+19)
        self.assertFalse(image.unresolved)

    def test_relative_and_null_symbol(self):
        a=module('main',relocs=[relocation('RELATIVE',addend=16),relocation('ABS64',addend=9,offset=136)])
        image=Image([a])
        self.assertEqual(struct.unpack_from('<QQ',image.mem,128),(BASE+16,9))

    def test_weak_zero_and_unresolved_trampoline(self):
        a=module('main',[symbol(bind='local'),symbol('optional',bind='weak'),symbol('missing')],
            [relocation('ABS64',1,offset=128),relocation('JUMP_SLOT',2,offset=136)])
        image=Image([a])
        self.assertEqual(struct.unpack_from('<QQ',image.mem,128),(0,TRAMP))
        self.assertEqual(image.unresolved,{'missing':1})

    def test_protected_definition_and_absolute_symbol(self):
        a=module('a',[symbol(bind='local'),symbol('f',True,4),symbol('abs',True,55,section=0xfff1)])
        b=module('b',[symbol(bind='local'),symbol('f',True,8,visibility=3)],
                 [relocation('GLOB_DAT',1,offset=128)])
        image=Image([a,b])
        self.assertEqual(struct.unpack_from('<Q',image.mem,b.base-BASE+128)[0],b.base+8)
        self.assertEqual(image.symbol_address(a,a.symbols[2]),55)

    def test_reject_invalid_relocation(self):
        for r in (relocation('RELATIVE',offset=252),relocation('TLS_TPREL'),relocation('ABS64',index=8)):
            with self.subTest(r=r),self.assertRaises(ValueError):Image([module('a',relocs=[r])])

    def test_control_flow_skips_unconditional_fallthrough(self):
        a=module('a',words=(0x14000002,0xd4000001,0xd65f03c0))
        report=Image([a]).discover()
        self.assertEqual(report['counts']['visited_words'],2)
        self.assertEqual(report['counts'].get('svc_sites',0),0)

    def test_conditional_both_paths_and_indirect_call(self):
        a=module('a',words=(0xB4000040,0xd63f0000,0xd65f03c0))
        report=Image([a]).discover()
        self.assertEqual(report['counts']['visited_words'],3)
        self.assertEqual(report['counts']['conditional_branches'],1)
        self.assertEqual(report['counts']['indirect_calls'],1)

    def test_loop_terminates_and_external_edge_reported(self):
        report=Image([module('loop',words=(0x14000000,))]).discover()
        self.assertEqual(report['counts']['visited_words'],1)
        report=Image([module('out',words=(0x14000040,))]).discover()
        self.assertEqual(report['counts']['external_direct_edges'],1)

