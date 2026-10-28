#!/usr/bin/env python3
"""Negative runtime checks: silent loader omissions must not produce completion."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root, binary, output = map(Path, sys.argv[1:])
assets = root / 'game'
source = (root / 'native/client/tests/dump/startup_dump.cpp').read_text()
inputs = set(re.findall(r'ensureInput\(root,"([^"]+)"\)', source))
for case in ['missing-global', 'unsupported-mantle']:
    with tempfile.TemporaryDirectory(prefix=case + '-', dir=output) as temporary:
        fixture = Path(temporary)
        for relative in inputs:
            dest = fixture / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            if case == 'missing-global' and relative == 'Tcd/TNPCGlobal.tcd':
                continue
            if case == 'unsupported-mantle' and relative == 'Tcd/TMantleCoord.tcd':
                dest.write_bytes(b'\xff\xff\xff\xff')
            else:
                dest.symlink_to(assets / relative)
        log = fixture / 'loads.tsv'
        result = subprocess.run([str(binary), str(fixture), str(fixture/'dump.tsv'), str(log)], capture_output=True, text=True)
        assert result.returncode != 0, case + ': unexpectedly succeeded'
        assert '__COMPLETE__' not in log.read_text(), case + ': false completion marker'
        assert 'TNPCGlobal.tcd\tInitTNPCGlobal\tPASS' not in log.read_text(), case + ': false global PASS'
        if case == 'unsupported-mantle':
            assert 'unsupported mantle coordinate version' in result.stderr, result.stderr
print('PASS missing-global and unsupported-mantle negative runtime checks')
