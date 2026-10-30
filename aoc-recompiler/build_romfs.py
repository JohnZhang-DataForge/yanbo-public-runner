# SPDX-License-Identifier: GPL-2.0-or-later
"""Build a private read-only RomFS image from one ZIP subtree (default: romfs/)."""
import argparse
import os
from pathlib import Path, PurePosixPath
import stat
import struct
import zipfile

NONE = 0xffffffff

def align(n, size=4):
    return (n+size-1)&-size

def bucket_count(count):
    n=max(3,count|1)
    while any(n%d==0 for d in range(3,int(n**0.5)+1,2)):
        n+=2
    return n

def name_hash(parent, name, count):
    value=parent^123456789
    for byte in name:
        value=((value>>5)|(value<<27))&NONE
        value^=byte
    return value%count

def build(archive, output, prefix='romfs'):
    archive,output=Path(archive),Path(output)
    if archive.resolve()==output.resolve() or output.exists():
        raise ValueError('Output must be a new file distinct from the input')
    if not prefix or prefix.startswith('/') or any(p in ('','.','..') for p in prefix.split('/')):
        raise ValueError('Invalid archive prefix')
    prefix=prefix+'/'
    with zipfile.ZipFile(archive) as z:
        files={};dirs={''};seen=set()
        for info in z.infolist():
            if not info.filename.startswith(prefix):continue
            path=info.filename[len(prefix):].rstrip('/')
            if not path:continue
            parts=path.split('/')
            if any(p in ('','.','..') or '\\' in p or '\0' in p or len(p.encode('utf-8'))>255 for p in parts):
                raise ValueError('Invalid virtual path')
            if path in seen:raise ValueError('Duplicate virtual path: '+path)
            seen.add(path)
            if stat.S_ISLNK(info.external_attr>>16):raise ValueError('Symlinks are unsupported')
            for i in range(1,len(parts)):dirs.add('/'.join(parts[:i]))
            if info.is_dir():dirs.add(path)
            else:files[path]=info
        if not files:raise ValueError('No files in the selected archive subtree')
        if dirs & files.keys():raise ValueError('A virtual path is both a directory and a file')
        order=lambda s:s.encode('utf-8')
        dir_names=sorted(dirs,key=order);file_names=sorted(files,key=order)
        directory_offsets={};file_offsets={};data_offsets={};pos=0
        for name in dir_names:
            directory_offsets[name]=pos;pos+=24+align(len(PurePosixPath(name).name.encode()) if name else 0)
        directory_size=pos;pos=0
        for name in file_names:
            file_offsets[name]=pos;pos+=32+align(len(PurePosixPath(name).name.encode()))
        file_size=pos;pos=0
        for name in file_names:
            pos=align(pos,16);data_offsets[name]=pos;pos+=files[name].file_size
        nd,nf=bucket_count(len(dirs)),bucket_count(len(files))
        dh=[NONE]*nd;fh=[NONE]*nf;dt=bytearray(directory_size);ft=bytearray(file_size)
        children={d:[] for d in dirs};members={d:[] for d in dirs}
        parent=lambda n:n.rpartition('/')[0]
        for name in dir_names:
            if name:children[parent(name)].append(name)
        for name in file_names:members[parent(name)].append(name)
        sibling={}
        for group in list(children.values())+list(members.values()):
            for i,name in enumerate(group):sibling[name]=group[i+1] if i+1<len(group) else None
        for path in dir_names:
            off=directory_offsets[path];name=PurePosixPath(path).name.encode() if path else b''
            po=directory_offsets[parent(path)] if path else 0;bucket=name_hash(po,name,nd)
            sib=directory_offsets[sibling[path]] if path and sibling[path] is not None else NONE
            child=directory_offsets[children[path][0]] if children[path] else NONE
            member=file_offsets[members[path][0]] if members[path] else NONE
            struct.pack_into('<6I',dt,off,po,sib,child,member,dh[bucket],len(name))
            dt[off+24:off+24+len(name)]=name;dh[bucket]=off
        for path in file_names:
            off=file_offsets[path];name=PurePosixPath(path).name.encode();po=directory_offsets[parent(path)]
            bucket=name_hash(po,name,nf);sib=file_offsets[sibling[path]] if sibling[path] is not None else NONE
            struct.pack_into('<IIQQII',ft,off,po,sib,data_offsets[path],files[path].file_size,fh[bucket],len(name))
            ft[off+32:off+32+len(name)]=name;fh[bucket]=off
        dho=80;dto=dho+4*nd;fho=dto+len(dt);fto=fho+4*nf;data_start=align(fto+len(ft),4096)
        header=struct.pack('<10Q',80,dho,4*nd,dto,len(dt),fho,4*nf,fto,len(ft),data_start)
        output.parent.mkdir(parents=True,exist_ok=True);temporary=output.with_name(output.name+'.part')
        created=False
        try:
            with temporary.open('xb') as dst:
                created=True
                dst.write(header);dst.write(struct.pack('<%dI'%nd,*dh));dst.write(dt)
                dst.write(struct.pack('<%dI'%nf,*fh));dst.write(ft)
                for path in file_names:
                    dst.seek(data_start+data_offsets[path]);copied=0
                    with z.open(files[path]) as src:
                        while True:
                            chunk=src.read(4*1024*1024)
                            if not chunk:break
                            dst.write(chunk);copied+=len(chunk)
                    if copied!=files[path].file_size:raise ValueError('Truncated archive member')
                dst.truncate(data_start+pos)
            os.link(temporary,output)
            temporary.unlink()
        except BaseException:
            if created and temporary.exists():temporary.unlink()
            raise
    return dict(files=len(files),directories=len(dirs),size=output.stat().st_size)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',type=Path);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--prefix',default='romfs');args=parser.parse_args()
    print(build(args.archive,args.out,args.prefix))
