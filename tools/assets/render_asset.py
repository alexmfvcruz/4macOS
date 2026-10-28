#!/usr/bin/env python3
"""Catalogue and preview local 4Story assets directly in the terminal."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys

from asset_data import AssetError, DEFAULT_ASSETS, ROOT, Reader, Store
from preview import geometry, terminal, ui_image

CACHE = ROOT / 'runtime/legacy/asset-renderer/catalog.json'
CATALOG = ROOT / 'research/renderable-assets.md'
VERSION = 1


def fingerprint(store):
    paths = list(store.path('Index').glob('*')) + [store.path('Tcd/' + n) for n in
            ('TMon.tcd', 'TNPCTemp.tcd', 'TItem.tcd', 'TItemVisual.tcd')]
    paths += [store.path(p) for p in sorted({row[0] for table in store.indexes.values() for row in table.values()})]
    r = Reader(store.path('Index/TClientList.LST').read_bytes())
    count, total = r.count(), r.count()
    paths += [store.path('Data/' + r.take(r.count()).decode('cp1252')) for _ in range(count)]
    r.eof()
    rows = [(str(p.relative_to(store.assets)), p.stat().st_size, p.stat().st_mtime_ns) for p in paths if p.is_file()]
    return hashlib.sha256(json.dumps([VERSION, rows]).encode()).hexdigest()


def refs(parts, object_id):
    return [[object_id, p['kind'], p['cloth'], p['selection']] for p in parts]


def build_catalog(store):
    entries, excluded = [], []
    valid_meshes = set()
    print('Scanning mesh records…', file=sys.stderr)
    for id, (file, off, end) in store.indexes['mesh'].items():
        try:
            pos, norm, uv, groups = store.mesh(id)
            valid_meshes.add(id)
            entries.append({'key': f'mesh:0x{id:08x}', 'category': 'mesh', 'name': Path(file.replace('\\', '/')).stem,
                            'mesh': id, 'note': f'{len(pos)} vertices; untextured geometry', 'source': file})
        except (ValueError, OSError, OverflowError) as e:
            excluded.append(['mesh', id, str(e)])
    print('Scanning object parts…', file=sys.stderr)
    parts_by_object, visual_parts = {}, defaultdict(list)
    for id, (file, off, end) in store.indexes['object'].items():
        try:
            parts = [p for p in store.object(id) if p['mesh'] in valid_meshes]
            if not parts: raise AssetError('No supported geometry')
            parts_by_object[id] = parts
            defaults = {}
            for p in sorted(parts, key=lambda p: (p['kind'], p['cloth'], p['selection'])):
                defaults.setdefault(p['kind'], p)
            default = list(defaults.values())
            entries.append({'key': f'object:0x{id:08x}', 'category': 'object', 'name': Path(file.replace('\\', '/')).stem,
                            'parts': refs(default, id), 'note': 'Default static parts; not a game-state assembly', 'source': file})
            for p in parts:
                visual_parts[(p['kind'], p['cloth'], p['selection'])].append((id, p))
                entries.append({'key': f'part:0x{id:08x}:{p["kind"]}:{p["cloth"]}:{p["selection"]}',
                                'category': 'part', 'name': Path(file.replace('\\', '/')).stem,
                                'parts': refs([p], id), 'note': f'Mesh 0x{p["mesh"]:08x}; exact individual appearance part', 'source': file})
        except (ValueError, OSError, OverflowError) as e:
            excluded.append(['object', id, str(e)])
    for category, chart in [('monster', store.monsters), ('npc', store.npcs)]:
        for id, row in chart.items():
            obj = row['object']
            if obj not in parts_by_object:
                excluded.append([category, id, 'No supported object geometry']); continue
            chosen = store.choose_parts(obj, row['equipment'])
            if any(p['mesh'] not in valid_meshes for p in chosen):
                excluded.append([category, id, 'Selected appearance has unsupported geometry']); continue
            entries.append({'key': f'{category}:{id}', 'category': category, 'name': row['name'],
                            'parts': refs(chosen, obj), 'note': f'Chart equipment selections; static object 0x{obj:08x}', 'source': f'Tcd/{"TMon" if category == "monster" else "TNPCTemp"}.tcd'})
    for id, row in store.items.items():
        visual = store.visuals.get(row['visual'])
        if not visual: continue
        if visual['object'] in parts_by_object:
            chosen = [p for p in store.choose_parts(visual['object']) if p['mesh'] in valid_meshes]
            if not chosen: continue
            parts = refs(chosen, visual['object'])
            note = 'Attached item object, in its stored pose; no character attachment'
        else:
            options = visual_parts.get((visual['kind'], visual['cloth'], visual['selection']))
            if not options: continue
            obj, p = min(options, key=lambda x: x[0]); parts = refs([p], obj)
            note = f'Equipment fragment; default model 0x{obj:08x}; {len(options)} model match(es)'
        entries.append({'key': f'item:{id}', 'category': 'item', 'name': row['name'], 'parts': parts,
                        'note': note, 'source': 'Tcd/TItem.tcd'})
    print('Indexing textures and UI images…', file=sys.stderr)
    checked_buffers = set()
    for category in ('texture', 'image'):
        for number, (id, (file, off, end)) in enumerate(store.indexes[category].items(), 1):
            if number % 1000 == 0: print(f'  {category}: {number} checked', file=sys.stderr)
            try:
                if category == 'texture':
                    decoded = store.texture(id)
                    note = f'Decoded texture sheet {decoded.width}x{decoded.height}'
                else:
                    width, height, tiles = store.image_parts(id)
                    if not tiles: raise AssetError('UI image has no tiles')
                    for buffer, vertices in tiles:
                        if buffer not in checked_buffers:
                            store.buffer(buffer)
                            checked_buffers.add(buffer)
                    note = f'UI artwork {width}x{height}; {len(tiles)} tile(s)'
                entries.append({'key': f'{category}:0x{id:08x}', 'category': category,
                                'name': Path(file.replace('\\', '/')).stem, 'resource': id,
                                'note': note, 'source': file})
            except (ValueError, OSError, OverflowError, SyntaxError) as e:
                excluded.append([category, id, str(e)])
    entries.sort(key=lambda x: (x['category'], x['name'].casefold(), x['key']))
    data = {'version': VERSION, 'assets': str(store.assets), 'fingerprint': fingerprint(store),
            'entries': entries, 'excluded': excluded}
    annotate_missing_textures(store, data)
    return data


def annotate_missing_textures(store, data):
    valid_textures = {e['resource'] for e in data['entries'] if e['category'] == 'texture'}
    for entry in data['entries']:
        if 'parts' not in entry: continue
        missing = set()
        for obj, kind, cloth, selection in entry['parts']:
            part = next(p for p in store.object(obj) if (p['kind'], p['cloth'], p['selection']) == (kind, cloth, selection))
            missing.update(m['texture'] for m in part['materials'] if m['texture'] and m['texture'] not in valid_textures)
        if missing and not entry.get('plain_only'):
            entry['plain_only'] = True
            entry['note'] += '; plain geometry only (unavailable primary texture)'


def load_catalog(store, rebuild=False):
    if not rebuild and CACHE.exists():
        data = json.loads(CACHE.read_text())
        if data.get('version') == VERSION and data.get('assets') == str(store.assets) and data.get('fingerprint') == fingerprint(store):
            return data
    data = build_catalog(store)
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps(data, ensure_ascii=False, separators=(',', ':')))
    return data


def markdown(data, target):
    def clean(text): return str(text).replace('|', '\\|').replace('\n', ' ').replace('\r', ' ')
    counts = Counter(e['category'] for e in data['entries'])
    lines = ['# Renderable 4Story asset catalogue', '',
        'Generated from the local supplied client assets. Search this file by game name,',
        'resource archive or ID. Copy an entry ID into the terminal renderer:', '',
        '```sh', 'sh tools/render-asset.sh show monster:31036',
        'sh tools/render-asset.sh list behemoth',
        'sh tools/render-asset.sh show monster:31036 --yaw 90 --width 100',
        'sh tools/render-asset.sh show monster:31036 --format iterm', '```', '',
        'No image files are created by `show`. ANSI colour half-blocks work in normal',
        'terminals; iTerm2 and Kitty can display an in-memory PNG inline. The first run',
        'sets up an ignored Python environment with numpy/Pillow. `catalog` updates',
        'this Markdown file and an ignored metadata cache; it does not render PNGs.', '',
        '## What the previews represent', '',
        '- Monsters/mounts and NPC templates: chart object plus matching default equipment',
        '  selections. These are static previews, not full animated game-state assembly.',
        '- Items: attached object or an equipment fragment, not a dressed character.',
        '  Model alternatives appear in the raw parts section. Not every item has 3D geometry.',
        '- Objects/parts: default combination or one exact indexed appearance selection.',
        '- Meshes: untextured static geometry. Textures: decoded texture sheets.',
        '- Images: UI artwork reconstructed from stored image tiles and texture buffers.',
        '- Texture pixels and UI tile dependencies are decoded during catalogue generation;',
        '  not every assembled preview has been visually verified. Decoder failures are reported.',
        '- Lighting is simplified. Stored vertices, LOD 0 and primary textures are used;',
        '  skinning/animations, attached equipment transforms, secondary texture stages,',
        '  particle simulation, full material rules and complete map scenes are not reproduced.',
        '- Raw map/terrain meshes may be previewable; a playable map scene is not supported.',
        '', '## Counts', '', '| Category | Entries |', '| --- | ---: |']
    lines += [f'| {k} | {v} |' for k, v in sorted(counts.items())]
    lines += ['', f'{len(data["excluded"])} resource/template records were excluded with explicit reasons below.',
              'No game source, assets or database rows are changed.', '',
              'Regenerate after changing assets: `sh tools/render-asset.sh catalog --refresh`.', '',
              'Source format references and usage: [renderer README](../tools/assets/README.md).', '']
    for category in ('monster', 'npc', 'item', 'object', 'part', 'mesh', 'texture', 'image'):
        lines += [f'## {category.capitalize()} entries', '', '| Entry ID | Name/archive | Notes |', '| --- | --- | --- |']
        for e in data['entries']:
            if e['category'] == category:
                lines.append(f'| `{e["key"]}` | {clean(e["name"])} | {clean(e["note"])} |')
        lines.append('')
    lines += ['## Excluded resources/templates', '', '| Category | ID | Reason |', '| --- | --- | --- |']
    lines += [f'| {kind} | {id} | {clean(reason)} |' for kind, id, reason in data['excluded']]
    target.parent.mkdir(parents=True, exist_ok=True); target.write_text('\n'.join(lines) + '\n')


def render_entry(store, entry, args):
    if entry['category'] == 'texture': return store.texture(entry['resource'])
    if entry['category'] == 'image': return ui_image(store, entry['resource'])
    if entry['category'] == 'mesh':
        parts = [{'mesh': entry['mesh'], 'materials': []}]
    else:
        parts = []
        for obj, kind, cloth, selection in entry['parts']:
            parts.append(next(p for p in store.object(obj) if (p['kind'], p['cloth'], p['selection']) == (kind, cloth, selection)))
    return geometry(store, parts, args.resolution, round(args.resolution * 0.786), args.yaw, args.elevation, args.plain or entry.get('plain_only', False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=DEFAULT_ASSETS, help='Alternate supplied client-data directory')
    sub = parser.add_subparsers(dest='command', required=True)
    listing = sub.add_parser('list', help='Search names, archives and entry IDs')
    listing.add_argument('query', nargs='?', default=''); listing.add_argument('--category')
    listing.add_argument('--limit', type=int, default=50)
    catalog = sub.add_parser('catalog', help='Generate the complete Markdown catalogue')
    catalog.add_argument('--refresh', action='store_true'); catalog.add_argument('--output', type=Path, default=CATALOG)
    show = sub.add_parser('show', help='Render an entry ID or a unique name in the terminal')
    show.add_argument('asset'); show.add_argument('--format', choices=('ansi', 'auto', 'iterm', 'kitty'), default='ansi')
    show.add_argument('--width', type=int); show.add_argument('--resolution', type=int, default=700)
    show.add_argument('--yaw', type=float, default=140); show.add_argument('--elevation', type=float, default=19)
    show.add_argument('--plain', action='store_true', help='Untextured geometry')
    args = parser.parse_args()
    if args.command == 'show':
        import math
        if not 64 <= args.resolution <= 1600 or args.width is not None and not 10 <= args.width <= 240:
            parser.error('Resolution must be 64..1600; terminal width 10..240 columns')
        if not math.isfinite(args.yaw) or not math.isfinite(args.elevation) or not -89 <= args.elevation <= 89:
            parser.error('Camera yaw must be finite; elevation -89..89 degrees')
    store = Store(args.assets); data = load_catalog(store, getattr(args, 'refresh', False))
    if args.command == 'catalog':
        markdown(data, args.output)
        print(f'{len(data["entries"])} entries -> {args.output}; {len(data["excluded"])} exclusions')
    elif args.command == 'list':
        matches = [e for e in data['entries'] if args.query.casefold() in (e['key']+' '+e['name']+' '+e['source']).casefold() and
                   (not args.category or e['category'] == args.category)]
        for e in matches[:max(0, args.limit)]: print(f'{e["key"]:48} {e["name"]}')
        print(f'{len(matches)} match(es)' + ('; increase --limit to see more' if len(matches) > args.limit else ''))
    else:
        matches = [e for e in data['entries'] if e['key'].casefold() == args.asset.casefold()]
        if not matches: matches = [e for e in data['entries'] if e['name'].casefold() == args.asset.casefold()]
        if len(matches) != 1:
            raise AssetError(f'{len(matches)} matches for {args.asset!r}; use list and select an exact entry ID')
        entry = matches[0]; print(f'{entry["name"]} [{entry["key"]}] — {entry["note"]}', file=sys.stderr)
        image = render_entry(store, entry, args)
        terminal(image, args.format, args.width)


if __name__ == '__main__':
    try: main()
    except BrokenPipeError:
        sys.exit(0)
    except (AssetError, OSError, ValueError, KeyError) as e:
        print(f'asset renderer: {e}', file=sys.stderr); sys.exit(1)
