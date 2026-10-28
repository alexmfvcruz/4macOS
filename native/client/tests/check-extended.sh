#!/bin/sh
# Build with tools/build-client-native.sh --target tables (or the default all).
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
TASK_BUILD="$TASK_ROOT/build/client/sanitized"
python3 "$TASK_ROOT/native/client/tests/asset_coverage.py" "$TASK_ROOT" "$TASK_BUILD/asset-coverage-inventory.tsv" --npc-list "$TASK_BUILD/npc-files.txt"
"$TASK_BUILD/c1-startup-dump" "$TASK_ROOT/game" "$TASK_BUILD/startup-dump.tsv" "$TASK_BUILD/startup-loads.tsv" "$TASK_BUILD/npc-files.txt"
python3 "$TASK_ROOT/native/client/tests/asset_coverage.py" "$TASK_ROOT" "$TASK_BUILD/extended-asset-coverage.tsv" --loads "$TASK_BUILD/startup-loads.tsv"
python3 "$TASK_ROOT/native/client/tests/startup_failure_test.py" "$TASK_ROOT" "$TASK_BUILD/c1-startup-dump" "$TASK_BUILD"
