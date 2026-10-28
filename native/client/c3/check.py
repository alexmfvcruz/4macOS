#!/usr/bin/env python3
"""Run native C3 scenes, verify every pixel, preserve diagnostics and timings."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser()
p.add_argument('--verify-only', action='store_true')
p.add_argument('--plain', action='store_true')
p.add_argument('--checked', action='store_true', help='run analytical scenes through the real-device observer')
p.add_argument('--resources', action='store_true', help='texture formats/mips and resource lifecycle')
p.add_argument('--features', action='store_true', help='synthetic R1 skinning/high-constant preflights')
a = p.parse_args()
if a.features and a.resources: p.error('--features and --resources are mutually exclusive')
local = ROOT/'runtime/legacy'
output = local/(('c3-resources' if a.resources else ('c3-features' if a.features else 'c3-results'))+('-plain' if a.plain else '')+('-checked' if a.checked else ''))
output.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
mvk = local/'c3-runtime/MoltenVK-1.4.2/MoltenVK/MoltenVK/dynamic/dylib/macOS'
sdk = local/'c3-sdk/1.4.363.0/macOS'
dxvk = local/'c3-build/dxvk-r2/src/d3d9'
env['VK_DRIVER_FILES'] = str(local/'c3-runtime/MoltenVK-1.4.2/local-icd.json')
# MoltenVK goes first: dyld otherwise resolves its install name into SDK/lib.
env['DYLD_LIBRARY_PATH'] = os.pathsep.join(map(str,[mvk,sdk/'lib',local/'c3-prefix/lib',dxvk]))
env['DXVK_WSI_DRIVER'] = 'SDL3'
env['DXVK_STATE_CACHE_PATH'] = str(output/'cache')
(output/'cache').mkdir(exist_ok=True)
(output/'dxvk.conf').write_text('# C3 uses the pinned backend defaults.\n')
env['DXVK_CONFIG_FILE'] = str(output/'dxvk.conf')
env.pop('DXVK_CONFIG',None)
binary = local/'c3-build'/('plain' if a.plain else 'sanitized')/('c3-checked' if a.checked else 'c3-probe')


def expected(scene,x,y):
    if scene == 'formats':
        for item in range(10):
            left,top=32+112*(item%5),48+160*(item//5)
            if left <= x < left+96 and top <= y < top+64:
                level=(x-left)//24
                has_alpha=item not in (1,3)
                passes=not has_alpha or level<2 or (item==5 and level==3)
                color=(255,255,255) if item==4 else ((255,0,0),(0,255,0),(0,0,255),(255,255,255))[level]
                return (color if passes else (32,48,64)),0
        return (32,48,64),0
    if scene == 'lifecycle':
        if 32 <= x < 96 and 440 <= y < 472:
            return ((224,64,32) if ((x-32)//16+(y-440)//8)%2==0 else (32,48,64)),0
        return expected('rtt',x,y)
    if scene == 'skinning':
        if 220 <= x < 300 and 120 <= y < 180: return (224,64,32),0
        if 180 <= x < 260 and 240 <= y < 300: return (32,224,64),0
        return (32,48,64),0
    if scene in {'constants','relative'}:
        if 200 <= x < 280 and 120 <= y < 180: return (224,64,32),0
        if 280 <= x < 360 and 120 <= y < 180: return (32,224,64),0
        return (32,48,64),0
    if scene == 'rtt':
        if not (64 <= x < 576 and 48 <= y < 432):
            return (32,48,64),0
        return (((255,32,32),(32,255,32),(32,32,255),(255,224,32))[(y>=240)*2+(x>=320)]),0
    if 48 <= y < 304:
        for left,blended in [(32,False),(352,True)]:
            if left <= x < left+256:
                transparent = ((x-left)//64 + (y-48)//64) % 2
                if not transparent:
                    return (224,64,32),0
                # SRCALPHA=64/255, DESTBLEND=1-SRCALPHA. One LSB allows
                # the fixed-function blend unit's implementation rounding.
                return ((104,112,152),1) if blended else ((64,128,192),0)
    return (32,48,64),0


def png(rgb,width,height):
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    rows=b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')

results=[]
for scene in (['formats','lifecycle'] if a.resources else (['skinning','constants','relative'] if a.features else ['quads','rtt'])):
    if not a.verify_only:
        with (output/(scene+'.log')).open('w') as log:
            subprocess.run([str(binary),str(output/scene),scene,'120','30'],cwd=output,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    files=[output/(scene+'.ppm')]
    if scene=='lifecycle': files += [output/(scene+'-reset'+str(i)+'.ppm') for i in range(5)]
    file=files[0]
    data=file.read_bytes()
    header,rgb=data.split(b'\n255\n',1)
    if header != b'P6\n640 480' or len(rgb)!=640*480*3:
        raise RuntimeError('invalid readback dimensions: '+scene)
    if any(f.read_bytes()!=data for f in files[1:]): raise RuntimeError('reset image mismatch')
    failures=[];max_delta=0
    for y in range(480):
        for x in range(640):
            actual=rgb[(y*640+x)*3:(y*640+x)*3+3]
            reference,tolerance=expected(scene,x,y)
            delta=max(abs(actual[i]-reference[i]) for i in range(3))
            max_delta=max(max_delta,delta)
            if delta>tolerance:
                if len(failures)<8:
                    failures.append({'x':x,'y':y,'actual':list(actual),'expected':reference})
    if failures:
        raise RuntimeError(scene+' failed analytic pixel check: '+str(failures))
    (output/(scene+'.png')).write_bytes(png(rgb,640,480))
    timing=[float(row.split('\t')[1]) for row in (output/(scene+'-timing.tsv')).read_text().splitlines()[1:]]
    if len(timing)!=(115 if scene=='lifecycle' else 119) or any(not math.isfinite(x) or x<0 for x in timing):
        raise RuntimeError('invalid measured frame sample count/values: '+scene)
    log=(output/(scene+'.log')).read_text()
    if 'PASS '+scene+' draw, readback and Present;' not in log:
        raise RuntimeError('no successful runtime completion marker: '+scene)
    if 'ERROR: AddressSanitizer' in log or 'runtime error:' in log:
        raise RuntimeError('sanitizer diagnostic: '+scene)
    if scene=='formats' and log.count(' mip upload verified')!=10: raise RuntimeError('missing format upload markers')
    if scene=='lifecycle' and log.count(' default resources recreated; managed texture retained')!=4: raise RuntimeError('missing reset markers')
    row={'scene':scene,'ppm_sha256':hashlib.sha256(data).hexdigest(),'pixels_verified':640*480*len(files),'captures_verified':len(files),'maximum_channel_delta':max_delta,'timed_frames':len(timing),'cpu_draw_present_median_ms':statistics.median(timing),'cpu_draw_present_p95_ms':sorted(timing)[math.ceil(.95*len(timing))-1],'scope':'warmup 30; all readback frames excluded; lifecycle reset excluded; CPU wall time, not GPU time or game FPS','driver_warnings':[line for line in log.splitlines() if '[mvk-warn]' in line],'backend_warnings':[line for line in log.splitlines() if line.startswith('warn:')]}
    results.append(row)
    print('PASS',scene,640*480*len(files),'analytic pixels across',len(files),'captures;',len(timing),'CPU timing samples; max channel delta',max_delta)
(output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print('Windows reference comparison NOT TESTED; static object and original character NOT TESTED')
