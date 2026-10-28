#!/bin/sh
# Print the coordination context that a new session needs:
#   1. board sections "Current*", "Branches", "Resource holders"
#   2. the last 2 entries of the agent's own file
#   3. entries of the other agent's file newer than the last run
#      (the last 10 entries when no marker exists)
# Usage: sh tools/coord-new.sh claude|codex [--all] [--no-mark]
#   --all      print all entries of the other file
#   --no-mark  do not update the seen marker
# The marker is runtime/legacy/coord-seen-<agent> (ignored by git).
set -eu
agent=${1:-}
case "$agent" in claude|codex) ;; *) echo "usage: $0 claude|codex [--all] [--no-mark]" >&2; exit 2 ;; esac
shift
root=$(git rev-parse --show-toplevel)
if [ "$agent" = claude ]; then
  main=$root
  own=$root/research/coordination/claude.md
  other=$root/runtime/legacy/codex/research/coordination/codex.md
else
  main=$(git -C "$root" remote get-url origin)
  own=$root/research/coordination/codex.md
  other=$main/research/coordination/claude.md
fi
board=$main/research/coordination/board.md
mkdir -p "$root/.local"
python3 - "$agent" "$board" "$own" "$other" "$root/runtime/legacy/coord-seen-$agent" "$@" <<'EOF'
import os, re, sys
agent, board, own, other, marker = sys.argv[1:6]
flags = set(sys.argv[6:])
head = re.compile(r'^### (\d{4}-\d{2}-\d{2})(?: (\d{2}:\d{2}))? ')
# A heading without a time sorts last in its day, so it shows until the date
# changes and is never skipped; it does not move the marker.

def entries(path):
    out, cur = [], None
    for line in open(path, encoding='utf-8', errors='replace'):
        m = head.match(line)
        if m:
            cur = [m.group(1) + ' ' + (m.group(2) or '99:99'), line]
            out.append(cur)
        elif cur:
            cur[1] += line
    return out

def section_wanted(title):
    return title.startswith('Current') or title in ('Branches', 'Resource holders')

print('=== board (current sections) ===')
keep = False
for line in open(board, encoding='utf-8', errors='replace'):
    if line.startswith('## '):
        keep = section_wanted(line[3:].strip())
    if keep:
        sys.stdout.write(line)

print('\n=== own file: last 2 entries (%s) ===' % os.path.basename(own))
for _, text in entries(own)[-2:]:
    sys.stdout.write(text)

seen = ''
if os.path.exists(marker) and '--all' not in flags:
    seen = open(marker).read().strip()
other_entries = entries(other)
# >= keeps entries written in the same minute as the last run
if seen or '--all' in flags:
    new = [e for e in other_entries if e[0] >= seen]
else:
    new = other_entries[-10:]  # first run: no marker yet
print('\n=== other file: %d new entries since %s (%s) ===' % (len(new), seen or ('start' if '--all' in flags else 'first run, last 10'), os.path.basename(other)))
for _, text in new:
    sys.stdout.write(text)

if '--no-mark' not in flags and other_entries:
    timed = [e[0] for e in other_entries if not e[0].endswith('99:99')]
    if timed:
        open(marker, 'w').write(max(timed) + '\n')
EOF
