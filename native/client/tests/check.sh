#!/bin/sh
# Run already-built C1 binaries. Build with tools/build-client-native.sh first.
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
TASK_BUILD="$TASK_ROOT/build/client/sanitized"
"$TASK_BUILD/c1-chart-transform-test" "$TASK_BUILD"
"$TASK_BUILD/c1-tcml-stack-test" "$TASK_BUILD/stack-test.tcml"
"$TASK_BUILD/c1-d3dx-test"
"$TASK_BUILD/c1-dump-format-test" "$TASK_BUILD/format-test.tsv"
if [ "${1:-}" != "--tables-only" ]; then
  "$TASK_BUILD/c1-engine-test" "$TASK_ROOT/native/packet/fixtures/scalars.hex"
  # C2: session events against loopback peers. Run in a scratch directory:
  # the original Encrypt writes LOL.txt into the working directory.
  (cd "$(mktemp -d)" && "$TASK_BUILD/c2-session-test")
  "$TASK_BUILD/c2-empty-device-test"
  # The generated stubs must match the shared d3d9.h.
  TASK_STUBS=$(mktemp)
  python3 "$TASK_ROOT/native/client/device/generate_stubs.py" "$TASK_STUBS" > /dev/null
  cmp -s "$TASK_STUBS" "$TASK_ROOT/native/client/device/stubs.inc" || { echo "FAIL native/client/device/stubs.inc is out of date"; exit 1; }
  rm -f "$TASK_STUBS"
  echo "PASS empty device stubs match d3d9.h"
fi
"$TASK_BUILD/c1-dump" "$TASK_ROOT/game" "$TASK_BUILD/native-dump.tsv"
python3 "$TASK_ROOT/native/client/tests/asset_coverage.py" "$TASK_ROOT" "$TASK_BUILD/asset-coverage.tsv" --selected-run-passed
