#!/bin/sh
# Header/ABI check only. No backend build, installation, window or rendering.
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
TASK_OUT="$TASK_ROOT/runtime/legacy/c3-header-check"
TASK_DXVK=${1:-"$TASK_ROOT/deps/sources/dxvk-macos"}
mkdir -p "$TASK_OUT"
clang++ -std=c++14 -I "$TASK_DXVK/include/native/directx" -I "$TASK_DXVK/include/native/windows" "$TASK_ROOT/native/client/d3d9/tests/layout_probe.cpp" -o "$TASK_OUT/upstream-layout"
clang++ -std=c++14 -DFOURSTORY_CLIENT_NATIVE -I "$TASK_ROOT/native/client/d3d9/include" -I "$TASK_ROOT/native/client/compat/include" "$TASK_ROOT/native/client/d3d9/tests/layout_probe.cpp" -o "$TASK_OUT/project-layout"
"$TASK_OUT/upstream-layout" > "$TASK_OUT/upstream.tsv"
"$TASK_OUT/project-layout" > "$TASK_OUT/project.tsv"
cmp "$TASK_OUT/upstream.tsv" "$TASK_OUT/project.tsv"
for TASK_ORDER in normal reverse; do
  TASK_DEFINE=""
  if [ "$TASK_ORDER" = reverse ]; then TASK_DEFINE=-DFOURSTORY_REVERSE_INCLUDE_ORDER; fi
  clang++ -std=c++14 -DFOURSTORY_CLIENT_NATIVE $TASK_DEFINE -I "$TASK_ROOT/native/client/d3d9/include" -I "$TASK_ROOT/native/client/compat/include" -I "$TASK_ROOT/native/client/d3dx/include" "$TASK_ROOT/native/client/d3d9/tests/project_include_probe.cpp" -o "$TASK_OUT/includes-$TASK_ORDER"
  "$TASK_OUT/includes-$TASK_ORDER"
done
printf '%s\n' 'PASS selected DXVK header layouts and project include orders (header-only)'
