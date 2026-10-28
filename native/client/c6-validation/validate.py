#!/usr/bin/env python3
"""Validate paired future C6 capture bundles. Never declares C6 completed."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('c3_compare', ROOT/'native/client/c3/compare.py')
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)
CATEGORIES = {'terrain_base','terrain_detail','bsp_base','bsp_lightmap','static_objects',
              'skinned_characters','water','effects','minimap','glow'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def scene_hash(scene):
    return digest(json.dumps(scene, sort_keys=True, separators=(',', ':'), allow_nan=False).encode())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def artifact(root, relative):
    require(isinstance(relative, str) and relative != '', 'missing artifact path')
    p = (root/relative).resolve()
    require(not Path(relative).is_absolute() and p.is_relative_to(root.resolve()), 'artifact escapes bundle')
    require(p.is_file(), 'missing artifact: '+relative)
    return p


def resolved(value):
    if value is None:
        return False
    if isinstance(value,dict):
        return 'unresolved' not in value and all(resolved(v) for v in value.values())
    if isinstance(value,list):
        return all(resolved(v) for v in value)
    return True


def check_parameters(params):
    require(isinstance(params,dict), 'missing runtime parameters')
    for key in ('gameplay_map_id','resource_map_id','unit_id','simulation_tick','animation_tick','texture_tick','random_seed'):
        require(type(params.get(key)) is int and 0 <= params[key] <= 0xffffffff, 'missing/invalid '+key)
    camera = params.get('camera',{})
    for key in ('eye','target','up'):
        v = camera.get(key)
        require(isinstance(v,list) and len(v) == 3 and all(type(x) in (int,float) and math.isfinite(x) for x in v), 'missing/invalid camera '+key)
    direction = [b-a for a,b in zip(camera['eye'],camera['target'])]
    up = camera['up']
    cross = [direction[1]*up[2]-direction[2]*up[1], direction[2]*up[0]-direction[0]*up[2], direction[0]*up[1]-direction[1]*up[0]]
    require(sum(v*v for v in cross) > 1e-12, 'degenerate camera')
    for key in ('fov_y_radians','near','far'):
        require(type(camera.get(key)) in (int,float) and math.isfinite(camera[key]), 'invalid camera '+key)
    require(0 < camera['fov_y_radians'] < math.pi and 0 < camera['near'] < camera['far'], 'invalid camera frustum')
    for key in ('view_matrix','projection_matrix'):
        v = params.get(key)
        require(isinstance(v,list) and len(v) == 16 and all(type(x) in (int,float) and math.isfinite(x) for x in v), 'missing/invalid '+key)
    for key in ('lighting','fog'):
        require(isinstance(params.get(key),dict) and bool(params[key]), 'missing '+key+' settings')
    flags = params.get('render_flags',{})
    for key in ('terrain','bsp','objects','characters','water','effects','minimap','glow','shadows','terrain_shader'):
        require(type(flags.get(key)) is bool, 'missing/invalid render flag '+key)
    require(resolved(params), 'scene still has unresolved runtime settings')


def capture(root, scene, platform):
    meta = json.loads((root/'capture.json').read_text())
    require(meta['platform'] == platform, 'wrong platform')
    require(meta['scene_sha256'] == scene_hash(scene), 'capture used a different scene/settings contract')
    require(meta['asset_hashes'] == scene['asset_hashes'], 'asset identities differ from approved scene')
    check_parameters(meta['actual_parameters'])
    require(meta['actual_parameters'] == scene['parameters'], 'actual runtime parameters differ from approved scene')
    for key in ('build_sha256','backend_sha256','config_sha256'):
        require(re.fullmatch('[0-9a-f]{64}',meta[key]) is not None, 'invalid '+key)
    require(type(meta['failed_device_calls']) is int and meta['failed_device_calls'] == 0, 'failed/invalid real-device call count')
    require(type(meta['nonfinite_meshes']) is int and meta['nonfinite_meshes'] == 0, 'nonfinite geometry must not be rendered')
    require(type(meta['meshes_checked']) is int and meta['meshes_checked'] >= 0, 'invalid mesh audit count')
    if {'static_objects','skinned_characters'} & set(scene['required_categories']):
        require(meta['meshes_checked'] > 0, 'object scene lacks finite mesh audit')
    for category in scene['required_categories']:
        count = meta['draws'].get(category,0)
        require(type(count) is int and count > 0, 'missing draw category: '+category)
    log = artifact(root,meta['runtime_log']).read_bytes()
    require(digest(log) == meta['runtime_log_sha256'], 'runtime log hash differs')
    text = log.decode('utf-8',errors='replace')
    require('C6_CAPTURE_COMPLETE' in text, 'incomplete capture log')
    require(not any(s in text for s in ('ERROR: AddressSanitizer','runtime error:','FAIL ')), 'runtime failure in log')
    frames = meta['readbacks']
    require(isinstance(frames,list) and len(frames) >= 5 and len(set(frames)) == len(frames), 'need five distinct frozen capture files')
    first = None
    physical_files = set()
    for name in frames:
        path = artifact(root,name)
        stat = path.stat()
        identity = (stat.st_dev, stat.st_ino)
        require(identity not in physical_files, 'frozen readback paths alias the same file')
        physical_files.add(identity)
        width,height,rgb,_ = compare.read_ppm(path)
        require(len({rgb[i:i+3] for i in range(0,len(rgb),3)}) >= 2, 'uniform blank readback')
        require([width,height] == scene['dimensions'], 'capture dimensions differ')
        if first is None:
            first = path
        require(path.read_bytes() == first.read_bytes(), 'frozen-frame temporal drift')
    timings = artifact(root,meta['timings']).read_text().splitlines()
    require(timings and timings[0] == 'frame\tcpu_draw_present_ms', 'wrong timing header')
    rows = [r.split('\t') for r in timings[1:]]
    require(len(rows) >= 119 and all(len(r) == 2 for r in rows), 'need at least119 raw timed frames')
    ids = [int(r[0]) for r in rows]
    values = [float(r[1]) for r in rows]
    require(ids == sorted(set(ids)) and all(math.isfinite(v) and v >= 0 for v in values), 'invalid timing values/order')
    return first, {'timed_frames':len(values),'cpu_draw_present_median_ms':sorted(values)[len(values)//2],
                   'scope':'CPU draw/Present wall time; not GPU time or game FPS'}


def validate(contract, native, windows, output):
    require(contract['schema'] == 1 and contract['status'] == 'APPROVED', 'contract requires Windows framing/settings approval first')
    require(contract['scope'] in ('preview','full-c6'), 'invalid contract scope')
    scenes = contract['scenes']
    require(bool(scenes), 'no scenes')
    ids = [s['id'] for s in scenes]
    require(len(set(ids)) == len(ids) and all(re.fullmatch('[a-z0-9-]+',i) for i in ids), 'unsafe or duplicate scene ID')
    categories = set()
    for scene in scenes:
        require(scene['required_categories'] and set(scene['required_categories']) <= CATEGORIES, 'unknown/empty draw categories')
        require(scene['asset_hashes'] and all(re.fullmatch('[0-9a-f]{64}',v) for v in scene['asset_hashes'].values()), 'missing/invalid asset hashes')
        require(len(scene['dimensions']) == 2 and all(type(v) is int and 0 < v <= 8192 for v in scene['dimensions']), 'invalid dimensions')
        check_parameters(scene['parameters'])
        flags = scene['parameters']['render_flags']
        feature_flags = {'terrain_base':'terrain','terrain_detail':'terrain','bsp_base':'bsp','bsp_lightmap':'bsp',
                         'static_objects':'objects','skinned_characters':'characters','water':'water','effects':'effects',
                         'minimap':'minimap','glow':'glow'}
        require(all(flags[feature_flags[c]] for c in scene['required_categories']), 'required category disabled by render flags')
        scene_hash(scene)  # Refuse nonfinite camera/settings values before reading bundles.
        categories.update(scene['required_categories'])
    if contract['scope'] == 'full-c6':
        require(categories == CATEGORIES, 'full C6 contract omits categories: '+','.join(sorted(CATEGORIES-categories)))
    output.mkdir(parents=True,exist_ok=True)
    results = []
    for scene in scenes:
        nf,nt = capture(native/scene['id'],scene,'native')
        wf,wt = capture(windows/scene['id'],scene,'windows-x64')
        stats = compare.compare(nf,wf,output/scene['id'])
        results.append({'id':scene['id'],'scene_sha256':scene_hash(scene),'comparison':stats,'native_timing':nt,'windows_timing':wt})
    return {'scope':contract['scope'],'status':'EXACT_CAPTURE_MATCH_REVIEW_REQUIRED' if all(s['comparison']['match'] for s in results) else 'PIXEL_DIFFERENCES_REVIEW_REQUIRED',
            'c6_completed':False,'reason':'Source-backed category visibility, visual review, portal/streaming sequences and milestone integration require separate evidence.',
            'scenes':results}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('contract','native','windows','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a = p.parse_args()
    try:
        report = validate(json.loads(a.contract.read_text()),a.native,a.windows,a.output)
        (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(report['status']+'; C6 completion NOT asserted')
        raise SystemExit(0 if report['status'] == 'EXACT_CAPTURE_MATCH_REVIEW_REQUIRED' else 1)
    except (OSError,ValueError,KeyError,TypeError) as error:
        p.exit(2,'FAIL capture validation: '+str(error)+'\n')
