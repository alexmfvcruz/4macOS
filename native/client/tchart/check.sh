#!/bin/sh
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
TASK_OUT="$TASK_ROOT/build/client/sanitized"
TASK_LIB="$TASK_ROOT/source/4Story 5.0 Source Files/Lib/Own"
# Rebuild the real chart/TCML archives with sanitizers before this focused test.
python3 "$TASK_ROOT/native/client/build/build.py" --target tables
clang++ -std=gnu++14 -arch arm64 -ffp-contract=off -g -O1 -fno-omit-frame-pointer \
  -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=non-pod-varargs \
  -DFOURSTORY_CLIENT_NATIVE -DFOURSTORY_CLIENT_64 -DFOURSTORY_CLIENT_MOD \
  -include fourstory_client_prefix.h -Wno-nonportable-include-path -Wno-ignored-pragmas \
  -I "$TASK_ROOT/native/client/d3d9/include" -I "$TASK_ROOT/native/client/compat/include" \
  -I "$TASK_ROOT/native/client/d3dx/include" -I "$TASK_ROOT/native/client/build/include" \
  -I "$TASK_LIB/TChart/TChart" -I "$TASK_LIB/TCML/TCML" -I "$TASK_LIB/TProtocol/include" \
  "$TASK_ROOT/native/client/tchart/tests/format_test.cpp" \
  "$TASK_OUT/libchart.a" "$TASK_OUT/libtcml.a" -lz -o "$TASK_OUT/c4-chart-format-test"
"$TASK_OUT/c4-chart-format-test"
