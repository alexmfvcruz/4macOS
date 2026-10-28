"""Small in-memory static renderer, not the game's animated D3D pipeline."""
import base64
import io
import math
import os
import shutil
import sys

import numpy as np
from PIL import Image

from asset_data import AssetError

BACKGROUND = (238, 240, 244, 255)


def raster(canvas, depth, screen, normals, uv, triangles, texture, alpha=0, lighting=True, wrap=True):
    height, width = canvas.shape[:2]
    tex = np.asarray(texture.convert('RGBA')); th, tw = tex.shape[:2]
    if lighting:
        light = np.array([-0.4, 0.8, -0.55]); light /= np.linalg.norm(light)
        norms = normals / np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-10)
        shades = np.clip(0.72 + 0.28 * (norms @ light), 0.50, 1.0)
    else:
        shades = np.ones(len(screen))
    for tri in triangles:
        q = screen[tri]
        xmin = max(0, int(np.floor(q[:, 0].min()))); xmax = min(width - 1, int(np.ceil(q[:, 0].max())))
        ymin = max(0, int(np.floor(q[:, 1].min()))); ymax = min(height - 1, int(np.ceil(q[:, 1].max())))
        if xmax < xmin or ymax < ymin: continue
        x, y = np.meshgrid(np.arange(xmin, xmax + 1) + 0.5, np.arange(ymin, ymax + 1) + 0.5)
        x0, y0 = q[0, :2]; x1, y1 = q[1, :2]; x2, y2 = q[2, :2]
        denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denom) < 1e-9: continue
        b0 = ((y1-y2)*(x-x2)+(x2-x1)*(y-y2))/denom
        b1 = ((y2-y0)*(x-x2)+(x0-x2)*(y-y2))/denom; b2 = 1-b0-b1
        z = b0*q[0, 2]+b1*q[1, 2]+b2*q[2, 2]
        u = b0*uv[tri[0], 0]+b1*uv[tri[1], 0]+b2*uv[tri[2], 0]
        v = b0*uv[tri[0], 1]+b1*uv[tri[1], 1]+b2*uv[tri[2], 1]
        if wrap: u, v = u % 1, v % 1
        tx = np.clip((u*tw).astype(int), 0, tw-1); ty = np.clip((v*th).astype(int), 0, th-1)
        sample = tex[ty, tx].copy()
        region = depth[ymin:ymax+1, xmin:xmax+1]
        mask = (b0 >= -1e-6) & (b1 >= -1e-6) & (b2 >= -1e-6) & (z > region) & (sample[:, :, 3] > alpha)
        brightness = b0*shades[tri[0]]+b1*shades[tri[1]]+b2*shades[tri[2]]
        sample[:, :, :3] = (sample[:, :, :3]*brightness[:, :, None]).clip(0, 255).astype(np.uint8)
        canvas[ymin:ymax+1, xmin:xmax+1][mask] = sample[mask]; region[mask] = z[mask]


def geometry(store, parts, width=700, height=550, yaw=140, elevation=19, plain=False):
    meshes = [(p, store.mesh(p['mesh'])) for p in parts]
    if not meshes: raise AssetError('No geometry selected')
    all_pos = np.concatenate([m[0] for _, m in meshes])
    yaw, elevation = math.radians(yaw), math.radians(elevation)
    toward = np.array([math.sin(yaw)*math.cos(elevation), math.sin(elevation), math.cos(yaw)*math.cos(elevation)])
    right = np.cross([0, 1, 0], toward); right /= np.linalg.norm(right); up = np.cross(toward, right)
    basis = np.stack([right, up, toward], axis=1)
    center = (all_pos.min(0)+all_pos.max(0))/2
    projected = (all_pos-center) @ basis
    offset = (projected[:, :2].min(0)+projected[:, :2].max(0))/2
    span = np.maximum(np.ptp(projected[:, :2], axis=0), 1e-6)
    scale = min(width*0.86/span[0], height*0.86/span[1])
    canvas = np.zeros((height, width, 4), dtype=np.uint8); depth = np.full((height, width), -np.inf)
    for part, (pos, normal, uv, groups) in meshes:
        p = (pos-center) @ basis; p[:, :2] -= offset
        screen = np.column_stack([width/2+p[:, 0]*scale, height/2-p[:, 1]*scale, p[:, 2]])
        for i, triangles in enumerate(groups):
            # Object.cpp uses texture-list index for submesh; order is draw sorting.
            mat = part['materials'][i] if i < len(part['materials']) else None
            texture = Image.new('RGBA', (1, 1), (190, 195, 205, 255))
            if mat and mat['texture'] and not plain:
                texture = store.texture(mat['texture'])
            raster(canvas, depth, screen, normal, uv, triangles, texture,
                   alpha=mat['alpha'] if mat and not plain else 0)
    if not canvas[:, :, 3].any(): raise AssetError('Selected geometry produced no visible pixels')
    result = Image.new('RGBA', (width, height), BACKGROUND)
    result.alpha_composite(Image.fromarray(canvas))
    return result.convert('RGB')


def ui_image(store, id):
    width, height, tiles = store.image_parts(id)
    canvas = np.zeros((height, width, 4), dtype=np.uint8); depth = np.full((height, width), -np.inf)
    for i, (buffer, v) in enumerate(tiles):
        if not np.isfinite(v).all(): raise AssetError('Nonfinite UI vertices')
        screen = v[:, :3].copy(); screen[:, :2] += 0.5; screen[:, 2] = i
        raster(canvas, depth, screen, np.zeros((4, 3)), v[:, 4:6],
               np.array([[0, 1, 2], [2, 1, 3]]), store.buffer(buffer), lighting=False, wrap=False)
    return Image.fromarray(canvas)


def flatten(image):
    bg = Image.new('RGBA', image.size, BACKGROUND); bg.alpha_composite(image.convert('RGBA'))
    return bg.convert('RGB')


def terminal(image, format='ansi', columns=None, stream=None):
    """ANSI half blocks work without a terminal image protocol or image files."""
    stream = stream or sys.stdout
    columns = columns or min(100, shutil.get_terminal_size((100, 35)).columns)
    if format == 'auto':
        format = 'kitty' if os.environ.get('TERM') == 'xterm-kitty' else ('iterm' if os.environ.get('TERM_PROGRAM') == 'iTerm.app' else 'ansi')
    if format == 'ansi':
        # Two pixels per cell; cells are roughly twice as tall as they are wide.
        pixel_height = max(2, round(image.height/image.width*columns))
        if pixel_height % 2: pixel_height += 1
        data = np.asarray(flatten(image).resize((columns, pixel_height), Image.Resampling.LANCZOS))
        for y in range(0, pixel_height, 2):
            pieces = []
            for x in range(columns):
                r, g, b = data[y, x]; br, bg, bb = data[y+1, x]
                pieces.append(f'\x1b[38;2;{r};{g};{b}m\x1b[48;2;{br};{bg};{bb}m▀')
            stream.write(''.join(pieces)+'\x1b[0m\n')
        stream.flush()
        return
    memory = io.BytesIO(); flatten(image).save(memory, format='PNG')
    data = base64.b64encode(memory.getvalue()).decode('ascii')
    if format == 'iterm':
        stream.write(f'\x1b]1337;File=inline=1;width={columns};preserveAspectRatio=1:{data}\a\n')
    elif format == 'kitty':
        chunks = [data[i:i+4096] for i in range(0, len(data), 4096)]
        for i, chunk in enumerate(chunks):
            header = f'a=T,f=100,t=d,c={columns},' if i == 0 else ''
            stream.write(f'\x1b_G{header}m={int(i < len(chunks)-1)};{chunk}\x1b\\')
        stream.write('\n')
    else: raise AssetError(f'Unknown terminal format {format}')
    stream.flush()
