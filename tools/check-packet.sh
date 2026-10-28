#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build/packet
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  native/packet/tests/packet_test.cpp -o build/packet/packet_test
build/packet/packet_test native/packet/fixtures/scalars.hex
