#!/usr/bin/env python3
"""Compare canonical resource groups without holding multi-GB dumps in memory.

Group order is part of the format. Fields within a resource may differ in order;
missing fields and duplicate fields are failures, never silently overwritten.
No numeric tolerances or driver-padding normalization are applied here.
"""
import argparse
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path


def groups(path, digest):
    with Path(path).open('rb') as stream:
        current = None
        fields = {}
        for number, raw in enumerate(stream, 1):
            digest.update(raw)
            parts = raw.rstrip(b'\n').split(b'\t')
            if len(parts) != 4:
                raise ValueError(f'{path}:{number}: expected four columns')
            dataset, key, field, value = (v.decode('ascii') for v in parts)
            group = dataset, key
            if current is not None and group != current:
                yield current, fields
                fields = {}
            current = group
            if field in fields:
                raise ValueError(f'{path}:{number}: duplicate field {field}')
            fields[field] = value
        if current is not None:
            yield current, fields


def compare(native, windows):
    hashes = [hashlib.sha256(), hashlib.sha256()]
    counts = Counter()
    datasets = {}
    examples = []
    errors = []
    devices = []
    def validated(path, digest, is_native):
        last = None
        for group, fields in groups(path, digest):
            last = group, fields
            if group[0] == 'device':
                if not is_native:
                    errors.append('unexpected Windows device instrumentation')
                devices.append(fields)
                if fields.get('backend') != 'cpu_empty_device':
                    errors.append(f'{path}: missing CPU empty-device label')
                for field in ('live_objects', 'live_bytes', 'failed_creations'):
                    if fields.get(field) != '0':
                        errors.append(f'{path}: device {field} is not zero')
                if any(k.startswith('not_executed.') and v != '0' for k, v in fields.items()):
                    errors.append(f'{path}: unsupported device calls')
                continue
            yield group, fields
        if not last or last[0] != ('result', '0') or last[1].get('status') != 'complete' or last[1].get('texture_failures') != '0':
            errors.append(f'{path}: missing successful terminal result')
        if is_native and len(devices) != 1:
            errors.append(f'{path}: expected one device group')
    left = validated(native, hashes[0], True)
    right = validated(windows, hashes[1], False)
    for a, b in itertools.zip_longest(left, right):
        if a is None or b is None or a[0] != b[0]:
            raise ValueError(f'group order/coverage differs: {a[0] if a else None} vs {b[0] if b else None}')
        group, af = a
        bf = b[1]
        dataset = datasets.setdefault(group[0], Counter())
        counts['groups'] += 1
        for field in sorted(af.keys() | bf.keys()):
            av, bv = af.get(field), bf.get(field)
            status = 'identical' if av == bv else ('missing_field' if av is None or bv is None else 'different')
            counts[status] += 1
            dataset[status] += 1
            if status != 'identical' and len(examples) < 100:
                examples.append({'dataset': group[0], 'key': group[1], 'field': field,
                                 'native': av, 'windows': bv})
    return {'native_sha256': hashes[0].hexdigest(), 'windows_sha256': hashes[1].hexdigest(),
            'counts': dict(counts), 'datasets': {k: dict(v) for k, v in datasets.items()},
            'first_differences': examples, 'native_device': devices, 'validation_errors': errors,
            'pass': not errors and not counts['different'] and not counts['missing_field']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native')
    parser.add_argument('windows')
    parser.add_argument('output')
    args = parser.parse_args()
    result = compare(args.native, args.windows)
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['counts'], sort_keys=True))
    raise SystemExit(0 if result['pass'] else 1)
