#!/bin/sh
# Q-C12: compile the original resource-loader umbrella, no substitutes.
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
clang++ -std=gnu++14 -ffp-contract=off -Wno-address-of-temporary -Wno-ignored-pragmas \
  -DFOURSTORY_CLIENT_NATIVE -DFOURSTORY_CLIENT_64 -DFOURSTORY_CLIENT_MOD \
  -I "$TASK_ROOT/native/client/d3d9/include" -I "$TASK_ROOT/native/client/compat/include" \
  -I "$TASK_ROOT/native/client/d3dx/include" \
  -I "$TASK_ROOT/source/4Story 5.0 Source Files/Lib/Own/Engine Lib/Engine Lib" \
  -I "$TASK_ROOT/source/4Story 5.0 Source Files/Lib/Own/TProtocol/include" \
  -fsyntax-only "$TASK_ROOT/native/client/c3/tests/t3d_include.cpp"
printf '%s\n' 'PASS native original T3D.h loader include and vertex layout assertions'
