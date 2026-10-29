# SPDX-License-Identifier: GPL-2.0-or-later
"""Link the single-thread OS diagnostic using a completed CPU diagnostic build."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parent


def build(directory):
    directory=Path(directory).resolve()
    info=json.loads((directory/'build.json').read_text())
    if not info.get('native_linked'):raise ValueError('Complete build_lift.py first')
    compiler=shutil.which('clang') or shutil.which('gcc')
    objects=directory/'objects';headers=directory/'diagnostic_headers'
    extra=[]
    for name in ('hos','boot_main'):
        target=objects/('os_'+name+'.o')
        subprocess.run([compiler,'-std=gnu11','-O0','-mcx16','-DA64_NO_MUSTTAIL',
            '-D_POSIX_C_SOURCE=200809L','-Werror=implicit-function-declaration',
            '-include',str(headers/'a64.h'),'-I',str(headers),'-I',str(directory),'-I',str(ROOT/'os'),
            '-c',str(ROOT/'os'/(name+'.c')),'-o',str(target)],check=True)
        extra.append(target)
    # Only use the objects belonging to the completed CPU build, excluding its main.
    original=(directory/'objects.rsp').read_text().splitlines()
    linked=[line for line in original if not line.strip('"').endswith('/link_probe.o')]
    response=directory/'os_objects.rsp'
    response.write_text('\n'.join(linked+['"'+str(p).replace('\\','/')+'"' for p in extra]))
    executable=directory/('aoc-os-diagnostic.exe' if os.name=='nt' else 'aoc-os-diagnostic')
    command=[compiler,'@'+str(response),'-o',str(executable)]
    if os.name!='nt':command+=['-lm','-latomic']
    subprocess.run(command,check=True)
    print(executable)
    return executable


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path)
    a=p.parse_args()
    build(a.directory)
