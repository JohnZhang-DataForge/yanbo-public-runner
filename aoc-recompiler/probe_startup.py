# SPDX-License-Identifier: GPL-2.0-or-later
"""Private, bounded base-game startup probe. Generated outputs contain game code.

Never upload this tool's output directory to a public runner or repository.
Completing these stages does not establish game boot or playability.
"""
import argparse
import sys,shutil,json,os
from pathlib import Path

from aoc_image import Image,load_archive
from aoc_lift import LiftImage,lifter_modules,VENDOR
from startup import plan,header
from build_lift import build
from build_os import build as build_os
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('exefs_zip')
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--romfs',type=Path)
parser.add_argument('--run-timeout',type=int,default=30)
parser.add_argument('--stages',type=int,default=10)
parser.add_argument('--seed',type=lambda s:int(s,0),action='append',default=[])
parser.add_argument('--max-functions',type=int,default=8192)
parser.add_argument('--jobs',type=int,choices=range(1,9),default=4)
args=parser.parse_args()
if args.stages<1 or args.max_functions<1 or args.run_timeout<1:parser.error('Limits must be positive')
# A dedicated marker prevents accidental overwrites of unrelated directories.
marker=args.out/'private-startup-probe.json'
if args.out.exists() and any(args.out.iterdir()) and not marker.exists():
    parser.error('Use an empty or previously marked private output directory')
args.out.mkdir(parents=True,exist_ok=True)
marker.write_text(json.dumps(dict(private_game_output=True,game_booted=False)))
if args.romfs and not args.romfs.is_file():parser.error('RomFS image not found')
image=Image(load_archive(args.exefs_zip))
adapted=LiftImage(image);mod,rt=lifter_modules();lifter=mod.Lifter(adapted)
manifest=plan(image)
_,initializers=image.seeds()
constructor_entries=[a for group in initializers.values() for a in group if image.locate(a) is not None]
pending=constructor_entries+args.seed+[s['address'] for s in manifest['stages'][:args.stages]]+[0xb362e00,0xb24dbc0,0xb2d2680,0xb2d61f0,0xb2d6660,0xb6dcf60,0xb6da388,0xb464000,0xb2e35d0,0xb2e35f0,0xb2e2d10,0xb4655f0,0xb465670,0xb280bd0,0xb280c50,0xb465850,0xb4658d0,0xb465600,0xb256300,0xb2d6ac0,0xb2d6590,0xb33e0c0,0xb33e0d0,0xb33b980,0xb336350,0xb336910,0xb336860,0xb336a40,0xb3374e0,0xb3362e0,0xb3362f0,0xb2d4e20,0xb2d4880,0xb2d4d50,0xb2d01a0,0xb2ce1b0,0xb2d1a80,0xb2d1e60,0xb2d1ce0,0xb2d0be0,0xb2d1cf0,0xb2cfb70,0xb49d970,0xb49db50,0xb49e970,0xb2e2d20,0xb2a2640,0xb434280,0xb256eb0,0xb71e958,0xb6de520,0xb722a58,0x800a0d0,0x800a160,0x800cb20,0xb5a4720,0xb599180,0xb715230,0xb6da460,0xb71dbf0,0xb4436b0,0xb4d06d0,0xb60eec0,0xb693b10,0xb6966c0,0xb697130,0xb342710,0x8a328e0,0x8a33240,0x8a32a70,0xb59c240,0xb59c0f0,0xb59c6b0,0xb59c030,0xb59c430,0xb59c650,0xb4d19b0,0xb4d1c10,0xb4d2960,0xb4d2bd0,0xb4d1df0,0xb4d19c0,0xb4d2900,0xb4d4620,0xb4d4630,0xb2a3af0,0xb2a27d0,0x8a32e80,0x994c990,0x994cfb0,0xb281710,0xb281730,0xb26d400,0xb26d360,0xb26f5f0,0xb26d370,0xb26ea30,0x95f9ce0,0xb273750,0xb273860,0xb2736c0,0xb2736d0,0xb2946e0,0xb273760,0xb294690,0xb273790,0xb2737c0,0xb2737e0,0xb273800,0xb273820,0xb273850,0xad5b6a0,0x95f9d30,0xb294660,0xb294720,0xb294760,0xb294790,0xb2947c0,0xb2947f0,0x994cd90,0x994c7b0,0x994c7c0,0x994c7d0,0x994c850,0x994c8a0,0x994c940,0x994c9f0,0x994ca70,0x994cae0,0x994cb70,0x994cc70,0x994cde0,0x994ced0,0x994cf10,0x994cf50,0x994cfc0,0x994d020,0x994d030,0x994d040,0x95fa950,0x88128b0,0x8df3f60,0x8df3360,0x994d530,0x8df46e0,0x88228e0,0x8df4850]+[a for a in adapted.entries if (0xb4dd000<=a<0xb4df000 or 0xb320000<=a<0xb340000 or 0xb2c0000<=a<0xb2d8000 or 0xb49d000<=a<0xb4a0000 or 0xb6da000<=a<0xb6db000 or 0xb71c000<=a<0xb71e000 or 0xb59c000<=a<0xb59d000)];selected={}
while pending and len(selected)<args.max_functions:
 address=pending.pop()
 if address in selected:continue
 body,labels=lifter.walk(address);selected[address]=(body,labels)
 target=lifter.plt_target(address)
 if target is not None and target[0] not in selected:pending.append(target[0])
 for pc,(kind,word) in body.items():
  for target in list(kind[1:])+([pc+4] if pc+4 not in body and kind[0] not in ('ret','b','br','trap') else []):
   if isinstance(target,int) and target in lifter.entries and target not in selected:pending.append(target)
if pending:raise RuntimeError('Diagnostic function limit reached')
lifter.funcs=selected
out=args.out.resolve()
# Remove stale generated units before a smaller or differently seeded re-lift.
for stale in out.glob('code_*.c'):stale.unlink()
files=lifter.write(str(out),chunk_bytes=512*1024);rt.write(str(out))
for n in ('a64.h','a64_rt.c'):shutil.copyfile(VENDOR/'rt'/n,out/n)
(out/'image.bin').write_bytes(image.mem)
(out/'hos_startup.h').write_text(header(manifest))
(out/'startup.json').write_text(json.dumps(manifest,indent=2))
(out/'lift.json').write_text(json.dumps(dict(functions=len(selected),native_linked=False,game_booted=False)))
print('BOUNDED SLICE',len(selected),'pending',len(pending),flush=True)
build(out,jobs=args.jobs);exe=build_os(out)
import subprocess
p=subprocess.run([str(exe),str(out/'image.bin')],capture_output=True,text=True,timeout=args.run_timeout,
    env=dict(os.environ,**({'HOS_ROMFS_PATH':str(args.romfs.resolve())} if args.romfs else {})))
(out/'startup-run.log').write_text(p.stdout+p.stderr)
print('EXIT',p.returncode,p.stdout,p.stderr,flush=True)

sys.exit(p.returncode)
