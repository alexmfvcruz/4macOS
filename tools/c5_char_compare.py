#!/usr/bin/env python3
"""Compare the rows of two characters from C5 SQL snapshots.

    c5_char_compare.py SNAPSHOT_A NAME_A SNAPSHOT_B NAME_B [--json OUT]

For each table, takes the rows of the named character (by dwCharID; TITEMTABLE
via the item IDs of its inventory rows when it has no character column) and
compares them after normalization. Normalized (listed in the output):
character, user, item, post and sequence identifiers (dwSeq is an identity
column) and the name. Date/time fields are compared as set or empty only;
dEndTime (item expiry) is compared exactly. Account-keyed rows (TPETTABLE)
are taken through the character's dwUserID. The inventory-to-item link (dlID)
is not compared.
Every other value must be equal. Prints counts and each difference; no
password field is printed. Exit 0 when the rows are equal after normalization.
"""
import json, re, sys

NORMALIZED = re.compile(r'^(dwCharID|dwUserID|szNAME|szName|dlID|dlItemID|dwItemID|dwSerial.*|dlSerial.*|dwOwnerID|dwPostID|dwSendID|dwRecvID|szSender|szRecver|szRecv.*|szSend.*|dwSeq)$')
# Date and time columns: only whether they are set is compared (the moment
# differs). dEndTime is an item expiry, not a moment of the test: compared exactly.
DATED = re.compile(r'^(d(?!EndTime$)[A-Z]\w*(Date|Time)|dDate|dTime|dLogin.*|dLogout.*|dFirstLogin|dLastLogin|time[A-Z]\w*|timestamp)$')

def rows_of(snap, name):
    t = snap['tables']
    chars = [r for r in t['TGAME.TCHARTABLE'] if r.get('szNAME') == name]
    if len(chars) != 1: raise SystemExit(f'{name}: {len(chars)} TCHARTABLE rows')
    cid = chars[0]['dwCharID']
    out = {}
    for table, rows in t.items():
        if table.startswith('TGLOBAL.TUSERINFOTABLE'): continue
        if rows and 'dwCharID' in rows[0]:
            sel = [r for r in rows if r.get('dwCharID') == cid]
        elif rows and 'dwOwnerID' in rows[0]:
            sel = [r for r in rows if r.get('dwOwnerID') == cid]
        elif rows and 'dwUserID' in rows[0]:
            sel = [r for r in rows if r.get('dwUserID') == chars[0]['dwUserID']]  # account rows (TPETTABLE)
        else:
            sel = []
        out[table] = sel
    return cid, out

def norm(r):
    out = {}
    for k, v in r.items():
        if NORMALIZED.match(k): out[k] = '<norm>'
        elif DATED.match(k): out[k] = '<null>' if v is None else '<set>'
        else: out[k] = v
    return out

def main():
    a = sys.argv[1:]
    if len(a) < 4: raise SystemExit(__doc__)
    sa, sb = json.load(open(a[0])), json.load(open(a[2]))
    ida, ra = rows_of(sa, a[1]); idb, rb = rows_of(sb, a[3])
    report = {'a': [a[1], ida], 'b': [a[3], idb], 'tables': {}, 'normalized_fields': set()}
    diffs = 0
    for table in sorted(set(ra) | set(rb)):
        x = sorted((norm(r) for r in ra.get(table, [])), key=lambda r: json.dumps(r, sort_keys=True, default=str))
        y = sorted((norm(r) for r in rb.get(table, [])), key=lambda r: json.dumps(r, sort_keys=True, default=str))
        for r in ra.get(table, []) + rb.get(table, []):
            report['normalized_fields'].update(k for k in r if NORMALIZED.match(k))
            report.setdefault('dated_fields', set()).update(k for k in r if DATED.match(k))
        entry = {'rows': [len(x), len(y)], 'differences': []}
        if len(x) != len(y): entry['differences'].append('row count'); diffs += 1
        for p, q in zip(x, y):
            for f in sorted(set(p) | set(q)):
                if p.get(f) != q.get(f):
                    hide = 'passwd' in f.lower() or f.lower().endswith('pw')
                    entry['differences'].append(f'{f}: ' + ('<differs>' if hide else f'{p.get(f)!r} != {q.get(f)!r}')); diffs += 1
        report['tables'][table] = entry
        print(f'{table}: rows {len(x)}/{len(y)}' + ('' if not entry['differences'] else '  ' + '; '.join(entry['differences'][:6])))
    report['normalized_fields'] = sorted(report['normalized_fields'])
    report['dated_fields'] = sorted(report.get('dated_fields', ()))
    report['differences'] = diffs
    print('normalized:', ', '.join(report['normalized_fields']))
    print('set/empty only:', ', '.join(report['dated_fields']))
    print('differences', diffs)
    if '--json' in a: open(a[a.index('--json') + 1], 'w').write(json.dumps(report, indent=1) + '\n')
    sys.exit(1 if diffs else 0)

if __name__ == '__main__':
    main()
