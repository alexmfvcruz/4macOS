#!/usr/bin/env python3
"""Build the server part of the 4Story macOS release (C8): release/Server/.

Usage: python3 tools/release/build_server.py

Copies the four server programs (build/server/module, built by
tools/build-modules.sh), the control program and launchers from
tools/release/server/, and the clean databases that
tools/release/clean_database.py wrote to release/Server/database/.
Also copies tools/release/docs/RELEASE-README.md to release/README.md.
Writes release/Server/SHA256SUMS.txt (check inside release/Server with
`shasum -a 256 -c SHA256SUMS.txt`). Installs and downloads nothing.
"""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'release/Server'
MODULES = ROOT / 'build/server/module'
SOURCE = ROOT / 'tools/release/server'
ROLES = ['TControlSvr', 'TLoginSvr', 'TWorldSvr', 'TMapSvr']
COMMANDS = {'Install': 'install', 'Start': 'start', 'Stop': 'stop', 'Create Account': 'create-account',
            'Logs': 'logs', 'Status': 'status'}
# Shared libraries the programs may load; Install provides the Homebrew ones.
ALLOWED_LIBRARIES = ('/usr/lib/', '/System/Library/', '/opt/homebrew/opt/unixodbc/lib/')


def fail(text):
    raise SystemExit('build_server: ' + text)


def check_program(path):
    archs = subprocess.run(['/usr/bin/lipo', '-archs', str(path)], capture_output=True, text=True).stdout.split()
    if archs != ['arm64']:
        fail(f'{path.name}: expected arm64 only, found {archs}')
    lines = subprocess.run(['/usr/bin/otool', '-L', str(path)], capture_output=True, text=True).stdout.splitlines()[1:]
    for line in lines:
        library = line.strip().split(' (')[0]
        if not library.startswith(ALLOWED_LIBRARIES):
            fail(f'{path.name} links {library}, which a server Mac does not have')
    if subprocess.run(['/usr/bin/codesign', '-v', str(path)], capture_output=True).returncode:
        fail(f'{path.name}: invalid code signature')


def main():
    database = OUT / 'database'
    manifest = database / 'manifest.json'
    if not manifest.is_file():
        fail('release/Server/database is missing; run tools/release/clean_database.py first')
    for name, entry in json.loads(manifest.read_text())['databases'].items():
        if hashlib.sha256((database / entry['file']).read_bytes()).hexdigest() != entry['sha256']:
            fail(f'{name}: backup does not match manifest.json')
    for role in ROLES:
        check_program(MODULES / role)
    # Rebuild everything except the clean databases.
    for item in OUT.iterdir():
        if item.name != 'database':
            shutil.rmtree(item) if item.is_dir() and not item.is_symlink() else item.unlink()
    (OUT / 'bin').mkdir()
    for role in ROLES:
        target = OUT / 'bin' / role
        shutil.copy2(MODULES / role, target)
        # The debug map (OSO entries) names object files under this Mac's home
        # folder. strip -S removes it; the new ad-hoc signature replaces the
        # one strip invalidates.
        subprocess.run(['/usr/bin/strip', '-S', str(target)], check=True)
        subprocess.run(['/usr/bin/codesign', '-s', '-', '-f', str(target)], check=True, capture_output=True)
        if str(Path.home()).encode() in target.read_bytes():
            fail(f'{role} still contains a path under {Path.home()}')
        check_program(target)
    (OUT / 'lib').mkdir()
    shutil.copy2(SOURCE / 'server.py', OUT / 'lib/server.py')
    shutil.copy2(SOURCE / 'README.md', OUT / 'README.md')
    template = (SOURCE / 'command.sh').read_text()
    for title, command in COMMANDS.items():
        path = OUT / (title + '.command')
        path.write_text(template.replace('@TITLE@', title.lower()).replace('@COMMAND@', command))
        path.chmod(0o755)
    files = sorted(p for p in OUT.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt')
    sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(OUT)}\n' for p in files)
    (OUT / 'SHA256SUMS.txt').write_text(sums)
    if subprocess.run(['/usr/bin/shasum', '-a', '256', '-c', '-s', 'SHA256SUMS.txt'], cwd=OUT).returncode:
        fail('SHA256SUMS.txt check failed')
    # Main release README beside the DMG and the Server folder.
    shutil.copy2(ROOT / 'tools/release/docs/RELEASE-README.md', OUT.parent / 'README.md')
    size = sum(p.stat().st_size for p in files)
    print(f'Wrote {OUT.relative_to(ROOT)}: {len(files)} files, {size // (1024 * 1024)} MiB; SHA256SUMS.txt checked.')


if __name__ == '__main__':
    sys.exit(main())
