#!/usr/bin/env python3
"""Cross-check parsed C2 fields against the existing read-only asset_data reader.

Run with the project's asset-renderer Python environment (numpy/Pillow). Supply
asset_data.py explicitly; it is a user-owned untracked file in the main checkout.
Uncovered animation/effect/map formats and native texture failures stay explicit.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare import groups


def run(args):
    spec = importlib.util.spec_from_file_location('asset_data', args.reader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    store = module.Store(args.assets)
    store.indexes['texture'] = store.index('0_TClientS.IDX')
    counts = Counter()
    errors = []
    validation_errors = []
    expected = {'obj': set(store.indexes['object']), 'mesh': set(store.indexes['mesh']),
                'imgbuf': set(store.buffer_index()), 'tex.created': set(), 'img.source': set()}
    for kind, dataset, key_stride, fixed in [('texture', 'tex.created', 24, 14), ('image', 'img.source', 8, 5)]:
        for resource_id in store.indexes[kind]:
            q = module.compressed(store.read(kind, resource_id))
            q.skip(q.count()*4)
            q.skip(q.count()*key_stride)
            q.skip(fixed)
            size = q.get('I') if kind == 'image' else q.get('II')[1]
            if size:
                expected[dataset].add(resource_id)
    seen = {dataset: set() for dataset in expected}
    terminal = None
    reader_rejections = []
    def check(fields, field, expected):
        expected = str(expected)
        counts['fields_checked'] += 1
        if fields.get(field) != expected:
            counts['different'] += 1
            if len(errors) < 100:
                errors.append({'dataset': dataset, 'id': id, 'field': field,
                               'native': fields.get(field), 'python': expected})
    def sha(bytes):
        return hashlib.sha256(bytes).hexdigest().upper()
    for (dataset, key), fields in groups(args.dump, hashlib.sha256()):
        terminal = ((dataset, key), fields)
        if dataset not in expected:
            continue
        id = int(key)
        if id in seen[dataset]:
            validation_errors.append(f'duplicate {dataset} ID {id}')
        seen[dataset].add(id)
        counts[dataset] += 1
        if dataset == 'obj':
            for part in store.object(id):
                prefix = f'cloth_kinds[{part["kind"]}].cloths[{part["cloth"]}].meshes[{part["selection"]}].'
                mesh = part['mesh'] if part['mesh'] in store.indexes['mesh'] else 0
                check(fields, prefix+'mesh_id', mesh)
                check(fields, prefix+'materials.count', len(part['materials']))
                for i, material in enumerate(part['materials']):
                    prefix2 = prefix+f'materials[{i}].'
                    texture = material['texture'] if material['texture'] in store.indexes['texture'] else 0
                    check(fields, prefix2+'texture[0]', texture)
                    check(fields, prefix2+'m_bMinAlpha', material['alpha'])
                    check(fields, prefix2+'m_dwOrder', material['order'])
        elif dataset == 'mesh':
            r = store.read('mesh', id)
            n_groups, nodes, use_vb, levels = r.get('IIBI')
            check(fields, 'm_dwMeshCount', n_groups)
            check(fields, 'm_dwNodeCount', nodes)
            check(fields, 'm_bUseVB', use_vb)
            r.skip(16 + nodes*64)
            n = r.get('I')
            raw = r.take(n*(48 if nodes else 40))
            if n:
                check(fields, 'vertex_buffer.data.sha256', sha(raw))
            for group in range(n_groups):
                for level in range(levels):
                    segments = r.count()
                    prefix = f'segments[{group}][{level}].'
                    check(fields, prefix+'count', segments)
                    data = bytearray()
                    for segment in range(segments):
                        count, matrix = r.get('IH')
                        check(fields, prefix+f'{segment}.indices', count)
                        check(fields, prefix+f'{segment}.matrix', matrix)
                        data.extend(r.take(count*2))
                    if data:
                        check(fields, f'index_buffer[{group}][{level}].data.sha256', sha(data))
            r.skip(max(0, levels-1)*4)
            r.eof()
            # Also execute the existing geometry reader's triangle validation.
            try:
                store.mesh(id)
                counts['mesh_geometry_reader_pass'] += 1
            except module.AssetError as error:
                counts['mesh_geometry_reader_rejections'] += 1
                if len(reader_rejections) < 100:
                    reader_rejections.append({'id': id, 'reason': str(error)})
        elif dataset in ('tex.created', 'imgbuf'):
            if dataset == 'tex.created':
                q = module.compressed(store.read('texture', id))
                q.skip(q.count()*4)
                q.skip(q.count()*24)
                q.skip(13)
                format_tag, original, size = q.get('BII')
                payload = q.take(size)
                check(fields, 'format_tag', format_tag)
                check(fields, 'decoded_size', original)
                check(fields, 'source.sha256', sha(payload))
                image = store.texture(id)
            else:
                file, offset, size = store.buffer_index()[id]
                with store.path(file).open('rb') as stream:
                    stream.seek(offset)
                    q = module.compressed(module.Reader(stream.read(size)))
                q.get('I')
                format_tag, original, length = q.get('BII')
                payload = q.take(length)
                image = store.buffer(id)
            decoded_payload = module.inflate(payload, original) if original else payload
            if fields.get('created') == '0':
                counts['native_texture_failures'] += 1
                continue
            # The reader returns file dimensions; default D3DX texture creation
            # pads uncompressed NPOT images. Compare that explicit contract.
            width = 1 << (image.width-1).bit_length()
            height = 1 << (image.height-1).bit_length()
            check(fields, 'level[0].width', width)
            check(fields, 'level[0].height', height)
            # Decoded-pixel parity applies to ARGB destinations. Compressed
            # destination rows are checked against the original Windows dump.
            if fields.get('level[0].format') == '21':
                # Pillow's BC decoder rounds differently from original D3DX.
                # Windows captures validate those converted pixels independently.
                compressed_dds = (decoded_payload[:4] == b'DDS '
                                  and decoded_payload[84:88] in (b'DXT1', b'DXT2', b'DXT3', b'DXT4', b'DXT5'))
                if compressed_dds:
                    counts['bc_to_argb_pixels_uncovered'] += 1
                else:
                    pixels = image.tobytes('raw', 'BGRA')
                    if (width, height) != image.size:
                        row = image.width*4
                        pixels = b''.join(pixels[y*row:(y+1)*row]+bytes((width-image.width)*4)
                                          for y in range(image.height))+bytes((height-image.height)*width*4)
                    check(fields, 'level[0].active_rows.sha256', sha(pixels))
                    counts['decoded_argb_checks'] += 1
        elif dataset == 'img.source':
            width, height, parts = store.image_parts(id)
            check(fields, 'width', width)
            check(fields, 'height', height)
            check(fields, 'parts.count', len(parts))
            for i, (buffer, vertices) in enumerate(parts):
                check(fields, f'parts[{i}].image_buffer_id', buffer)
                check(fields, f'parts[{i}].vertex_buffer.data.sha256', sha(vertices.tobytes()))
    coverage = {}
    for dataset in expected:
        missing, extra = expected[dataset]-seen[dataset], seen[dataset]-expected[dataset]
        coverage[dataset] = {'expected': len(expected[dataset]), 'seen': len(seen[dataset]),
                             'missing': len(missing), 'extra': len(extra),
                             'missing_examples': sorted(missing)[:20], 'extra_examples': sorted(extra)[:20]}
        if missing or extra:
            validation_errors.append(f'{dataset}: resource ID coverage differs')
    if not terminal or terminal[0] != ('result', '0') or terminal[1].get('status') not in ('complete', 'incomplete'):
        validation_errors.append('missing terminal result')
    elif terminal[1].get('texture_failures') != str(counts['native_texture_failures']):
        validation_errors.append('terminal texture failure count differs from observed failures')
    return {'counts': dict(counts), 'first_differences': errors,
            'coverage': coverage, 'validation_errors': validation_errors,
            'mesh_geometry_reader_all_pass': not counts['mesh_geometry_reader_rejections'],
            'mesh_geometry_reader_rejections': reader_rejections,
            'reader_sha256': sha(Path(args.reader).read_bytes()),
            'uncovered': ['animation', 'effect', 'map', 'compressed destination conversion', 'BC-to-ARGB pixel conversion (Pillow rounding differs)',
                          'secondary material texture references, types, blend and other material fields'],
            'pass_for_compared_fields': not counts['different'] and not validation_errors,
            'complete_texture_coverage': not counts['native_texture_failures'] and not validation_errors}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('reader')
    p.add_argument('assets')
    p.add_argument('dump')
    p.add_argument('output')
    a = p.parse_args()
    result = run(a)
    Path(a.output).write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result['counts'], sort_keys=True))
    raise SystemExit(0 if result['pass_for_compared_fields'] else 1)
