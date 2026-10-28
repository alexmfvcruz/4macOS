#!/usr/bin/env python3
"""Describe spatial detail on a common foreground interior; not a parity test."""
import argparse
import gzip
import json
from pathlib import Path


def image(path):
    raw = gzip.decompress(path.read_bytes()) if path.suffix == '.gz' else path.read_bytes()
    magic, size, depth, rgb = raw.split(b'\n', 3)
    width, height = map(int, size.split())
    if magic != b'P6' or depth != b'255' or len(rgb) != width * height * 3:
        raise ValueError('invalid P6 image')
    return width, height, [tuple(rgb[i:i+3]) for i in range(0, len(rgb), 3)]


def measure(before, after, reference):
    images = [image(path) for path in (before, after, reference)]
    w, h = images[0][:2]
    if any(im[:2] != (w, h) for im in images):
        raise ValueError('dimensions differ')
    pixels = [im[2] for im in images]
    mask = [all(im[i] != (32, 48, 64) for im in pixels) for i in range(w*h)]
    interior = [i for y in range(1, h-1) for x in range(1, w-1)
                for i in [y*w+x]
                if all(mask[(y+dy)*w+x+dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1))]
    if not interior:
        raise ValueError('empty foreground interior')
    result = {'interior_pixels': len(interior), 'method': 'Common before/after/Windows foreground; erode one pixel with 3x3 neighborhood; Rec709 luminance; mean absolute four-neighbor Laplacian. Descriptive texture-detail metric, not perceptual certification.'}
    for name, im in zip(('before', 'after', 'windows'), pixels):
        lum = [.2126*r+.7152*g+.0722*b for r, g, b in im]
        lap = sum(abs(4*lum[i]-lum[i-1]-lum[i+1]-lum[i-w]-lum[i+w]) for i in interior)/len(interior)
        mae = sum(abs(im[i][c]-pixels[2][i][c]) for i in interior for c in range(3))/(3*len(interior))
        result[name] = {'mean_absolute_laplacian': lap, 'rgb_mean_absolute_error': mae}
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    for name in ('before', 'after', 'reference'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    print(json.dumps(measure(a.before, a.after, a.reference), indent=2))
