#!/usr/bin/env python3
"""Build/run the offline native audio tests; no game, server, or output device."""
import argparse, math, os, struct, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--assets',type=Path);a=p.parse_args()
out=ROOT/'runtime/legacy/c7-audio-tests'/('sanitized' if a.sanitize else 'plain');out.mkdir(parents=True,exist_ok=True)
fixture=out/'tone.wav'
with wave.open(str(fixture),'wb') as w:
 w.setparams((1,2,48000,0,'NONE','not compressed'))
 w.writeframes(b''.join(struct.pack('<h',int(12000*math.sin(i*2*math.pi*440/48000))) for i in range(48000)))
tail_fixture=out/'impulse-44100.wav'
with wave.open(str(tail_fixture),'wb') as w:
 w.setparams((1,2,44100,0,'NONE','not compressed'))
 w.writeframes(b''.join(struct.pack('<h',24000 if i<32 or i>=44036 else 0) for i in range(44100)))
silence_fixture=out/'tone-silence.wav'
with wave.open(str(silence_fixture),'wb') as w:
 w.setparams((1,2,48000,0,'NONE','not compressed'))
 w.writeframes(b''.join(struct.pack('<h',int(12000*math.sin(i*2*math.pi*440/48000))) if i<24000 else b'\0\0' for i in range(48000)))
mixed_fixture=out/'mixed.wav'
with wave.open(str(mixed_fixture),'wb') as w:
 w.setparams((1,2,48000,0,'NONE','not compressed'))
 w.writeframes(b''.join(struct.pack('<h',int(7000*(math.sin(i*2*math.pi*1000/48000)+math.sin(i*2*math.pi*12000/48000)))) for i in range(48000)))
flags=['-std=c++17','-g','-O1','-Wall','-Wextra','-Werror']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
subprocess.run(['clang++',*flags,'-fobjc-arc',str(ROOT/'native/client/audio/backend.mm'),str(ROOT/'native/client/audio/test_backend.cpp'),'-framework','AVFoundation','-o',str(out/'test')],check=True)
assets=sorted(a.assets.glob('*.TMD')) if a.assets else []
r=subprocess.run([str(out/'test'),str(fixture),str(tail_fixture),str(silence_fixture),str(mixed_fixture),*map(str,assets)])
raise SystemExit(r.returncode)
