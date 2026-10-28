#!/usr/bin/env python3
"""Compare probe RGB readbacks exactly; list every differing pixel, without rescaling."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import zlib


def read_ppm(path):
    data = Path(path).read_bytes()
    # This intentionally accepts only the portable probe's P6 output contract.
    parts = data.split(b'\n', 3)
    if len(parts) != 4 or parts[0] != b'P6' or parts[2] != b'255':
        raise ValueError('expected probe P6 readback with 8-bit RGB: '+str(path))
    dimensions = parts[1].split()
    if len(dimensions) != 2:
        raise ValueError('invalid readback dimensions: '+str(path))
    width, height = map(int, dimensions)
    if not (0 < width <= 8192 and 0 < height <= 8192) or len(parts[3]) != width*height*3:
        raise ValueError('invalid readback dimensions or byte count: '+str(path))
    return width, height, parts[3], hashlib.sha256(data).hexdigest()


def png(rgb, width, height):
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data)&0xffffffff)
    rows = b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')


def compare(native, reference, prefix):
    nw, nh, nrgb, nsha = read_ppm(native)
    rw, rh, rrgb, rsha = read_ppm(reference)
    if (nw,nh) != (rw,rh):
        raise ValueError('readback dimensions differ; no resize or crop permitted')
    prefix = Path(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    # A zero-difference image is black. Differences use their exact absolute RGB
    # magnitude; there is no amplification, rounding allowance or hidden mask.
    delta = bytes(abs(nrgb[i]-rrgb[i]) for i in range(len(nrgb)))
    differences = 0
    with Path(str(prefix)+'-pixels.tsv').open('w',newline='') as out:
        writer = csv.writer(out, delimiter='\t', lineterminator='\n')
        writer.writerow(['x','y','native_r','native_g','native_b','reference_r','reference_g','reference_b','delta_r','delta_g','delta_b'])
        for pixel in range(nw*nh):
            offset = pixel*3
            if any(delta[offset:offset+3]):
                differences += 1
                writer.writerow([pixel%nw,pixel//nw,*nrgb[offset:offset+3],*rrgb[offset:offset+3],*delta[offset:offset+3]])
    Path(str(prefix)+'-delta.png').write_bytes(png(delta,nw,nh))
    for label, rgb in [('native',nrgb),('reference',rrgb)]:
        Path(str(prefix)+'-'+label+'.png').write_bytes(png(rgb,nw,nh))
    result = {'native':str(Path(native).resolve()),'reference':str(Path(reference).resolve()),
              'native_ppm_sha256':nsha,'reference_ppm_sha256':rsha,'width':nw,'height':nh,
              'pixels_compared':nw*nh,'different_pixels':differences,'maximum_channel_delta':max(delta),
              'match':differences==0,'method':'Exact RGB; every differing pixel listed; no tolerance, resizing, color conversion or alignment'}
    Path(str(prefix)+'-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('native', type=Path)
    p.add_argument('reference', type=Path, help='actual Windows probe PPM; screenshot evidence is separate')
    p.add_argument('output_prefix', type=Path)
    a = p.parse_args()
    try:
        result = compare(a.native,a.reference,a.output_prefix)
    except (OSError,ValueError) as error:
        p.exit(2,str(error)+'\n')
    print(('PASS' if result['match'] else 'DIFFERENT')+': '+str(result['different_pixels'])+' / '+str(result['pixels_compared'])+' pixels differ; maximum channel delta '+str(result['maximum_channel_delta']))
    return 0 if result['match'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
