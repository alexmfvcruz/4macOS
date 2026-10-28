#!/usr/bin/env python3
"""Field differences between two C5 SQL snapshots (tools/c5_sql_snapshot.py).

    c5_sql_diff.py A.json B.json [--ignore FIELD ...]

Rows are matched per table by position after sorting on all columns except
the ignored ones. Prints table row counts and every differing field, without
values of fields named *passwd* or ending in pw. Exit 0 only when nothing differs.
"""
import json, sys

def load(p): return json.load(open(p))

def main():
    args = sys.argv[1:]
    if len(args) < 2: raise SystemExit(__doc__)
    a, b = load(args[0]), load(args[1])
    ignore = set()
    i = 2
    while i < len(args):
        if args[i] == '--ignore': ignore.add(args[i + 1]); i += 2
        else: raise SystemExit(__doc__)
    differences = 0
    for table in sorted(set(a['tables']) | set(b['tables'])):
        ra, rb = a['tables'].get(table, []), b['tables'].get(table, [])
        key = lambda r: json.dumps({k: v for k, v in sorted(r.items()) if k not in ignore}, sort_keys=True, default=str)
        ra, rb = sorted(ra, key=key), sorted(rb, key=key)
        if len(ra) != len(rb):
            print(f'{table}: rows {len(ra)} -> {len(rb)}'); differences += 1
        for x, y in zip(ra, rb):
            for f in sorted(set(x) | set(y)):
                if f in ignore or x.get(f) == y.get(f): continue
                secret = 'passwd' in f.lower() or f.lower().endswith('pw')
                print(f'{table}.{f}: ' + ('<differs>' if secret else f'{x.get(f)!r} -> {y.get(f)!r}'))
                differences += 1
    print('differences', differences)
    sys.exit(1 if differences else 0)

if __name__ == '__main__':
    main()
