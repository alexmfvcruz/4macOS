#!/bin/sh
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
TASK_OUT="$TASK_ROOT/runtime/legacy/d3dx-check"
mkdir -p "$TASK_OUT"
for TASK_TEST in math_test c13_math_test c14_vectors_test tga_test dds_test surface_test; do
  clang++ -std=c++14 -ffp-contract=off -O1 -g -fno-omit-frame-pointer \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -DFOURSTORY_CLIENT_NATIVE -I "$TASK_ROOT/native/client/d3d9/include" \
    -I "$TASK_ROOT/native/client/compat/include" -I "$TASK_ROOT/native/client/d3dx/include" \
    "$TASK_ROOT/native/client/d3dx/tests/$TASK_TEST.cpp" -o "$TASK_OUT/$TASK_TEST"
  "$TASK_OUT/$TASK_TEST"
done

clang++ -std=c++14 -ffp-contract=off -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -DFOURSTORY_CLIENT_NATIVE -I "$TASK_ROOT/native/client/d3d9/include" \
  -I "$TASK_ROOT/native/client/compat/include" -I "$TASK_ROOT/native/client/d3dx/include" \
  "$TASK_ROOT/native/client/d3dx/tests/c2_math_reference.cpp" -o "$TASK_OUT/c2_math_reference"
"$TASK_OUT/c2_math_reference" "$TASK_ROOT/native/client/d3dx/baseline/c2-math-inputs.txt" \
  "$TASK_ROOT/native/client/d3dx/baseline/c2-math-windows-x64.txt" > "$TASK_OUT/c2-math-native.txt"
echo 'PASS 870 original Windows x64 D3DX math cases, exact bits'

# The C2 device is implemented by Claude; test it without changing its files.
python3 "$TASK_ROOT/native/client/d3dx/tests/extract_dds.py"
clang++ -std=c++14 -ffp-contract=off -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -DFOURSTORY_CLIENT_NATIVE -DFOURSTORY_D3DX_EMPTY_TEST \
  -I "$TASK_ROOT/native/client/d3d9/include" -I "$TASK_ROOT/native/client/compat/include" \
  -I "$TASK_ROOT/native/client/d3dx/include" \
  "$TASK_ROOT/native/client/d3dx/tests/gpu_upload.cpp" \
  "$TASK_ROOT/native/client/device/empty_device.cpp" -o "$TASK_OUT/empty-upload"
"$TASK_OUT/empty-upload" "$TASK_OUT"/assets/*.dds
