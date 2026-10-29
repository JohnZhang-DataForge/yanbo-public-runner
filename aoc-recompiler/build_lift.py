# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile/link generated functions with CPU dispatch; not a Switch OS runtime."""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess


def build(directory, jobs=2):
    directory=Path(directory).resolve()
    report=json.loads((directory/'lift.json').read_text())
    compiler=shutil.which('clang') or shutil.which('gcc')
    if not compiler:raise RuntimeError('Native C compiler required')
    objects=directory/'objects';objects.mkdir(exist_ok=True)
    # Preserve original generated headers. Diagnostic compilation keeps helper
    # calls out of line to bound GCC memory; this is not a performance build.
    headers=directory/'diagnostic_headers';headers.mkdir(exist_ok=True)
    for source in directory.glob('*.h'):
        text=source.read_text().replace('__attribute__((always_inline))','').replace('__forceinline','inline')
        text=re.sub(r'(?m)^RECOMP_INLINE ', 'static RECOMP_INLINE ', text)
        (headers/source.name).write_text(text)

    # Small function-aligned units avoid GCC's high memory cost on 4 MB units.
    parts=directory/'diagnostic_units';parts.mkdir(exist_ok=True)
    sources=[]
    for original in sorted(directory.glob('code_*.c')):
        text=original.read_text()
        starts=[m.start() for m in re.finditer(r'(?m)^void f_[0-9a-f]+\([^;\n]+\) \{',text)]
        if not starts:raise ValueError('No function definitions in '+str(original))
        prefix=text[:starts[0]]
        bodies=[text[start:end] for start,end in zip(starts,starts[1:]+[len(text)])]
        group=[];size=0;index=0
        for body in bodies:
            if group and size+len(body)>512*1024:
                target=parts/(original.stem+'_'+str(index)+'.c')
                target.write_text(prefix+''.join(group));sources.append(target)
                group=[];size=0;index+=1
            group.append(body);size+=len(body)
        if group:
            target=parts/(original.stem+'_'+str(index)+'.c')
            target.write_text(prefix+''.join(group));sources.append(target)
    sources += [directory/n for n in ('a64_table.c','a64_hooks.c','a64_suyu.c','a64_rt.c')]
    probe=directory/'link_probe.c'
    probe.write_text('''#include <stdio.h>
#include <stdint.h>
extern const uint32_t a64_fn_count;
int main(void) { printf("Native link check: %u lifted functions. No game entry executed; Switch OS/GPU runtime is not implemented.\\n",a64_fn_count); return 0; }
''')
    sources.append(probe)
    def compile_one(source):
        output=objects/(source.stem+'.o')
        log=objects/(source.stem+'.log')
        stamp=objects/(source.stem+'.sha256')
        identity=hashlib.sha256(compiler.encode()+Path(__file__).read_bytes()+source.read_bytes()+b''.join(p.read_bytes() for p in sorted(directory.glob('*.h')))).hexdigest()
        if output.exists() and stamp.exists() and stamp.read_text()==identity:
            return output
        command=[compiler,'-std=gnu11','-O0','-mcx16','-DA64_NO_MUSTTAIL',
            '-D_POSIX_C_SOURCE=200809L','-Werror=implicit-function-declaration','-include',str(headers/'a64.h'),'-I',str(headers),'-I',str(directory),'-c',str(source),'-o',str(output)]
        with log.open('w') as stream:
            result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError('Compilation failed: '+str(log))
        stamp.write_text(identity)
        return output
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures=[pool.submit(compile_one,source) for source in sources]
        for number,future in enumerate(concurrent.futures.as_completed(futures),1):
            try:
                future.result()
            except Exception:
                for pending in futures: pending.cancel()
                raise
            if number%10==0 or number==len(sources):print(f'Compiled {number}/{len(sources)} translation units',flush=True)
    executable=directory/('aoc-link-probe.exe' if os.name=='nt' else 'aoc-link-probe')
    response=directory/'objects.rsp'
    response.write_text('\n'.join('"'+str(objects/(source.stem+'.o')).replace('\\','/')+'"' for source in sources))
    command=[compiler,'@'+str(response),'-o',str(executable)]
    if os.name!='nt':command+=['-lm','-latomic']
    with (directory/'link.log').open('w') as stream:
        result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError('Link failed: '+str(directory/'link.log'))
    subprocess.run([str(executable)],check=True,timeout=15)
    report.update(native_linked=True,game_booted=False,probe=str(executable),build_mode='diagnostic, out-of-line helpers; no performance claim')
    (directory/'build.json').write_text(json.dumps(report,indent=2))
    return executable


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--jobs',type=int,default=2,choices=range(1,9))
    args=parser.parse_args()
    print(build(args.directory,args.jobs))
