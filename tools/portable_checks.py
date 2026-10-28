#!/usr/bin/env python3
"""Read-only check of standalone inputs, links, data and native library paths."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    required = [
        'game/Tcd/TString.tcd', 'game/TClientCmd.tif',
        'deps/sdl3/include/SDL3/SDL.h', 'deps/sdl3/lib/libSDL3.0.dylib',
        'deps/dxvk/lib/libdxvk_d3d9.0.dylib',
        'deps/moltenvk/MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib',
        'deps/vulkan-sdk/macOS/lib/libvulkan.1.dylib',
        'deps/tools/rust/bin/rustc', 'deps/tools/rust/bin/cargo',
        'deps/odbc/include/sql.h', 'deps/odbc/lib/libodbc.2.dylib',
        'native/client/app/build.py', 'tools/build-modules.sh',
        'tools/release/server/server.py', 'server/database/manifest.json',
    ]
    failures = [str(p) for p in required if not (ROOT/p).is_file()]
    external = []
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in {'.git', '__pycache__'}]
        if Path(base) == ROOT:
            dirs[:] = [d for d in dirs if d not in {'build', 'runtime'}]
        for name in [*dirs, *files]:
            path = Path(base)/name
            if path.is_symlink():
                try:
                    path.resolve(strict=True).relative_to(ROOT)
                except (ValueError, OSError):
                    external.append(str(path.relative_to(ROOT)))
    if (ROOT/'.local').exists():
        failures.append('unexpected .local directory')
    manifest = ROOT/'server/database/manifest.json'
    if manifest.is_file():
        for name, info in json.loads(manifest.read_text())['databases'].items():
            p = manifest.parent/info['file']
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != info['sha256']:
                failures.append('database seed: '+name)
    for command in ('clang++', 'codesign', 'python3'):
        if not shutil.which(command):
            failures.append('system prerequisite: '+command)
    print('Workspace:', ROOT)
    print('Missing/invalid inputs:', failures)
    print('External/broken symlinks:', external)
    print('Server setup uses its own profile/container/keychain; standard game ports must be free.')
    print('Server install additionally needs Docker/Colima, Rosetta and the SQL Server/ODBC licenses.')
    if failures or external:
        return 1
    print('PASS project input check (not a gameplay or first-time server installation test)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
