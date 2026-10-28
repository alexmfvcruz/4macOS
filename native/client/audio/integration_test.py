#!/usr/bin/env python3
"""Link the real client objects with an offline test entry point; never starts the game."""
import argparse, json, math, os, struct, subprocess, sys, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'native/client/app'))
from build_freshness import validate_build
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--build',type=Path,help='test an isolated candidate output directory');args=p.parse_args()
out=args.build.resolve() if args.build else ROOT/'build/client'/('c4-sanitized' if args.sanitize else 'c4-plain')
validate_build(out)
flags=(out/'obj/shell-sound_native.cpp.flags').read_text().splitlines()
client=ROOT/'source/4Story 5.0 Source Files/Client/TClient'
flags += ['-iquote',str(client),'-I',str(client.parent/'History Files'),'-I',str(ROOT/'source/4Story 5.0 Source Files/Lib/Own/HwidLib/HwidLib'),'-DNEW_IF','-DUSE_HWID_AUTH']
source=ROOT/'native/client/audio/tests/integration.cpp';obj=out/'audio-integration.o'
subprocess.run(['clang++',*flags,'-c',str(source),'-o',str(obj)],check=True)
record=json.loads((out/'client-build.json').read_text())
objects=[str(Path(f).with_suffix('.o')) for f in record['inputs'] if f.endswith('.flags') and Path(f).name!='app-main.cpp.flags']
objects += [str(out/('accounts-'+name+'.o')) for name in ('artwork',)]
objects.append(str(ROOT/'build/accounts/release/libfourstory_account_bridge.a'))
sys.path.insert(0,str(ROOT/'native/client'))
import deps
sdl=Path(os.environ.get('FOURSTORY_SDL3_PREFIX',deps.SDL3_PREFIX))
dxvk=Path(os.environ.get('FOURSTORY_DXVK_D3D9',deps.DXVK_D3D9))
frameworks=['AVFoundation','AppKit','Carbon','ApplicationServices','CoreText','CoreGraphics','CoreFoundation','IOKit','Foundation','ImageIO','Security','SystemConfiguration']
link=['clang++','-arch','arm64',*(['-fsanitize=address,undefined'] if args.sanitize else []),str(obj),*objects,str(dxvk),'-L',str(sdl/'lib'),'-lSDL3','-Wl,-rpath,'+str(sdl/'lib'),'-Wl,-rpath,'+str(dxvk.parent),'-lz','-liconv']
for f in frameworks:link+=['-framework',f]
binary=out/'audio-integration-test';subprocess.run([*link,'-o',str(binary)],check=True)
fixtures=[]
for channels in (1,2):
 f=out/f'audio-test-{channels}.wav';fixtures.append(f)
 with wave.open(str(f),'wb') as w:
  w.setparams((channels,2,48000,0,'NONE','not compressed'))
  w.writeframes(b''.join(struct.pack('<h',int(12000*math.sin(i*2*math.pi*440/48000)))*channels for i in range(48000)))
subprocess.run([str(binary),*map(str,fixtures)],check=True)
