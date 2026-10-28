#!/usr/bin/env python3
"""Build C3 from the approved local dependencies. Never downloads or installs SDKs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
LOCAL = ROOT / 'runtime/legacy'
p = argparse.ArgumentParser()
p.add_argument('--deps', action='store_true', help='rebuild SDL3/local prefix and pinned DXVK first')
p.add_argument('--no-sanitize', action='store_true')
p.add_argument('--lod', action='store_true', help='build the analytic sampler LOD oracle')
p.add_argument('--backend', action='store_true', help='rebuild the patched approved DXVK backend')
p.add_argument('--assets', action='store_true', help='build original-loader static/character probe')
p.add_argument('--shaders', action='store_true', help='build original shader creation probe without object dependency')
a = p.parse_args()
env = os.environ.copy()
env['DEVELOPER_DIR'] = '/Applications/Xcode.app/Contents/Developer'
env['VULKAN_SDK'] = str(LOCAL / 'c3-sdk/1.4.363.0/macOS')
tools = LOCAL / 'c3-tools'
env['PATH'] = os.pathsep.join([str(tools/'cmake-3.31.9-macos-universal/CMake.app/Contents/bin'), str(tools/'ninja'), str(Path(env['VULKAN_SDK'])/'bin'), env['PATH']])
prefix = LOCAL / 'c3-prefix'
env['PKG_CONFIG_PATH'] = str(prefix/'lib/pkgconfig')
source = LOCAL/'c3-sources/dxvk-macos'
build = LOCAL/'c3-build'
dxvk_build = build/'dxvk-r2'
for item in json.loads((ROOT/'native/client/d3d9/origin.json').read_text())['files']:
    assert hashlib.sha256((ROOT/'native/client/d3d9'/item['file']).read_bytes()).hexdigest() == item['sha256'], item['file']
assert subprocess.check_output(['git','rev-parse','HEAD'], cwd=source, text=True).strip() == 'f66da559fd48cd55ddfebca38ed0f708c0f76a24'
patch=ROOT/'native/client/d3d9/patches/sampler-lod-bias.patch'
if subprocess.run(['git','apply','--reverse','--check',str(patch)],cwd=source,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode:
    subprocess.run(['git','apply','--check',str(patch)],cwd=source,check=True)
    subprocess.run(['git','apply',str(patch)],cwd=source,check=True)
expected=patch.read_bytes()
actual=subprocess.check_output(['git','diff','--binary'],cwd=source)
if actual!=expected:raise RuntimeError('DXVK source differs from the pinned patch')
backend_identity=build/'backend-identity.json'
backend_needs_build=not backend_identity.exists() or json.loads(backend_identity.read_text()).get('patch_sha256')!=hashlib.sha256(expected).hexdigest()
backend_library=dxvk_build/'src/d3d9/libdxvk_d3d9.0.dylib'
if not backend_needs_build and not a.backend and not a.deps:
    recorded_backend=json.loads(backend_identity.read_text())
    if not backend_library.exists() or hashlib.sha256(backend_library.read_bytes()).hexdigest()!=recorded_backend.get('binary_sha256'):
        raise RuntimeError('DXVK binary differs from recorded build; rebuild with --backend')


def run(args):
    subprocess.run([str(x) for x in args], cwd=ROOT, env=env, check=True)

if a.deps:
    run(['cmake','-S',LOCAL/'c3-sources/SDL3-3.4.18','-B',build/'sdl3','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DSDL_SHARED=ON','-DSDL_STATIC=OFF','-DSDL_TESTS=OFF','-DSDL_VULKAN=ON','-DSDL_RENDER=OFF','-DSDL_GPU=OFF'])
    run(['cmake','--build',build/'sdl3','--parallel','4'])
    run(['cmake','--install',build/'sdl3'])
    # SDL's generated .pc leaves paths unquoted, splitting this repo's space.
    pc = prefix/'lib/pkgconfig/sdl3.pc'
    text = pc.read_text().replace('-I${includedir}','-I"${includedir}"').replace('-L${libdir}','-L"${libdir}"').replace('-Wl,-rpath,${libdir}','-Wl,-rpath,"${libdir}"')
    pc.write_text(text)
    env.update(CC='clang', CXX='clang++', CFLAGS='-arch arm64 -mcpu=apple-m1', CXXFLAGS='-arch arm64 -mcpu=apple-m1', LDFLAGS='-arch arm64')
    configure = ['python3',LOCAL/'c3-sources/meson-1.9.1/meson.py','setup',dxvk_build,source,'--buildtype=release','--prefix='+str(prefix),'-Denable_d3d9=true','-Denable_d3d8=false','-Denable_dxgi=false','-Denable_d3d10=false','-Denable_d3d11=false','-Ddxvk_native_wsi=sdl3']
    if (dxvk_build/'build.ninja').exists():
        configure.append('--reconfigure')
    run(configure)
    run(['ninja','-C',dxvk_build,'-j4','src/d3d9/libdxvk_d3d9.0.dylib'])

if a.backend or a.deps or backend_needs_build:
    run(['ninja','-C',dxvk_build,'-j4','src/d3d9/libdxvk_d3d9.0.dylib'])
    backend_identity.write_text(json.dumps({'upstream_commit':'f66da559fd48cd55ddfebca38ed0f708c0f76a24','patch_sha256':hashlib.sha256(expected).hexdigest(),'binary_sha256':hashlib.sha256((dxvk_build/'src/d3d9/libdxvk_d3d9.0.dylib').read_bytes()).hexdigest()},indent=2)+'\n')

if a.assets:
    # Reuse the owned C2 build entrypoint; never edit its files here.
    resource_args=['python3',ROOT/'native/client/build/build.py','--target','resources']
    if a.no_sanitize: resource_args.append('--no-sanitize')
    run(resource_args)

flags = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'], env=env, text=True))
output = build / ('plain' if a.no_sanitize else 'sanitized')
output.mkdir(parents=True, exist_ok=True)
command = ['clang++','-std=c++17','-arch','arm64','-O1','-g','-fno-omit-frame-pointer','-DFOURSTORY_CLIENT_NATIVE','-DFOURSTORY_CLIENT_64','-DFOURSTORY_CLIENT_MOD','-I',ROOT/'native/client/d3d9/include','-I',ROOT/'native/client/compat/include',ROOT/'native/client/c3/probe.cpp',dxvk_build/'src/d3d9/libdxvk_d3d9.0.dylib',*flags,'-o',output/'c3-probe']
if not a.no_sanitize:
    command += ['-fsanitize=address,undefined','-fno-sanitize-recover=all']
run(command)
print('PASS C3 probe build:', output/'c3-probe')
checked_command=[(ROOT/'native/client/c3/checked_probe.cpp' if x==ROOT/'native/client/c3/probe.cpp' else (output/'c3-checked' if x==output/'c3-probe' else x)) for x in command]
run(checked_command)
print('PASS real-device observer probe build:',output/'c3-checked')
if a.lod:
    fixture_dir=ROOT/'native/client/c3/lod-shaders'
    fixture_manifest=json.loads((fixture_dir/'manifest.json').read_text(encoding='utf-8-sig'))
    for entry in fixture_manifest['files']+fixture_manifest['sources']:
        if hashlib.sha256((fixture_dir/entry['name']).read_bytes()).hexdigest().upper()!=entry['SHA256']:raise RuntimeError('LOD fixture changed: '+entry['name'])
    lod_command=[(ROOT/'native/client/c3/lod_probe.cpp' if x==ROOT/'native/client/c3/probe.cpp' else (output/'c3-lod' if x==output/'c3-probe' else x)) for x in command]
    run(lod_command)
    print('PASS sampler LOD oracle build:',output/'c3-lod')


if a.assets or a.shaders:
    import tempfile
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        generated=Path(temporary)/'checked_device.inc'
        run(['python3',ROOT/'native/client/c3/generate_checked_device.py',generated])
        if generated.read_bytes()!=(ROOT/'native/client/c3/checked_device.inc').read_bytes():
            raise RuntimeError('checked_device.inc is stale')
    manifest=json.loads((ROOT/'native/client/reference/shaders/manifest.json').read_text())
    for shader in manifest['shaders']:
        path=ROOT/'native/client/reference/shaders'/(shader['shader']+'.bin')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=shader['code_sha256']:
            raise RuntimeError('original shader capture changed: '+shader['shader'])
    lib=ROOT/'source/4Story 5.0 Source Files/Lib/Own'
    includes=[ROOT/'native/client/d3d9/include',ROOT/'native/client/compat/include',ROOT/'native/client/d3dx/include',ROOT/'native/client/build/include',lib/'Engine Lib/Engine Lib',lib/'TProtocol/include',lib/'TCML/TCML',lib/'TChart/TChart']
    compile_flags=['-std=c++17','-arch','arm64','-O1','-g','-ffp-contract=off','-fno-omit-frame-pointer','-DFOURSTORY_CLIENT_NATIVE','-DFOURSTORY_CLIENT_NATIVE_LOADER','-DFOURSTORY_CLIENT_64','-DFOURSTORY_CLIENT_MOD','-include','fourstory_client_prefix.h','-Wno-address-of-temporary','-Wno-nonportable-include-path','-Wno-c++11-narrowing']
    for directory in includes: compile_flags+=['-I',directory]
    if not a.no_sanitize: compile_flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    sdl_compile=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl3'],env=env,text=True))
    run(['clang++',*compile_flags,ROOT/'native/client/c3/shader_probe.cpp',ROOT/'native/client/c3/device_context.cpp',dxvk_build/'src/d3d9/libdxvk_d3d9.0.dylib',*flags,'-o',output/'c3-original-shaders'])
    print('PASS original shader probe build:',output/'c3-original-shaders')
    if not a.assets: raise SystemExit(0)
    objects=[]
    # Compile our own scene first, so missing original-object support is separate.
    for name,path in [('asset-probe',ROOT/'native/client/c3/asset_probe.cpp'),('asset-device',ROOT/'native/client/c3/device_context.cpp'),('original-object',lib/'Engine Lib/Engine Lib/TachyonObject.cpp')]:
        obj=output/(name+'.o')
        run(['clang++',*compile_flags,*sdl_compile,'-c',path,'-o',obj]);objects.append(obj)
    resources=LOCAL/'native-client'/('plain' if a.no_sanitize else 'sanitized')
    run(['clang++',*compile_flags,*objects,resources/'libresources.a',resources/'libengine.a',dxvk_build/'src/d3d9/libdxvk_d3d9.0.dylib',*flags,'-lz','-Wl,-dead_strip','-o',output/'c3-assets'])
    print('PASS C3 original-loader asset probe build:',output/'c3-assets')
