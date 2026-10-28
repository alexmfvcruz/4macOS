#!/usr/bin/env python3
"""Make standard ZIP inputs for a source checkout; never include live state."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('workspace', ROOT/'tools/workspace.py')
workspace = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(workspace)
# Installation receipts and installer network/log state are not build inputs.
EXCLUDED = {
    'deps/odbc/INSTALL_RECEIPT.json',
    'deps/vulkan-sdk/network.xml',
    'deps/vulkan-sdk/InstallationLog.txt',
}


def allowed(rel):
    return rel.as_posix() not in EXCLUDED


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(8 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def validate_destination(output):
    output = output.resolve()
    for subtree in ('deps', 'game', 'server/database'):
        if output.is_relative_to((ROOT/subtree).resolve()):
            raise ValueError('Output must be outside input subtree: ' + subtree)
    names = ['4macOS-dependencies.zip', '4macOS-game-data.zip',
             '4macOS-database-seeds.zip', 'SHA256SUMS.txt', 'packages.json']
    for name in names + [n + '.partial' for n in names if n.endswith('.zip')]:
        path = output/name
        if path.exists() or path.is_symlink():
            raise ValueError('Refusing to overwrite ' + name)
    return output


def package(output):
    output = validate_destination(output)
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for name, subtree in [('4macOS-dependencies.zip', 'deps'),
                          ('4macOS-game-data.zip', 'game'),
                          ('4macOS-database-seeds.zip', 'server/database')]:
        target = output/name
        if target.exists() or target.with_suffix('.zip.partial').exists():
            raise ValueError('Refusing to overwrite ' + name)
        temporary = target.with_suffix('.zip.partial')
        entries = []
        with zipfile.ZipFile(temporary, 'x', zipfile.ZIP_DEFLATED, compresslevel=1,
                             strict_timestamps=False, allowZip64=True) as archive:
            for path in workspace.archive_files(ROOT/subtree):
                rel = path.relative_to(ROOT)
                if not allowed(rel):
                    continue
                archive.write(path, rel.as_posix())
                entries.append({'path': rel.as_posix(), 'bytes': path.stat().st_size,
                                'sha256': sha256(path)})
            archive.writestr('INPUTS-MANIFEST.json', json.dumps({'files': entries}, indent=2)+'\n')
        # Read and decompress every member: catches corruption before delivery.
        with zipfile.ZipFile(temporary) as archive:
            broken = archive.testzip()
            if broken:
                raise ValueError('ZIP verification failed: ' + broken)
        temporary.rename(target)
        result = {'file': name, 'bytes': target.stat().st_size, 'files': len(entries),
                  'uncompressed_bytes': sum(x['bytes'] for x in entries), 'sha256': sha256(target)}
        results.append(result)
        print(json.dumps(result), flush=True)
    (output/'SHA256SUMS.txt').write_text(''.join(r['sha256']+'  '+r['file']+'\n' for r in results))
    (output/'packages.json').write_text(json.dumps(results, indent=2)+'\n')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'.publication/packages')
    args = parser.parse_args()
    package(args.output)


if __name__ == '__main__':
    main()
