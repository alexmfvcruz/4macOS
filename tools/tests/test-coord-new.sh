#!/bin/sh
# Scenario tests for tools/coord-new.sh in a throwaway repository.
# Usage: sh tools/tests/test-coord-new.sh   (exit 0 when all pass)
set -u
S=$(cd "$(dirname "$0")/.." && pwd)/coord-new.sh
T="$(mktemp -d)/main repo"   # the space is deliberate
trap 'rm -rf "$(dirname "$T")"' EXIT
mkdir -p "$T/research/coordination" "$T/tools"; cp "$S" "$T/tools/"
cd "$T" || exit 1
git init -q; printf '/runtime/legacy/\n' > .gitignore
printf '# Joint work board\n\n## Current step\nC6 by Claude\n\n## Tasks\nSECRET-TASKS\n\n## Branches\ncodex/c6\n\n## Resource holders\nVM: Claude\n' > research/coordination/board.md
printf '# claude\n\n### 2026-10-17 10:00 STATUS S-1 one\ntext1\n### 2026-10-17 11:00 STATUS S-2 two\ntext2\n### 2026-10-17 12:00 STATUS S-3 three\ntext3\n' > research/coordination/claude.md
git add -A; git -c user.name=t -c user.email=t@t commit -qm init
git clone -q "$T" runtime/legacy/codex 2>/dev/null
C=runtime/legacy/codex/research/coordination/codex.md; mkdir -p "$(dirname "$C")"
printf '# codex\nintro text\n### 2026-10-16 STATUS S-0 old date-only\nold\n' > "$C"
for i in $(seq 1 12); do printf '### 2026-10-17 09:%02d RECORD R-%d e%d\nbody %d\n' "$i" "$i" "$i" "$i" >> "$C"; done
fails=0
ok() { if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; fails=$((fails+1)); fi; }
run() { o=$(sh tools/coord-new.sh claude "$@" 2>&1); rc=$?; }
n() { echo "$o" | grep -c "$1"; }

run
ok "first run prints the last 10 of 13" '[ $(n "^### .*\(RECORD\|S-0\)") = 10 ] && ! echo "$o" | grep -q "R-2 "'
ok "board prints current, branches, holders only" 'echo "$o" | grep -q "C6 by Claude" && echo "$o" | grep -q codex/c6 && echo "$o" | grep -q "VM: Claude" && ! echo "$o" | grep -q SECRET-TASKS'
ok "own file prints the last 2 entries" 'echo "$o" | grep -q "S-2 two" && echo "$o" | grep -q "S-3 three" && ! echo "$o" | grep -q "S-1 one"'
ok "marker written" '[ "$(cat runtime/legacy/coord-seen-claude)" = "2026-10-17 09:12" ]'

printf '### 2026-10-17 09:12 RECORD R-13 same-minute\nb\n### 2026-10-17 13:00 REQUEST Q-1 new\nplease\n' >> "$C"
run
ok "second run prints new and same-minute entries only" 'echo "$o" | grep -q "Q-1 new" && echo "$o" | grep -q "R-13 same" && ! echo "$o" | grep -q "R-11 "'
ok "marker advanced" '[ "$(cat runtime/legacy/coord-seen-claude)" = "2026-10-17 13:00" ]'
run
ok "third run repeats only the 13:00 entry" '[ $(n "^### .*\(Q-\|R-\)") = 1 ]'
run --no-mark --all
ok "--all prints all 15 entries" '[ $(n "^### 2026-10-1[67] ") = 17 ]'
ok "--no-mark keeps the marker" '[ "$(cat runtime/legacy/coord-seen-claude)" = "2026-10-17 13:00" ]'
ok "old date-only entry is its own entry" 'echo "$o" | grep -q "^### 2026-10-16 STATUS S-0"'

printf '### 2026-10-17 STATUS S-9 new date-only\nx\n' >> "$C"
run
ok "same-day date-only entry is shown" 'echo "$o" | grep -q "S-9 new date-only"'
ok "date-only entry does not move the marker" '[ "$(cat runtime/legacy/coord-seen-claude)" = "2026-10-17 13:00" ]'
ok "old date-only entry is not shown" '! echo "$o" | grep -q "S-0 old"'

printf '### 2026-10-17 14:00 RECORD R-14 caf\351 latin1\r\nbody\r\n' >> "$C"
run
ok "Latin-1 and CRLF entry handled" '[ $rc = 0 ] && echo "$o" | grep -q "R-14 caf"'

o=$(cd runtime/legacy/codex && sh "$T/tools/coord-new.sh" codex 2>&1)
ok "codex mode reads board and claude.md from main" 'echo "$o" | grep -q "C6 by Claude" && echo "$o" | grep -q "S-3 three" && echo "$o" | grep -q "R-14"'
ok "codex marker is in the clone" '[ -f runtime/legacy/codex/runtime/legacy/coord-seen-codex ]'

mv "$C" "$C.bak"; run; mv "$C.bak" "$C"
ok "missing other file fails" '[ $rc != 0 ] && echo "$o" | grep -q "No such file"'
o=$(cd / && sh "$T/tools/coord-new.sh" claude 2>&1); rc=$?
ok "outside a git repository fails" '[ $rc != 0 ]'
o=$(sh tools/coord-new.sh 2>&1); rc=$?
ok "missing argument prints usage" '[ $rc = 2 ] && echo "$o" | grep -q usage'

echo "$fails failed"
[ "$fails" = 0 ]
