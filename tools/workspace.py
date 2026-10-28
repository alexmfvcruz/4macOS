#!/usr/bin/env python3
"""Build and run the visible, relocatable 4Story source workspace."""
import argparse
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    return subprocess.run([str(x) for x in command], cwd=ROOT, check=True, **kwargs)


def build(args):
    targets = ['accounts', 'server', 'client'] if args.target == 'all' else [args.target]
    for target in targets:
        if target == 'client':
            cmd = [sys.executable, ROOT/'native/client/app/build.py', '--jobs', str(args.jobs)]
            if args.channel:
                cmd += ['--channel', args.channel]
            if args.version:
                cmd += ['--version', args.version]
            if args.checks:
                cmd += ['--sanitize']
            run(cmd)
        elif target == 'accounts':
            run([sys.executable, ROOT/'native/accounts/build.py'])
        elif target == 'server':
            run(['/bin/sh', ROOT/'tools/build-modules.sh'])
            dest = ROOT/'server/bin'
            dest.mkdir(parents=True, exist_ok=True)
            for role in ('TControlSvr', 'TLoginSvr', 'TWorldSvr', 'TMapSvr'):
                shutil.copy2(ROOT/'build/server/module'/role, dest/role)


def client(args):
    command = [sys.executable, ROOT/'native/client/app/run.py', '--game', ROOT/'game']
    if not args.checks:
        command.append('--plain')
    if args.seconds:
        command += ['--seconds', str(args.seconds)]
    if args.server:
        command += ['--', args.server, str(args.port), '1']
    run(command)


def server(args):
    if not args.action:
        print('Server commands: install, start, stop, status, create-account, logs')
        print('Example: ./4story.command server install')
        print('Install prepares prerequisites and asks for Microsoft licenses; profile: 4story-port.')
        return
    run([sys.executable, ROOT/'tools/release/server/server.py', args.action, *args.arguments])


def doctor():
    run([sys.executable, ROOT/'tools/portable_checks.py'])


def archive_files(directory=None, ancestors=frozenset()):
    """Materialize internal directory/file links; reject escapes and cycles."""
    directory = ROOT if directory is None else directory
    resolved = directory.resolve(strict=True)
    try:
        resolved.relative_to(ROOT)
    except ValueError:
        raise RuntimeError('Archive contains external symlink: ' + str(directory))
    if resolved in ancestors:
        raise RuntimeError('Archive contains a directory link cycle: ' + str(directory))
    for path in sorted(directory.iterdir()):
        if path.name in {'.git', '.local', '__pycache__', '.DS_Store', 'Thumbs.db',
                         '.package-cache', '.package-cache-mutate', '.global-cache'}:
            continue
        if directory == ROOT and path.name in {'build', 'runtime', '.publication'}:
            continue
        if path == ROOT/'server/bin':
            continue
        if path.suffix in {'.pyc', '.log', '.dmp'}:
            continue
        if path.parent == ROOT/'game' and path.name in {'log.txt', 'npc_error.txt', 'LOL.txt'}:
            continue
        try:
            real_parts = path.resolve(strict=True).relative_to(ROOT).parts
        except (OSError, ValueError):
            raise RuntimeError('Archive contains an external or broken link: ' + str(path))
        if real_parts and (real_parts[0] in {'build', 'runtime', '.publication'} or real_parts[:2] == ('server', 'bin')):
            raise RuntimeError('Archive link points into generated/private state: ' + str(path))
        if path.is_dir():
            yield from archive_files(path, ancestors | {resolved})
        elif path.is_file():
            yield path
        else:
            raise RuntimeError('Unsupported archive entry: ' + str(path))


def archive(args):
    target = args.output or ROOT.parent/(ROOT.name+'.zip')
    target = target.resolve()
    try:
        target.relative_to(ROOT)
    except ValueError:
        pass
    else:
        raise RuntimeError('Write the archive outside the workspace')
    checksum = target.with_suffix(target.suffix+'.sha256')
    temporary = target.with_name(target.name+'.partial')
    if target.exists() or checksum.exists() or temporary.exists():
        raise RuntimeError('Archive/checksum already exists; select a different output name')
    start = time.monotonic()
    count = 0
    with zipfile.ZipFile(temporary, 'x', zipfile.ZIP_DEFLATED, compresslevel=1,
                         allowZip64=True, strict_timestamps=False) as out:
        for path in archive_files():
            out.write(path, ROOT.name+'/'+str(path.relative_to(ROOT)))
            count += 1
            if count % 5000 == 0:
                print('Archived', count, 'files', flush=True)
    import hashlib
    h = hashlib.sha256()
    with temporary.open('rb') as source:
        for chunk in iter(lambda: source.read(8 << 20), b''):
            h.update(chunk)
    temporary.rename(target)
    with checksum.open('x') as out:
        out.write(h.hexdigest()+'  '+target.name+'\n')
    print('Archive:', target, 'files:', count, 'seconds:', round(time.monotonic()-start, 1))
    print('SHA-256:', h.hexdigest())


def menu():
    while True:
        print('\n4Story source workspace\n1. Play\n2. Build all\n3. Check workspace\n4. Server help\n5. Quit')
        choice = input('Choose: ').strip()
        choices = {'1': ['client'], '2': ['build', 'all'], '3': ['doctor'], '4': ['server']}
        if choice == '5':
            return
        if choice in choices:
            main(choices[choice])


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command')
    b = sub.add_parser('build')
    b.add_argument('target', choices=('all', 'client', 'server', 'accounts'))
    b.add_argument('--channel', choices=('dev', 'beta', 'stable'))
    b.add_argument('--version')
    b.add_argument('--jobs', type=int, default=3)
    b.add_argument('--checks', action='store_true')
    c = sub.add_parser('client')
    c.add_argument('--server', help='Server IPv4; default is this Mac routed LAN address')
    c.add_argument('--port', type=int, default=4815)
    c.add_argument('--seconds', type=float, default=0)
    c.add_argument('--checks', action='store_true', help='Run the client built with memory checks')
    s = sub.add_parser('server')
    s.add_argument('action', nargs='?', choices=('install', 'start', 'stop', 'status', 'create-account', 'logs'))
    s.add_argument('arguments', nargs=argparse.REMAINDER)
    sub.add_parser('doctor')
    sub.add_parser('setup', help='Validate the supplied offline project inputs; installs nothing')
    z = sub.add_parser('archive')
    z.add_argument('--output', type=Path)
    args = p.parse_args(argv)
    try:
        if args.command == 'build':
            if args.jobs < 1:
                p.error('--jobs must be positive')
            build(args)
        elif args.command == 'client':
            if not 1 <= args.port <= 65535 or not math.isfinite(args.seconds) or args.seconds < 0:
                p.error('Invalid port or duration')
            client(args)
        elif args.command == 'server':
            server(args)
        elif args.command in ('doctor', 'setup'):
            doctor()
        elif args.command == 'archive':
            archive(args)
        else:
            menu()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print('4Story:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
