"""Read this release's indexed visual resources; never modify game assets.

Layouts: TChart.cpp InitTMONTEMP/InitTNPCTEMP/InitTITEMTEMP/InitTITEMVISUAL;
TachyonRes.cpp LoadOBJ/LoadMESH/LoadTEX/LoadIMG/LoadIMGBUF;
T3D.h WMESHVERTEX/MESHVERTEX/TVERTEX. All disk scalars are Windows LE.
"""
from __future__ import annotations
from collections import defaultdict
from functools import lru_cache
from pathlib import Path
import io
import struct
import zlib

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ASSETS = ROOT / 'game'
MAX_RECORD = 128 * 1024 * 1024


class AssetError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, size: int) -> bytes:
        if not 0 <= size <= len(self.data) - self.pos:
            raise AssetError(f'Truncated record at {self.pos}: need {size} bytes')
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def skip(self, size: int):
        self.take(size)

    def get(self, fmt: str):
        values = struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))
        return values[0] if len(values) == 1 else values

    def count(self):
        n = self.get('i')
        if not 0 <= n <= 1_000_000:
            raise AssetError(f'Invalid count {n}')
        return n

    def text(self):
        n = self.get('B')
        if n == 255:
            n = self.get('H')
            if n == 65534:
                raise AssetError('Unicode CArchive string is not supported in this MBCS release')
            if n == 65535:
                n = self.get('I')
        return self.take(n).decode('cp1252', errors='replace')

    def eof(self):
        if self.pos != len(self.data):
            raise AssetError(f'Unconsumed record bytes: {len(self.data) - self.pos}')


def inflate(data: bytes, expected: int) -> bytes:
    if not 0 < expected <= MAX_RECORD:
        raise AssetError(f'Invalid decompressed size {expected}')
    decoder = zlib.decompressobj()
    try:
        result = decoder.decompress(data, expected + 1)
    except zlib.error as e:
        raise AssetError('Invalid compressed asset stream') from e
    if len(result) != expected or not decoder.eof or decoder.unused_data:
        raise AssetError('Compressed size/stream mismatch')
    return result


def compressed(r: Reader) -> Reader:
    original, size = r.get('II')
    return Reader(inflate(r.take(size), original))


def decode_image(payload: bytes, original: int = 0) -> Image.Image:
    if original:
        payload = inflate(payload, original)
    with Image.open(io.BytesIO(payload)) as img:
        if img.width * img.height > 16_777_216:
            raise AssetError('Texture exceeds preview pixel limit')
        return img.convert('RGBA')


class Store:
    def __init__(self, assets=DEFAULT_ASSETS):
        self.assets = Path(assets).resolve()
        self.indexes = {k: self.index(name) for k, name in {
            'object': 'TClientO.IDX', 'mesh': 'TClientM.IDX',
            'texture': '2_TClientS.IDX', 'image': 'TClientI.IDX',
        }.items()}
        self.items, self.visuals, self.monsters, self.npcs = self.charts()
        self._buffers = None

    def path(self, relative):
        path = (self.assets / relative.replace('\\', '/')).resolve()
        if not path.is_relative_to(self.assets):
            raise AssetError('Asset path escapes data directory')
        return path

    def index(self, name):
        r = Reader(self.path('Index/' + name).read_bytes())
        nf, n = r.count(), r.count()
        files = [r.take(r.count()).decode('cp1252') for _ in range(nf)]
        rows = []
        offsets = defaultdict(set)
        for _ in range(n):
            id, fi, off = r.get('III')
            if fi >= nf:
                raise AssetError('Invalid resource file index')
            file = 'Data/' + files[fi]
            rows.append((id, file, off))
            offsets[file].add(off)
        r.eof()
        ends = {}
        for file, starts in offsets.items():
            size = self.path(file).stat().st_size
            ordered = sorted(starts)
            if ordered and ordered[-1] >= size:
                raise AssetError(f'Offset outside {file}')
            ends[file] = dict(zip(ordered, ordered[1:] + [size]))
        return {id: (file, off, ends[file][off]) for id, file, off in rows}

    def read(self, kind, id):
        try:
            file, start, end = self.indexes[kind][id]
        except KeyError as e:
            raise AssetError(f'Missing {kind} resource 0x{id:08x}') from e
        if end - start > MAX_RECORD:
            raise AssetError('Resource record exceeds preview limit')
        with self.path(file).open('rb') as f:
            f.seek(start)
            data = f.read(end - start)
        return Reader(data)

    def charts(self):
        r = Reader(self.path('Tcd/TItem.tcd').read_bytes()); items = {}
        for _ in range(r.get('H')):
            id, type, kind, attr = r.get('HBBH'); name = r.text()
            # Fixed fields through skill-item type, before five visual IDs.
            r.skip(65); visuals = r.get('5H'); r.skip(28)
            items[id] = {'name': name, 'visual': visuals[0]}
        r.eof()
        r = Reader(self.path('Tcd/TItemVisual.tcd').read_bytes()); visuals = {}
        for _ in range(r.get('H')):
            id = r.get('H'); inven, obj, kind, cloth, normal, battle, pivot, bpivot = r.get('8I')
            icon = r.get('H'); r.skip(27)
            visuals[id] = {'object': obj, 'kind': kind, 'cloth': cloth, 'selection': normal, 'icon': icon}
        r.eof()
        r = Reader(self.path('Tcd/TMon.tcd').read_bytes()); monsters = {}
        for _ in range(r.get('H')):
            id, kind = r.get('HH'); title, name = r.text(), r.text(); r.skip(12)
            obj = r.get('I'); equip = r.get('19H')
            r.skip(8 + 8 + 4 + 16 + 1 + 2 + 4 + 2 + 2 + 2 + 1 + 2 + 4 + 2 + 1 + 1 + 1 + 4 + 2 + 4)
            monsters[id] = {'name': name, 'object': obj, 'equipment': equip}
        r.eof()
        r = Reader(self.path('Tcd/TNPCTemp.tcd').read_bytes()); npcs = {}
        for _ in range(r.get('H')):
            id, obj = r.get('HI'); equip = r.get('19H'); r.skip(10)
            npcs[id] = {'name': f'NPC template {id}', 'object': obj, 'equipment': equip}
        r.eof()
        return items, visuals, monsters, npcs

    @lru_cache(maxsize=128)
    def object(self, id):
        r = self.read('object', id)
        def effects(): r.skip(r.count() * 49)
        def sounds(): r.skip(r.count() * 20)
        r.skip(1); r.skip(r.count() * 8); effects(); sounds()
        for _ in range(r.count()):
            r.skip(5); r.skip(r.count())
        for _ in range(r.count()):
            r.skip(4); effects(); sounds()
            for _ in range(r.count()):
                r.skip(8); effects(); sounds()
        parts = []
        for _ in range(r.count()):
            kind = r.get('I')
            for _ in range(r.count()):
                cloth = r.get('I')
                for _ in range(r.count()):
                    selection, mesh = r.get('II'); mats = []
                    for _ in range(r.count()):
                        tex, tex1, type0, type1 = r.get('IIBB')
                        color_id, color, order, op, dst, src, blend, intensity = r.get('IIIIIIIf')
                        alpha, z, zw, two, light, ambient, unused = r.get('BBBBIIB')
                        mats.append({'texture': tex, 'alpha': alpha, 'order': order})
                    parts.append({'kind': kind, 'cloth': cloth, 'selection': selection, 'mesh': mesh, 'materials': mats})
        r.eof()
        return parts

    @lru_cache(maxsize=16)
    def mesh(self, id):
        r = self.read('mesh', id)
        groups, nodes, vb, levels = r.get('IIBI'); r.skip(16 + nodes * 64)
        if groups > 10000 or nodes > 10000 or levels > 100:
            raise AssetError('Invalid mesh counts')
        n = r.get('I'); stride = 48 if nodes else 40
        raw = r.take(n * stride)
        v = np.frombuffer(raw, dtype='<f4').reshape(n, stride // 4)
        normal = v[:, 7:10] if nodes else v[:, 3:6]
        uv = v[:, 10:12] if nodes else v[:, 6:8]
        indices = []
        for _ in range(groups):
            batches = []
            for level in range(levels):
                for _ in range(r.count()):
                    count, mat = r.get('IH')
                    ind = np.frombuffer(r.take(count * 2), dtype='<u2')
                    if level == 0:
                        if count % 3 or (count and ind.max() >= n):
                            raise AssetError('Invalid triangle indices')
                        batches.append(ind.reshape(-1, 3))
            indices.append(np.concatenate(batches) if batches else np.empty((0, 3), dtype=np.uint16))
        r.skip(max(0, levels - 1) * 4); r.eof()
        if not n or not any(len(x) for x in indices):
            raise AssetError('Mesh has no triangles')
        if not np.isfinite(v[:, :3]).all() or not np.isfinite(normal).all() or not np.isfinite(uv).all():
            raise AssetError('Nonfinite mesh data')
        return v[:, :3], normal, uv, indices

    @lru_cache(maxsize=16)
    def texture(self, id, stack=()):
        if id in stack or len(stack) > 32:
            raise AssetError('Cyclic texture reference')
        r = compressed(self.read('texture', id))
        refs = [r.get('I') for _ in range(r.count())]
        r.skip(r.count() * 24); r.skip(14); original, size = r.get('II')
        if size:
            return decode_image(r.take(size), original)
        if not refs:
            raise AssetError('Texture has no image')
        return self.texture(refs[0], stack + (id,))

    def buffer_index(self):
        if self._buffers is not None:
            return self._buffers
        r = Reader(self.path('Index/TClientList.LST').read_bytes()); n, total = r.count(), r.count()
        files = [r.take(r.count()).decode('cp1252') for _ in range(n)]; r.eof()
        result = {}
        for file in files:
            path = self.path('Data/' + file)
            with path.open('rb') as f:
                while f.tell() < path.stat().st_size:
                    offset = f.tell(); header = f.read(8)
                    if len(header) != 8: raise AssetError('Truncated image buffer')
                    original, size = struct.unpack('<II', header)
                    q = Reader(inflate(f.read(size), original)); id = q.get('I')
                    result[id] = ('Data/' + file, offset, size + 8)
        self._buffers = result
        return result

    @lru_cache(maxsize=16)
    def buffer(self, id):
        try: file, off, size = self.buffer_index()[id]
        except KeyError as e: raise AssetError(f'Missing UI buffer {id}') from e
        with self.path(file).open('rb') as f:
            f.seek(off); q = compressed(Reader(f.read(size)))
        q.get('I'); fmt, original, length = q.get('BII')
        return decode_image(q.take(length), original)

    def image_parts(self, id, stack=()):
        if id in stack or len(stack) > 32: raise AssetError('Cyclic UI image reference')
        q = compressed(self.read('image', id))
        refs = [q.get('I') for _ in range(q.count())]; q.skip(q.count() * 8)
        tick, fmt, size = q.get('IBI')
        if not size:
            if not refs: raise AssetError('UI image has no pixels')
            return self.image_parts(refs[0], stack + (id,))
        p = Reader(q.take(size)); n, width, height = p.count(), p.count(), p.count()
        if not width or not height or width * height > 16_777_216: raise AssetError('Invalid UI dimensions')
        tiles = []
        for _ in range(n):
            buffer = p.get('I')
            vertices = np.frombuffer(p.take(4 * 24), dtype='<f4').reshape(4, 6)
            tiles.append((buffer, vertices))
        p.skip(p.count()); p.eof()
        return width, height, tiles

    def choose_parts(self, object_id, equipment=()):
        parts = self.object(object_id)
        chosen = {}
        for part in sorted(parts, key=lambda x: (x['kind'], x['cloth'], x['selection'])):
            chosen.setdefault(part['kind'], part)
        for id in equipment:
            visual = self.visuals.get(self.items.get(id, {}).get('visual'))
            if not visual or visual['object']: continue
            match = next((p for p in parts if p['kind'] == visual['kind'] and
                          p['cloth'] == visual['cloth'] and p['selection'] == visual['selection']), None)
            if match: chosen[match['kind']] = match
        return list(chosen.values())
