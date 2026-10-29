# SPDX-License-Identifier: GPL-2.0-or-later
"""AoC offline module linker and conservative seeded control-flow discovery."""
import argparse
import bisect
import collections
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import zipfile
from nso import load_nso, MAX_SEGMENT

ROOT = Path(__file__).resolve().parent
EXPECTED = {
 'main':'935ebe83c4a0658348a8a1c05cf8dac3f43ceb84000000000000000000000000',
 'subsdk0':'3ab92c3444d4ece2f52417b5d8ee86eb115212a3000000000000000000000000',
 'sdk':'95832210b36121e88b4f9b7b66239dade9c82a67000000000000000000000000',
 'rtld':'51b657503637c8a8d6729ce7d8ddaa6a163c356a000000000000000000000000',
}
BASE = 0x08000000
TRAMP = 0x07000000


def metadata_class():
    path=ROOT/'vendor/skybound/tools/nso_metadata.py'
    spec=importlib.util.spec_from_file_location('skybound_nso_metadata',path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.Nso


def load_archive(path):
    parser=metadata_class()
    modules=[]
    with tempfile.TemporaryDirectory() as directory, zipfile.ZipFile(path) as archive:
        for name in EXPECTED:
            matches=[i for i in archive.infolist() if not i.is_dir() and Path(i.filename).name==name]
            if len(matches)!=1 or matches[0].file_size>MAX_SEGMENT:
                raise ValueError('Missing, ambiguous or oversized module: '+name)
            raw=archive.read(matches[0])
            checked=load_nso(raw)
            if checked['build_id']!=EXPECTED[name]:
                raise ValueError('Unexpected base-game build ID: '+name)
            target=Path(directory)/name
            target.write_bytes(raw)
            parsed=parser(target)
            if len(parsed.image)>MAX_SEGMENT:
                raise ValueError('Oversized module image')
            if name!='rtld':
                modules.append(parsed)
    return modules


class Image:
    def __init__(self, modules):
        self.modules=modules
        self.unresolved=collections.Counter()
        self.trampolines={}
        self.exports={}
        base=BASE
        for m in modules:
            m.base=base
            base=(base+len(m.image)+0xfffff)&~0xfffff
            for s in m.symbols:
                if s['defined'] and s['name'] and s['bind']!='local' and s.get('visibility',0) in (0,3):
                    self.exports.setdefault(s['name'],(m,s))
        self.end=base
        self.mem=bytearray(base-BASE)
        self.code=[]
        for m in modules:
            self.mem[m.base-BASE:m.base-BASE+len(m.image)]=m.image
            lo,hi=m.code_range()
            self.code.append((m.base+lo,m.base+hi,m))
        self.starts=[lo for lo,_,_ in self.code]
        self.applied=collections.Counter()
        self.relocated_entries=set()
        self.relocate()

    @staticmethod
    def symbol_address(m,s):
        return s['value'] if s.get('section')==0xfff1 else m.base+s['value']

    def resolve(self,m,index):
        if not 0<=index<len(m.symbols):
            raise ValueError('Relocation symbol index out of range')
        s=m.symbols[index]
        if s['defined'] and (s['bind']=='local' or s.get('visibility',0) in (1,2,3)):
            return self.symbol_address(m,s)
        hit=self.exports.get(s['name'])
        if hit:
            return self.symbol_address(*hit)
        if s['defined']:
            return self.symbol_address(m,s)
        if index==0 or s['bind']=='weak':
            return 0
        self.unresolved[s['name']]+=1
        return self.trampolines.setdefault(s['name'],TRAMP+16*len(self.trampolines))

    def locate(self,address):
        if address&3:
            return None
        index=bisect.bisect_right(self.starts,address)-1
        if index>=0 and address<self.code[index][1]:
            return index
        return None

    def relocate(self):
        for m in self.modules:
            for r in m.relocs:
                off=r['offset']
                if off<0 or off+8>len(m.image):
                    raise ValueError('Relocation writes outside module')
                if r['type']=='RELATIVE':
                    value=m.base+r['addend']
                elif r['type'] in ('ABS64','GLOB_DAT','JUMP_SLOT'):
                    value=self.resolve(m,r['symbol_index'])+r['addend']
                else:
                    raise ValueError('Unsupported relocation: '+str(r['type']))
                struct.pack_into('<Q',self.mem,m.base-BASE+off,value&0xffffffffffffffff)
                self.applied[r['type']]+=1
                if r['type'] in ('ABS64','RELATIVE') and self.locate(value) is not None:
                    self.relocated_entries.add(value)

    def seeds(self):
        seeds=set(self.relocated_entries)
        initializers={}
        for m in self.modules:
            seeds.add(m.base+m.code_range()[0])
            seeds.update(m.base+a for a in m.eh_funcs)
            seeds.update(self.symbol_address(m,s) for s in m.symbols if s['defined'] and s['type']=='func')
            for tag in ('INIT','FINI'):
                seeds.update(m.base+a for a in m.dynamic.get(tag,[]))
            start=m.dynamic.get('INIT_ARRAY',[0])[0]
            size=m.dynamic.get('INIT_ARRAYSZ',[0])[0]
            if size%8 or start<0 or start+size>len(m.image):
                raise ValueError('Invalid initializer array')
            arr=[struct.unpack_from('<Q',self.mem,m.base-BASE+start+i)[0] for i in range(0,size,8)]
            initializers[m.name]=arr
            seeds.update(arr)
        return {a for a in seeds if self.locate(a) is not None},initializers

    def discover(self):
        seeds,initializers=self.seeds()
        pending=list(seeds)
        seen=[bytearray((hi-lo)//4) for lo,hi,_ in self.code]
        counts=collections.Counter()
        calls=set()
        unresolved=[]
        indirect=[]
        def target(pc,destination,kind):
            if self.locate(destination) is not None:
                pending.append(destination)
            else:
                counts['external_direct_edges']+=1
                if len(unresolved)<100:
                    unresolved.append(dict(pc=pc,target=destination,kind=kind))
        while pending:
            pc=pending.pop()
            while True:
                index=self.locate(pc)
                if index is None:
                    break
                slot=(pc-self.code[index][0])//4
                if seen[index][slot]:
                    break
                seen[index][slot]=1
                word=struct.unpack_from('<I',self.mem,pc-BASE)[0]
                counts['visited_words']+=1
                if word&0x7c000000==0x14000000:
                    imm=word&0x3ffffff
                    dest=pc+4*(imm-(1<<26) if imm&(1<<25) else imm)
                    iscall=bool(word&0x80000000)
                    counts['direct_calls' if iscall else 'direct_branches']+=1
                    target(pc,dest,'BL' if iscall else 'B')
                    if not iscall:break
                    calls.add(dest)
                elif word&0xff000010==0x54000000 or word&0x7e000000 in (0x34000000,0x36000000):
                    bits=14 if word&0x7e000000==0x36000000 else 19
                    imm=(word>>5)&((1<<bits)-1)
                    dest=pc+4*(imm-(1<<bits) if imm&(1<<(bits-1)) else imm)
                    counts['conditional_branches']+=1
                    target(pc,dest,'conditional')
                elif word&0xfffffc1f in (0xd61f0000,0xd63f0000,0xd65f0000):
                    kind=word&0xfffffc1f
                    counts[{0xd61f0000:'indirect_branches',0xd63f0000:'indirect_calls',0xd65f0000:'returns'}[kind]]+=1
                    if kind!=0xd65f0000 and len(indirect)<100:
                        indirect.append(dict(pc=pc,register=(word>>5)&31,kind='BLR' if kind==0xd63f0000 else 'BR'))
                    if kind!=0xd63f0000:break
                elif word&0xffe0001f in (0xd4200000,0xd4400000) or word==0:
                    counts['trap_or_zero_stops']+=1
                    break
                elif word&0xffe0001f==0xd4000001:
                    counts['svc_sites']+=1
                pc+=4
        return dict(seed_count=len(seeds),counts=dict(counts),
            discovered_direct_call_targets=len(calls),external_direct_samples=unresolved,
            unresolved_indirect_samples=indirect,
            initializers={k:len(v) for k,v in initializers.items()},
            caveat='Seeded static traversal; unwind/relocation candidates may include data. Indirect targets and jump tables are not recovered; not runtime reachability or complete function recovery.')

    def report(self):
        return dict(status='offline linked image; no guest execution',
            rtld='verified input but excluded: relocation is performed offline',
            modules=[dict(name=m.name,base=m.base,size=len(m.image),build_id=m.build_id) for m in self.modules],
            relocations=dict(self.applied),unresolved_imports=dict(self.unresolved),
            unresolved_trampolines=self.trampolines,control_flow=self.discover())


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exefs_zip')
    p.add_argument('--out',required=True,type=Path)
    p.add_argument('--write-image',action='store_true')
    a=p.parse_args()
    image=Image(load_archive(a.exefs_zip))
    report=image.report()
    a.out.mkdir(parents=True,exist_ok=True)
    (a.out/'analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if a.write_image:
        (a.out/'image.bin').write_bytes(image.mem)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
