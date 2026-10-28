#!/usr/bin/env python3
"""Build the original C1 libraries on macOS. No Windows or server side effects."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / 'source/4Story 5.0 Source Files/Lib/Own'
p = argparse.ArgumentParser()
p.add_argument('--target', choices=['tcml', 'chart', 'engine', 'session', 'device', 'resources', 'tables', 'all'], default='all')
p.add_argument('--no-sanitize', action='store_true')
a = p.parse_args()
OUT = ROOT / 'build/client' / ('plain' if a.no_sanitize else 'sanitized')
OUT.mkdir(parents=True, exist_ok=True)
flags = ['-std=gnu++14', '-arch', 'arm64', '-ffp-contract=off', '-Wno-address-of-temporary', '-g', '-O1', '-fno-omit-frame-pointer',
         '-DFOURSTORY_CLIENT_NATIVE', '-DFOURSTORY_CLIENT_64', '-DFOURSTORY_CLIENT_MOD',
         # C1/C2 programs have no window or renderer (loader stand-ins; C4 client: native/client/app).
         '-DFOURSTORY_CLIENT_NATIVE_LOADER',
         '-include', 'fourstory_client_prefix.h', '-Wno-missing-declarations',
         '-Wno-nonportable-include-path', '-Wno-ignored-pragmas']
if not a.no_sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
for directory in [ROOT/'native/client/compat/include', ROOT/'native/client/d3dx/include', ROOT/'native/client/build/include',
                  LIB/'TProtocol/include', LIB/'TCML/TCML', LIB/'TChart/TChart',
                  LIB/'Engine Lib/Engine Lib']:
    flags += ['-I', str(directory)]
modules = {'tcml': ('TCML/TCML', ['cinterf.cpp','TCML.cpp','TCML.l.cpp','TCMLParser.cpp']),
           'chart': ('TChart/TChart', ['stdafx.cpp','TChart.cpp']),
           'engine': ('Engine Lib/Engine Lib', ['TBitStream.cpp','TachyonCompressor.cpp',
                     'Packet.cpp','TMath.cpp','TBSPPortal.cpp','TBSPNode.cpp']),
           'session': ('Engine Lib/Engine Lib', ['TachyonSession.cpp','CryptographyExt.cpp'])}
selected = list(modules) if a.target == 'all' else (['tcml','chart'] if a.target == 'tables' else (['engine','session'] if a.target == 'session' else (['engine'] if a.target == 'resources' else ([] if a.target == 'device' else [a.target]))))
for name in selected:
    directory, sources = modules[name]
    objects = []
    for source in sources:
        obj = OUT / (name+'-'+source+'.o')
        print('Compile', name+'/'+source, flush=True)
        result = subprocess.run([os.environ.get('CXX','clang++'), *flags, '-c', str(LIB/directory/source), '-o', str(obj)])
        if result.returncode: sys.exit(result.returncode)
        objects.append(str(obj))
    subprocess.run(['ar', 'rcs', str(OUT/('lib'+name+'.a')), *objects], check=True)
if selected: print('PASS native libraries:', OUT, flush=True)

if a.target in ['all','device','resources']:
    # C2 empty device: the shared d3d9.h comes before the compatibility headers (contract item 11).
    device = ROOT/'native/client/device'
    dflags = [f for f in flags]
    dflags[dflags.index('-I'):dflags.index('-I')] = ['-I', str(ROOT/'native/client/d3d9/include')]
    obj = OUT/'device-empty_device.cpp.o'
    if subprocess.run([os.environ.get('CXX','clang++'), *dflags, '-Wall', '-Wextra', '-c', str(device/'empty_device.cpp'), '-o', str(obj)]).returncode: sys.exit(1)
    subprocess.run(['ar', 'rcs', str(OUT/'libdevice.a'), str(obj)], check=True)
    if subprocess.run([os.environ.get('CXX','clang++'), *dflags, str(device/'tests/empty_device_test.cpp'), str(OUT/'libdevice.a'),
                       '-o', str(OUT/'c2-empty-device-test')]).returncode: sys.exit(1)
    print('PASS native device library and test executable:', OUT, flush=True)

if a.target in ['all','resources']:
    # C2 resource loaders: shared d3d9.h first (contract item 11), all errors reported before stopping.
    rflags = [f for f in flags]
    rflags[rflags.index('-I'):rflags.index('-I')] = ['-I', str(ROOT/'native/client/d3d9/include')]
    # MSVC accepts narrowing in brace initializers with warning C4838 (D3DImage.cpp); keep that meaning.
    rflags += ['-Wno-c++11-narrowing', '-ferror-limit=0']
    # TachyonObject.cpp (object sounds, window messages) is play-time code; the loaders do not use it.
    sources = ['TachyonRes.cpp','T3DTexture.cpp','D3DImage.cpp','TachyonAnimation.cpp','TachyonMesh.cpp',
               'TachyonSFX.cpp','TachyonSlashSFX.cpp','TachyonMAP.cpp','TachyonBSPMAP.cpp','TachyonEXTMAP.cpp','TachyonHUGEMAP.cpp',
               'D3DCamera.cpp','D3DLight.cpp']
    objects, failed = [], []
    for source in sources:
        obj = OUT/('resources-'+source+'.o')
        print('Compile resources/'+source, flush=True)
        if subprocess.run([os.environ.get('CXX','clang++'), *rflags, '-c', str(LIB/'Engine Lib/Engine Lib'/source), '-o', str(obj)]).returncode: failed.append(source)
        else: objects.append(str(obj))
    if failed: print('FAIL resources:', ' '.join(failed), flush=True); sys.exit(1)
    subprocess.run(['ar', 'rcs', str(OUT/'libresources.a'), *objects], check=True)
    print('PASS native resource loader library:', OUT, flush=True)
    smoke = ROOT/'native/client/tests/resources'
    command = [os.environ.get('CXX','clang++'), *rflags, str(smoke/'smoke.cpp'), str(smoke/'device_context.cpp'),
               str(OUT/'libresources.a'), str(OUT/'libengine.a'), str(OUT/'libdevice.a'), '-lz', '-Wl,-dead_strip', '-o', str(OUT/'c2-resource-smoke')]
    if subprocess.run(command).returncode: sys.exit(1)
    print('PASS native resource smoke executable:', OUT, flush=True)
    command[command.index(str(smoke/'smoke.cpp'))] = str(smoke/'dump.cpp')
    command[-1] = str(OUT/'c2-resource-dump')
    if subprocess.run(command).returncode: sys.exit(1)
    print('PASS native resource dump executable:', OUT, flush=True)


if a.target in ['all','session']:
    client = ROOT/'source/4Story 5.0 Source Files/Client/TClient'
    # -iquote: Client/TClient has its own unistd.h, which must not replace the system header.
    command = [os.environ.get('CXX','clang++'), *flags, '-iquote', str(client),
               str(ROOT/'native/client/login-probe/login_probe.cpp'), str(client/'SHA1.cpp'),
               str(OUT/'libsession.a'), str(OUT/'libengine.a'), '-lz', '-o', str(OUT/'c2-login-probe')]
    if subprocess.run(command).returncode: sys.exit(1)
    command = [os.environ.get('CXX','clang++'), *flags, '-include', 'functional', str(ROOT/'native/client/tests/session_test.cpp'),
               str(OUT/'libsession.a'), str(OUT/'libengine.a'), '-lz', '-o', str(OUT/'c2-session-test')]
    if subprocess.run(command).returncode: sys.exit(1)
    print('PASS native session executables:', OUT, flush=True)

if a.target in ['all','tables']:
    programs = [('c1-tcml-stack-test', ROOT/'native/client/tests/tcml_stack_test.cpp', ['tcml']),
                ('c1-dump', ROOT/'native/client/tests/dump/dump.cpp', ['chart','tcml']),
                ('c1-startup-dump', ROOT/'native/client/tests/dump/startup_dump.cpp', ['chart','tcml']),
                ('c1-chart-transform-test', ROOT/'native/client/tests/chart_transform_test.cpp', ['chart','tcml']),
                ('c1-dump-format-test', ROOT/'native/client/tests/dump/dump_format_test.cpp', ['chart','tcml']),
                ('c1-engine-test', ROOT/'native/client/tests/engine_test.cpp', ['engine']),
                ('c1-d3dx-test', ROOT/'native/client/tests/d3dx_test.cpp', [])]
    for name, source, dependencies in programs:
        if a.target == 'tables' and name == 'c1-engine-test': continue
        command = [os.environ.get('CXX','clang++'), *flags, str(source)]
        command += [str(OUT/('lib'+m+'.a')) for m in dependencies]
        command += ['-lz','-o',str(OUT/name)]
        result = subprocess.run(command)
        if result.returncode: sys.exit(result.returncode)
    print('PASS native test executables:', OUT, flush=True)
