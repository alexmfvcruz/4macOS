#!/usr/bin/env python3
"""Run the two original-loader scenes; preserve images, timings and failures."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--assets',type=Path);p.add_argument('--scene',action='append',choices=['static','character','character-ffp']);p.add_argument('--output',type=Path);p.add_argument('--config',type=Path);p.add_argument('--shaders-only',action='store_true');p.add_argument('--plain',action='store_true');p.add_argument('--verify-only',action='store_true');a=p.parse_args()
local=ROOT/'runtime/legacy';out=a.output.resolve() if a.output else local/('c3-assets-plain' if a.plain else 'c3-assets-sanitized');out.mkdir(parents=True,exist_ok=True)
exe=local/'c3-build'/('plain' if a.plain else 'sanitized')/'c3-assets'
backend_path=local/'c3-build/dxvk-r2/src/d3d9/libdxvk_d3d9.0.dylib'
backend_sha=hashlib.sha256(backend_path.read_bytes()).hexdigest()
env=os.environ.copy();env['DYLD_LIBRARY_PATH']=os.pathsep.join(map(str,[local/'c3-runtime/MoltenVK-1.4.2/MoltenVK/MoltenVK/dynamic/dylib/macOS',local/'c3-sdk/1.4.363.0/macOS/lib',local/'c3-prefix/lib',local/'c3-build/dxvk-r2/src/d3d9']))
env['VK_DRIVER_FILES']=str(local/'c3-runtime/MoltenVK-1.4.2/local-icd.json');env['DXVK_WSI_DRIVER']='SDL3';env['DXVK_STATE_CACHE_PATH']=str(out);env['DXVK_CONFIG_FILE']=str(out/'dxvk.conf');env.pop('DXVK_CONFIG',None)
if not a.verify_only:(out/'dxvk.conf').write_text(a.config.read_text() if a.config else '# pinned backend defaults\n')
elif a.config and a.config.read_bytes()!=(out/'dxvk.conf').read_bytes():raise RuntimeError('verification config differs from executed config')
spec=importlib.util.spec_from_file_location('c3_compare',ROOT/'native/client/c3/compare.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
if a.shaders_only:
    shader_exe=exe.with_name('c3-original-shaders')
    log_path=out/'shaders.log'
    if a.verify_only:
        recorded=json.loads((out/'shaders.json').read_text())
        if recorded.get('binary_sha256')!=hashlib.sha256(shader_exe.read_bytes()).hexdigest() or recorded.get('source_sha256')!=hashlib.sha256((ROOT/'native/client/c3/asset_shaders.h').read_bytes()).hexdigest() or recorded.get('backend_sha256')!=backend_sha:raise RuntimeError('shader verification requires matching execution identity')
    if not a.verify_only:
        with log_path.open('w') as log:subprocess.run([str(shader_exe),str(ROOT/'native/client/reference/shaders')],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    text=log_path.read_text()
    if 'PASS original shader creation: 2 vertex, 24 pixel, 2 declarations;' not in text or 'Present parameters:' not in text or 'ERROR: AddressSanitizer' in text or 'runtime error:' in text:raise RuntimeError('original shader preflight incomplete')
    result={'scope':'creation only, original object rendering not tested','vertex_shaders':2,'pixel_shaders':24,'declarations':2,'source_sha256':hashlib.sha256((ROOT/'native/client/c3/asset_shaders.h').read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(shader_exe.read_bytes()).hexdigest(),'backend_sha256':backend_sha,'log_sha256':hashlib.sha256(log_path.read_bytes()).hexdigest()}
    (out/'shaders.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS 26 original shaders and 2 declarations on real device; drawing NOT TESTED');raise SystemExit(0)
if a.assets is None:p.error('--assets is required for original object scenes')
scenes=a.scene or ['static','character','character-ffp']
results=[]
binary_sha=hashlib.sha256(exe.read_bytes()).hexdigest()
backend_sha=hashlib.sha256(backend_path.read_bytes()).hexdigest()
identity={'binary_sha256':binary_sha,'backend_sha256':backend_sha,'config_sha256':hashlib.sha256((out/'dxvk.conf').read_bytes()).hexdigest(),'scenes':scenes,'forced_lod_bias':env.get('C3_FORCE_LOD_BIAS'),'diagnostics':bool(env.get('C3_DIAGNOSTICS'))}
if a.verify_only:
    recorded=json.loads((out/'run-status.json').read_text())
    if recorded.get('status')!='PASS' or any(recorded.get(k)!=v for k,v in identity.items()):
        raise RuntimeError('verification requires a successful execution of this exact binary')
else:
    (out/'run-status.json').write_text(json.dumps({'status':'RUNNING',**identity})+'\n')
try:
    for scene in scenes:
        prefix=out/scene
        if not a.verify_only:
            with (out/(scene+'.log')).open('w') as log:subprocess.run([str(exe),str(prefix),scene,str(a.assets.resolve()),str(ROOT/'native/client/reference/shaders'),'120','30'],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        width,height,rgb,digest=m.read_ppm(prefix.with_suffix('.ppm'))
        if (width,height)!=(640,480):raise RuntimeError('unexpected dimensions')
        if prefix.with_suffix('.ppm').read_bytes()!=(out/(scene+'-first.ppm')).read_bytes():raise RuntimeError('fixed scene changed across frames')
        foreground=sum(rgb[i:i+3]!=bytes([32,48,64]) for i in range(0,len(rgb),3))
        if foreground<1000:raise RuntimeError('scene is blank or too small')
        log=(out/(scene+'.log')).read_text()
        if 'PASS original '+scene+' draw, readback and Present;' not in log or 'failed_device_calls=0' not in log:raise RuntimeError('missing runtime success/checked-call marker')
        if 'Present parameters:' not in log:raise RuntimeError('missing actual presentation metadata')
        if 'ERROR: AddressSanitizer' in log or 'runtime error:' in log:raise RuntimeError('sanitizer diagnostic')
        timing=[float(x.split('\t')[1]) for x in (out/(scene+'-timing.tsv')).read_text().splitlines()[1:]]
        if len(timing)!=119 or any(not math.isfinite(x) or x<0 for x in timing):raise RuntimeError('invalid timing samples')
        (out/(scene+'.png')).write_bytes(m.png(rgb,width,height))
        results.append({'scene':scene,'ppm_sha256':digest,'foreground_pixels':foreground,'frozen_first_last_equal':True,'timed_frames':len(timing),'cpu_draw_present_median_ms':statistics.median(timing),'cpu_draw_present_p95_ms':sorted(timing)[int(.95*(len(timing)-1))],'scope':'CPU draw/Present wall time, first/readback frames excluded; not GPU timing or game FPS','windows_comparison':'pending'})
    (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');
    if not a.verify_only:(out/'run-status.json').write_text(json.dumps({'status':'PASS',**identity})+'\n')
    print('PASS original native runtime/readback: '+', '.join(scenes)+'; Windows comparison is separate')
except Exception as error:
    (out/'run-status.json').write_text(json.dumps({'status':'FAILED','error':str(error),**identity})+'\n')
    raise
