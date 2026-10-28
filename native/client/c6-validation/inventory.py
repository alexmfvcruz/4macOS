#!/usr/bin/env python3
"""Read-only, bounded original map-data preflight. Does not load/render a map."""
import argparse
from array import array
from collections import Counter
import hashlib
import json
import math
from pathlib import Path, PureWindowsPath
import struct
import sys


class DataError(ValueError):
    pass


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise DataError('truncated or invalid-sized map record')
        start = self.pos
        self.pos += size
        return self.data[start:self.pos]

    def get(self, fmt):
        values = struct.unpack('<'+fmt, self.take(struct.calcsize('<'+fmt)))
        return values[0] if len(values) == 1 else values

    def count(self, maximum=1_000_000):
        n = self.get('i')
        if not 0 <= n <= maximum:
            raise DataError('invalid count: '+str(n))
        return n


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(root, relative):
    p = PureWindowsPath(relative)
    if p.root or p.drive or '..' in p.parts:
        raise DataError('unsafe asset path')
    result = root.joinpath(*p.parts).resolve()
    if not result.is_relative_to(root.resolve()):
        raise DataError('asset path escapes root')
    return result


def map_index(data):
    r = Reader(data)
    nf, ne = r.count(10000), r.count()
    files = [r.take(r.count(4096)).decode('latin-1') for _ in range(nf)]
    entries = []
    for _ in range(ne):
        ident, file, offset = r.get('III')
        if file >= len(files):
            raise DataError('map file index out of bounds')
        entries.append({'resource_id': ident, 'file': files[file], 'offset': offset})
    if r.pos != len(data) or len({e['resource_id'] for e in entries}) != ne:
        raise DataError('trailing index bytes or duplicate map IDs')
    return entries


def chart_maps(data):
    r = Reader(data)
    count = r.get('H')
    maps = []
    for _ in range(count):
        key, resource = r.get('II')
        transform = list(r.get('9f'))
        if not all(map(math.isfinite, transform)):
            raise DataError('nonfinite chart transform')
        dungeon, npc = r.get('BB')
        maps.append({'gameplay_id': key, 'resource_id': resource, 'scale': transform[:3],
                     'translation': transform[3:6], 'rotation': transform[6:], 'dungeon': dungeon, 'npc_call': npc})
    if r.pos != len(data) or len({m['gameplay_id'] for m in maps}) != count:
        raise DataError('chart record size or duplicate gameplay IDs')
    return maps


def terrain_header(data):
    r = Reader(data)
    kind = r.get('B')
    if kind != 0:
        return {'map_type': kind, 'status': 'not decoded: only MT_EXT header supported'}
    ux, uz, length, tile, cells, texture, shadow = r.get('7i')
    if not all(v > 0 for v in (ux, uz, length, tile, cells, texture, shadow)) or ux*uz > 1_000_000 or length > 16384:
        raise DataError('invalid terrain dimensions')
    light, shader, spec = r.get('3f'), r.get('B'), r.get('3f')
    details = []
    for _ in range(r.get('H')):
        scale, texture_id = r.get('fI')
        if not math.isfinite(scale):
            raise DataError('nonfinite detail scale')
        details.append({'texture_id': texture_id, 'scale': scale})
    enabled = [i for i, flag in enumerate(r.take(ux*uz)) if flag]
    if not all(map(math.isfinite, (*light, *spec))):
        raise DataError('nonfinite terrain lighting')
    return {'map_type': kind, 'unit_count_x': ux, 'unit_count_z': uz, 'unit_length': length,
            'tile_length': tile, 'cell_count': cells, 'texture_size': texture, 'shadow_pixel': shadow,
            'light': list(light), 'use_shader': shader, 'specular': list(spec), 'details': details,
            'enabled_units': enabled, 'decoded_header_bytes': r.pos}


def unit_prefix(data, header, unit_id):
    r = Reader(data)
    length = header['unit_length']
    count = r.count(300_000_000)
    if count != (length+1)**2:
        raise DataError('height count disagrees with terrain header')
    heights = array('f')
    heights.frombytes(r.take(count*4))
    if sys.byteorder != 'little':
        heights.byteswap()
    if not all(map(math.isfinite, heights)):
        raise DataError('nonfinite terrain heights')
    regions = r.take(r.count(300_000_000))
    shadows = r.take(r.count(300_000_000)*2)
    detail = r.take(r.count(300_000_000))
    if len(regions) != length*length or len(shadows) != length*length*2 or len(detail) != length*length:
        raise DataError('tile plane size disagrees with terrain header')
    objects = []
    for _ in range(r.count()):
        instance, obj = r.get('II')
        vectors = list(r.get('9f'))
        if not all(map(math.isfinite, vectors)):
            raise DataError('nonfinite object placement')
        funcs = r.get('5I')
        cloths = [r.get('III') for _ in range(r.count())]
        for _ in range(r.count()):
            r.get('I'); r.get('B'); r.take(r.count(100_000_000))
        objects.append({'instance_id': instance, 'object_id': obj, 'position': vectors[3:6],
                        'action_id': funcs[3], 'animation_id': funcs[4], 'cloth_selections': cloths})
    effects = []
    for _ in range(r.count()):
        instance, effect = r.get('II')
        vectors = r.get('6f')
        if not all(map(math.isfinite, vectors)):
            raise DataError('nonfinite effect placement')
        effects.append({'instance_id': instance, 'effect_id': effect, 'position': list(vectors[:3]), 'bias': r.get('B')})
    sounds = r.count()
    for _ in range(sounds):
        r.take(20)
    # Only the prefix through sound records is interpreted; remaining GPU image
    # payloads are hashed with the full file, not claimed decoded or validated.
    cx, cz = length//2, length//2
    x = ((unit_id % header['unit_count_x'])*length + cx)*header['tile_length']
    z = ((unit_id // header['unit_count_x'])*length + cz)*header['tile_length']
    y = heights[cz*(length+1)+cx]
    return {'unit_id': unit_id, 'height_count': count, 'height_min': min(heights), 'height_max': max(heights),
            'center_height': y, 'center_world': [x, y, z], 'detail_histogram': dict(sorted(Counter(detail).items())),
            'object_count': len(objects), 'effect_count': len(effects), 'sound_count': sounds,
            'objects': objects, 'effects': effects, 'decoded_prefix_bytes': r.pos,
            'unparsed_tail_bytes': len(data)-r.pos, 'mesh_finiteness': 'NOT CHECKED: placements are not mesh geometry'}


def inventory(assets, gameplay_id, include_placements=False):
    index = (assets/'Index/TClientP.IDX').read_bytes()
    chart = (assets/'Tcd/TMAP.tcd').read_bytes()
    entries, maps = map_index(index), chart_maps(chart)
    selected = next((m for m in maps if m['gameplay_id'] == gameplay_id), None)
    if selected is None:
        raise DataError('gameplay map absent from chart')
    records, files = [], {}
    for e in entries:
        path = safe_path(assets/'Data', e['file'])
        if e['file'] not in files:
            files[e['file']] = path.read_bytes()
        data = files[e['file']]
        if e['offset'] >= len(data):
            raise DataError('map offset outside archive')
        # The header decoder intentionally stops before the next record.
        end = min((other['offset'] for other in entries if other['file'] == e['file'] and other['offset'] > e['offset']), default=len(data))
        h = terrain_header(data[e['offset']:end])
        records.append({**e, **h})
    chosen = next((m for m in records if m['resource_id'] == selected['resource_id']), None)
    if chosen is None or chosen['map_type'] != 0:
        raise DataError('chosen map has no decoded terrain header')
    units, missing = [], []
    for uid in chosen['enabled_units']:
        relative = f'Data/MAP/EM{chosen["resource_id"]}_{uid}.tmu'
        path = safe_path(assets, relative)
        if not path.is_file():
            missing.append(relative)
            continue
        data = path.read_bytes()
        unit = unit_prefix(data, chosen, uid)
        unit['object_id_histogram'] = dict(sorted(Counter(o['object_id'] for o in unit['objects']).items()))
        unit['effect_id_histogram'] = dict(sorted(Counter(e['effect_id'] for e in unit['effects']).items()))
        if not include_placements:
            unit['placement_samples'] = unit.pop('objects')[:8]
            unit['effect_samples'] = unit.pop('effects')[:8]
        units.append({'file': relative, 'bytes': len(data), 'sha256': sha(data), **unit})
    standalone = []
    for path in sorted((assets/'Data/MAP').iterdir()):
        if path.suffix.lower() == '.tma' and path.name.lower().startswith('bsp'):
            data = path.read_bytes()
            standalone.append({'file': str(path.relative_to(assets)), 'bytes': len(data), 'sha256': sha(data),
                               'status': 'present, not decoded; not proof of indexed/renderable BSP'})
    return {'schema': 1, 'scope': 'offline map/header/unit-prefix inspection only; no original loader or GPU execution',
            'input_hashes': {'Index/TClientP.IDX': sha(index), 'Tcd/TMAP.tcd': sha(chart),
                             **{'Data/'+k.replace('\\','/'): sha(v) for k, v in files.items()}},
            'chart_maps': maps, 'indexed_maps': records, 'selected_map': selected, 'selected_terrain': chosen,
            'units': units, 'missing_enabled_units': missing, 'standalone_bsp_files': standalone}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets', type=Path, required=True)
    p.add_argument('--map', type=int, default=0)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--placements', action='store_true', help='include all placements (large output)')
    args = p.parse_args()
    try:
        result = inventory(args.assets.resolve(), args.map, args.placements)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
    except (DataError, OSError, struct.error) as error:
        p.exit(1, 'FAIL '+str(error)+'\n')
    print(f'PASS offline inventory: gameplay {args.map}, resource {result["selected_map"]["resource_id"]}, '
          f'{len(result["units"])} parsed units, {len(result["missing_enabled_units"])} missing; rendering NOT TESTED')
