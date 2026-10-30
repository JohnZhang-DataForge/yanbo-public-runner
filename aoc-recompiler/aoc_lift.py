# SPDX-License-Identifier: GPL-2.0-or-later
"""AoC multi-function lift using an isolated Skybound runtime contract."""
import argparse
import importlib
import json
from pathlib import Path
import shutil
import struct
import sys
from aoc_image import Image, load_archive, BASE
from startup import plan, header

ROOT=Path(__file__).resolve().parent
VENDOR=ROOT/'vendor/skybound'


def lifter_modules():
    sys.path.insert(0,str(VENDOR/'lift'))
    return importlib.import_module('a64_lift'),importlib.import_module('a64_rtgen')


class LiftImage:
    def __init__(self,image):
        self.image=image
        self.entries,_=image.seeds()
        self.hooks={}
        if image.unresolved:
            raise ValueError('Unresolved imports require explicit runtime hooks; refusing lift')
        self.eh_starts=sorted(m.base+a for m in image.modules for a in m.eh_funcs if self.in_code(m.base+a))
        self.names={image.symbol_address(m,s):s['name'] for m in image.modules for s in m.symbols
                    if s['defined'] and s['type']=='func' and s['name']}
        for lo,hi,_ in image.code:
            for pc in range(lo,hi,4):
                word=self.word(pc)
                if word&0xfc000000==0x94000000:
                    imm=word&0x3ffffff
                    target=pc+4*(imm-(1<<26) if imm&(1<<25) else imm)
                    if self.in_code(target):self.entries.add(target)

    def in_code(self,address):return self.image.locate(address) is not None

    def word(self,address):
        if not self.in_code(address):raise ValueError('Instruction outside executable segment')
        return struct.unpack_from('<I',self.image.mem,address-BASE)[0]

    def read(self,address,size):
        if size<0 or address<BASE or address+size>self.image.end:
            raise ValueError('Read outside linked image')
        return bytes(self.image.mem[address-BASE:address-BASE+size])


def lift(image,out):
    if out.exists() and any(out.iterdir()):raise ValueError('Output directory must be empty')
    out.mkdir(parents=True,exist_ok=True)
    module,rtgen=lifter_modules()
    adapted=LiftImage(image)
    lifter=module.Lifter(adapted)
    print('Discovering',len(adapted.entries),'candidate entries',flush=True)
    lifter.discover(max_insns=4096)
    print('Generating C for',len(lifter.funcs),'functions',flush=True)
    files=lifter.write(str(out),chunk_bytes=4*1024*1024)
    rtgen.write(str(out))
    for name in ('a64.h','a64_rt.c'):shutil.copyfile(VENDOR/'rt'/name,out/name)
    (out/'image.bin').write_bytes(image.mem)
    if 'nninitStartup' in image.exports:
        manifest=plan(image)
        (out/'startup.json').write_text(json.dumps(manifest,indent=2))
        (out/'hos_startup.h').write_text(header(manifest))
    report=dict(functions=len(lifter.funcs),emitted_instructions=sum(len(b) for b,_ in lifter.funcs.values()),
                translation_units=len(files),recovered_jump_tables=len(lifter.jt),
                unresolved_indirect_branches=len(lifter.ijump),unhandled_patterns={f'{k:08x}':v for k,v in lifter.unhandled.items()},
                stats=dict(lifter.stats),native_linked=False,game_booted=False,
                caveat='Generated code may trap; static recovery and emission do not prove execution correctness or runtime completeness.')
    (out/'lift.json').write_text(json.dumps(report,indent=2))
    lifter.report()
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exefs_zip')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(lift(Image(load_archive(args.exefs_zip)),args.out),indent=2))
