#!/usr/bin/env python3
"""Compare a native C4/C5 capture (PPM readback, or a PNG made from it) with a Windows capture (PNG).

  c4_login_compare.py NATIVE.ppm WINDOWS.png OUTDIR [--region NAME X Y W H]...

Both images must have the same size. Writes OUTDIR/compare.json (per region:
pixel count, changed pixels, mean and maximum absolute channel difference),
OUTDIR/side-<name>.png (native left, Windows right) and OUTDIR/diff-<name>.png
(absolute difference, amplified x4). No resizing, alignment or tolerance is
applied. Each named region also gets best_shift: the Windows offset in
-4..4 pixels with the smallest difference (0,0 means the same position). Pure Python (zlib only); PNG input: 8-bit RGB or RGBA, any filter.
"""
import json, struct, sys, zlib
from pathlib import Path


def read_ppm(path):
    data = Path(path).read_bytes()
    parts = data.split(b'\n', 3)
    if parts[0] != b'P6': raise SystemExit('not a binary PPM: %s' % path)
    w, h = map(int, parts[1].split())
    px = parts[3]
    return w, h, [px[(y * w) * 3:(y * w + w) * 3] for y in range(h)]


def read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n': raise SystemExit('not a PNG: %s' % path)
    i, idat, w = 8, b'', 0
    while i < len(data):
        n, kind = struct.unpack('>I4s', data[i:i + 8])
        body = data[i + 8:i + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
            if depth != 8 or ctype not in (2, 6) or body[12] != 0: raise SystemExit('unsupported PNG: %s' % path)
            bpp = 3 if ctype == 2 else 4
        elif kind == b'IDAT': idat += body
        i += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, at = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[at]; line = bytearray(raw[at + 1:at + 1 + stride]); at += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        rows.append(bytes(line) if bpp == 3 else bytes(v for j, v in enumerate(line) if j % 4 != 3))
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b''.join(b'\0' + r for r in rows)
    chunk = lambda k, b: struct.pack('>I', len(b)) + k + b + struct.pack('>I', zlib.crc32(k + b) & 0xffffffff)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def main():
    args = sys.argv[1:]
    if len(args) < 3: raise SystemExit(__doc__)
    native, windows, out = args[:3]
    regions = [('full', None)]
    rest = args[3:]
    while rest:
        if rest[0] != '--region' or len(rest) < 6: raise SystemExit(__doc__)
        regions.append((rest[1], tuple(map(int, rest[2:6])))); rest = rest[6:]
    nw, nh, nrows = read_png(native) if str(native).lower().endswith(".png") else read_ppm(native)
    ww, wh, wrows = read_png(windows)
    if (nw, nh) != (ww, wh): raise SystemExit('size differs: native %dx%d, Windows %dx%d' % (nw, nh, ww, wh))
    Path(out).mkdir(parents=True, exist_ok=True)
    result = {'native': str(native), 'windows': str(windows), 'width': nw, 'height': nh, 'regions': {}}
    for name, box in regions:
        x0, y0, rw, rh = box or (0, 0, nw, nh)
        changed = total = 0; maximum = 0; sad = 0
        side, diff = [], []
        for y in range(y0, y0 + rh):
            a = nrows[y][x0 * 3:(x0 + rw) * 3]; b = wrows[y][x0 * 3:(x0 + rw) * 3]
            d = bytes(abs(p - q) for p, q in zip(a, b))
            for x in range(rw):
                px = d[x * 3:x * 3 + 3]
                m = max(px)
                if m: changed += 1
                maximum = max(maximum, m)
            sad += sum(d); total += rw
            if box:
                side.append(a + b'\x40\x40\x40' * 4 + b)
                diff.append(bytes(min(255, v * 4) for v in d))
        result['regions'][name] = {'box': [x0, y0, rw, rh], 'pixels': total, 'changed_pixels': changed,
                                   'mean_abs_channel': round(sad / (3 * total), 3), 'max_abs_channel': maximum}
        if box:
            # Layout check: the shift of the Windows region (dx, dy in -4..4)
            # with the smallest mean difference to the native region.
            best = None
            for dy in range(-4, 5):
                for dx in range(-4, 5):
                    if not (0 <= x0 + dx and x0 + dx + rw <= nw and 0 <= y0 + dy and y0 + dy + rh <= nh): continue
                    t = 0
                    for y in range(y0, y0 + rh, 2):
                        a = nrows[y][x0 * 3:(x0 + rw) * 3]; b = wrows[y + dy][(x0 + dx) * 3:(x0 + dx + rw) * 3]
                        t += sum(abs(p - q) for p, q in zip(a, b))
                    if best is None or t < best[0]: best = (t, dx, dy)
            result['regions'][name]['best_shift'] = [best[1], best[2]]
        if box:
            write_png(Path(out) / ('side-%s.png' % name), rw * 2 + 4, rh, side)
            write_png(Path(out) / ('diff-%s.png' % name), rw, rh, diff)
    (Path(out) / 'compare.json').write_text(json.dumps(result, indent=1) + '\n')
    for name, r in result['regions'].items():
        print('%-10s %dx%d changed %d/%d mean %.3f max %d shift %s' % (name, r['box'][2], r['box'][3], r['changed_pixels'], r['pixels'], r['mean_abs_channel'], r['max_abs_channel'], r.get('best_shift', '-')))


if __name__ == '__main__':
    main()
