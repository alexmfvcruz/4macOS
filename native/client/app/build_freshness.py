"""Fast launch-time provenance checks for the binary's actual build inputs."""
import json
import shlex
import time
from pathlib import Path

RECORD = 'client-build.json'
MESSAGE = 'The client is older than its build inputs or its build is incomplete. Rebuild with python3 native/client/app/build.py before running.'

def metadata(path):
    stat = Path(path).stat()
    return [stat.st_mtime_ns, stat.st_size]

def parse_dependencies(text):
    text = text.replace('\\\n', ' ')
    return [Path(p) for p in shlex.split(text.split(':', 1)[1])]

def dependencies(depfile):
    return parse_dependencies(Path(depfile).read_text())

def inventory(folder):
    return sorted(str(p.resolve()) for p in Path(folder).glob('*.cpp'))

def capture_inputs(inputs, source_folders=()):
    return {'started_ns': time.time_ns(),
            'inputs': {str(Path(p).resolve()): metadata(p) for p in inputs},
            'inventories': {str(Path(p).resolve()): inventory(p) for p in source_folders}}

def record_build(out, inputs, source_folders=(), snapshot=None):
    out = Path(out)
    binary = out/'4story-client'
    binary_info = metadata(binary)
    records = {}
    for path in inputs:
        path = Path(path).resolve()
        info = metadata(path)
        if info[0] > binary_info[0]:
            raise RuntimeError(MESSAGE)
        if snapshot is not None:
            expected = snapshot['inputs'].get(str(path))
            if (expected is not None and info != expected) or (expected is None and info[0] > snapshot['started_ns']):
                raise RuntimeError('Build inputs changed during compilation. '+MESSAGE)
        records[str(path)] = info
    if not records:
        raise RuntimeError(MESSAGE)
    inventories = {str(Path(p).resolve()): inventory(p) for p in source_folders}
    if snapshot is not None and inventories != snapshot['inventories']:
        raise RuntimeError('Compilation inventory changed during the build. '+MESSAGE)
    record = {'version': 1, 'binary': binary_info, 'inputs': records,
              'inventories': inventories}
    temporary = out/(RECORD+'.new')
    temporary.write_text(json.dumps(record, sort_keys=True)+'\n')
    temporary.replace(out/RECORD)

def validate_build(out):
    out = Path(out)
    try:
        record = json.loads((out/RECORD).read_text())
        if record['version'] != 1 or metadata(out/'4story-client') != record['binary'] or not record['inputs']:
            raise ValueError('invalid build record')
        for path, expected in record['inputs'].items():
            if metadata(path) != expected or expected[0] > record['binary'][0]:
                raise ValueError('a compiled input changed')
        for folder, expected in record['inventories'].items():
            if inventory(folder) != expected:
                raise ValueError('the compilation inventory changed')
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        raise RuntimeError(MESSAGE) from error
