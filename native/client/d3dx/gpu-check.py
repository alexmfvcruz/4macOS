#!/usr/bin/env python3
"""Use the already-approved C3 dependencies for real DDS device uploads; installs nothing."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[3]
LOCAL=ROOT/'runtime/legacy'
sys.path.insert(0,str(ROOT/'native/client'))
import deps
OUT=LOCAL/'d3dx-check'
env=os.environ.copy()
env['DEVELOPER_DIR']='/Applications/Xcode.app/Contents/Developer'
env['PKG_CONFIG_PATH']=str(deps.SDL3_PREFIX/'lib/pkgconfig')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],env=env,text=True))
dxvk=deps.DXVK_D3D9_DIR
subprocess.run(['clang++','-std=c++14','-O1','-g','-ffp-contract=off','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DFOURSTORY_CLIENT_NATIVE','-I',str(ROOT/'native/client/d3d9/include'),'-I',str(ROOT/'native/client/compat/include'),'-I',str(ROOT/'native/client/d3dx/include'),str(ROOT/'native/client/d3dx/tests/gpu_upload.cpp'),str(dxvk/'libdxvk_d3d9.0.dylib'),*flags,'-o',str(OUT/'gpu-upload')],env=env,check=True)
env['DYLD_LIBRARY_PATH']=os.pathsep.join(map(str,deps.RUNTIME_LIBRARY_DIRS))
env['VK_DRIVER_FILES']=str(deps.MOLTENVK_ICD)
env['DXVK_WSI_DRIVER']='SDL3'
env['DXVK_STATE_CACHE_PATH']=str(OUT)
(OUT/'dxvk.conf').write_text('# pinned defaults\n');env['DXVK_CONFIG_FILE']=str(OUT/'dxvk.conf');env.pop('DXVK_CONFIG',None)
with (OUT/'gpu-upload.log').open('w') as log:
    subprocess.run([str(OUT/'gpu-upload'),*map(str,sorted((OUT/'assets').glob('*.dds')))],cwd=OUT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
text=(OUT/'gpu-upload.log').read_text()
if text.count('PASS real DXVK DDS texture/mip bytes')!=9 or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text: raise RuntimeError('incomplete or sanitizer-failed DDS upload test')
print('PASS nine real DXVK DDS mip uploads; BC3-to-ARGB creation; no probe sanitizer report')
