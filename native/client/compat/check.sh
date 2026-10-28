#!/bin/sh
# Build and run the compatibility layer tests with AddressSanitizer and
# UndefinedBehaviorSanitizer, as C++14 and as C++20.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d)
volume=
cleanup() { [ -z "$volume" ] || hdiutil detach -quiet "$volume" || true; rm -rf "$work"; }
trap cleanup EXIT
# A small case-sensitive volume, for the file name resolution test. The
# default macOS volume ignores case and would not exercise the directory scan.
hdiutil create -quiet -size 8m -fs "Case-sensitive APFS" -volname compatcase "$work/case.dmg"
volume="$work/case"
hdiutil attach -quiet -nobrowse -mountpoint "$volume" "$work/case.dmg"
n=0
for options in "-std=c++14 -fms-extensions" "-std=c++20 -fms-extensions" "-std=c++14"; do
  n=$((n+1)); mkdir -p "$work/$n"
  xcrun clang++ $options -arch arm64 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -DFOURSTORY_CLIENT_NATIVE -DFOURSTORY_CLIENT_64 -DFOURSTORY_CLIENT_MOD \
    -DFOURSTORY_COMPAT_FIXTURES="\"$here/tests/fixtures\"" -I"$here/include" "$here/tests/compat_test.cpp" -o "$work/$n/compat_test"
  printf '%s: ' "$options"
  mkdir "$volume/$n"
  "$work/$n/compat_test" "$work/$n" "$volume/$n"
done
