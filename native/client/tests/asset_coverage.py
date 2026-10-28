#!/usr/bin/env python3
"""Inventory TCD inputs; completed extended logs preserve failures and gaps."""
import argparse
from collections import Counter
from pathlib import Path
import hashlib
import re

p = argparse.ArgumentParser()
p.add_argument('root', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--loads', type=Path)
p.add_argument('--npc-list', type=Path)
p.add_argument('--selected-run-passed', action='store_true', help='caller verified c1-dump exited successfully')
a = p.parse_args()
root = a.root.resolve()
assets = root / 'game'
paths = sorted(path for path in assets.rglob('*') if path.is_file() and path.suffix.lower() == '.tcd')
if a.npc_list:
    a.npc_list.write_text(''.join(path.relative_to(assets).as_posix() + '\n' for path in paths
                                if re.fullmatch(r'TNPC[0-9a-fA-F]{8,9}\.tcd', path.name)), encoding='utf8')
loaders = {}
source = root / 'source/4Story 5.0 Source Files/Client/TClient/TClientWnd.cpp'
for number, line in enumerate(source.read_bytes().decode('latin1').splitlines(), 1):
    match = re.search(r'CTChart::(Init\w+)\s*\(', line)
    if match:
        for filename in re.findall(r'([A-Za-z0-9_]+\.tcd)', line, re.I):
            loaders['tcd/' + filename.lower()] = match[1] + ';TClientWnd.cpp:' + str(number)
selected = {'tstring.tcd', 'titem.tcd', 'tskill.tcd', 'tenv.tcd', 'tlight.tcd',
            'tfog.tcd', 'tnode.tcd', 'tmap.tcd', 'tnpcglobal.tcd'}
loads = {}
if a.loads:
    lines = a.loads.read_text().splitlines()
    if not lines or lines[-1] != '__COMPLETE__\tc1-startup-dump\tPASS':
        p.error('extended load log has no successful completion marker')
    valid = {'PASS', 'EMPTY_ACCEPTED', 'FAIL_ARCHIVE', 'FAIL_FORMAT'}
    for line in lines[:-1]:
        fields = line.split('\t')
        if len(fields) != 3 or fields[2] not in valid or fields[0].lower() in loads:
            p.error('invalid or duplicate extended load-log entry: ' + line)
        loads[fields[0].lower()] = (fields[1], fields[2])
rows = []
for path in paths:
    relative = path.relative_to(assets).as_posix()
    loader = loaders.get(relative.lower(), 'UNMAPPED')
    if relative.lower() == 'tcd/tstring.tcd':
        loader = 'InitTString;TClient.cpp:312'
    status = 'NOT_TESTED'
    if a.loads:
        if relative.lower() in loads:
            runtime_loader, status = loads[relative.lower()]
            loader = runtime_loader + ';completed-run'
            if status == 'PASS':
                status = 'PASS_TEXT' if runtime_loader == 'text-first-line' else 'PASS_DECODED'
    elif a.selected_run_passed and relative.lower() in {'tcd/' + name for name in selected}:
        status = 'PASS_SELECTED_DUMP'
    rows.append((relative, str(path.stat().st_size), hashlib.sha256(path.read_bytes()).hexdigest(), loader, status))
with a.output.open('w', newline='\n') as stream:
    stream.write('asset\tbytes\tsha256\tloader_evidence\tload_status\n')
    for row in rows:
        stream.write('\t'.join(row) + '\n')
print('Coverage:', len(rows), '.tcd assets;', dict(sorted(Counter(row[-1] for row in rows).items())), a.output)
