#!/bin/sh
# Build the original server modules as native macOS executables against the
# Win32/ATL compatibility layer in native/server/win32 (FOURSTORY_NATIVE_MODULE).
# Usage: sh tools/build-modules.sh [TControlSvr|TLoginSvr|TWorldSvr|TMapSvr ...]
# On Linux run it with bash: the job limit uses `jobs -r`, which dash lacks.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
# Oldest macOS the release supports (C8): clang reads it from the environment.
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-14.0}
# unixODBC: Homebrew on macOS, the system prefix on Linux (unixodbc-dev).
if command -v brew >/dev/null 2>&1; then odbc_prefix=$(brew --prefix unixodbc); else odbc_prefix=/usr; fi
odbc_include=$odbc_prefix/include
stdlib=""
if [ "$(uname -s)" = Linux ]; then
  # libc++, the standard library of the macOS build: the same container and
  # algorithm behavior, and no std::random_shuffle in C++17 (libstdc++ keeps it,
  # which makes the compatibility version in win32/windows.hpp ambiguous).
  stdlib="-stdlib=libc++"
  # The original sources use Windows file-name case (SQL.h, SQLEXT.h). On Linux
  # the checkout must be on a case-insensitive (ext4 casefold) directory; the
  # system ODBC headers are linked into it so those names resolve too.
  odbc_include=native/server/build/odbc-include
  mkdir -p "$odbc_include"
  for header in "$odbc_prefix"/include/sql*.h "$odbc_prefix"/include/odbc*.h; do
    [ -f "$header" ] && ln -sf "$header" "$odbc_include/"
  done
fi
source_dir='source/4Story 5.0 Source Files'
out=${FOURSTORY_MODULE_OUT:-native/server/build/module}
# FOURSTORY_MODULE_SANITIZE=1 builds ASan/UBSan executables (use a separate
# FOURSTORY_MODULE_OUT); sanitizer findings abort the process.
# FOURSTORY_MODULE_SANITIZE=report keeps running after undefined-behaviour
# findings so one run lists them all (address errors still abort).
sanitize=""
if [ "${FOURSTORY_MODULE_SANITIZE:-0}" = 1 ]; then
  sanitize="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
elif [ "${FOURSTORY_MODULE_SANITIZE:-0}" = report ]; then
  sanitize="-fsanitize=address,undefined -fno-omit-frame-pointer"
fi
mkdir -p "$out"
# Rebuild everything when the compile flags change (for example the standard
# library); the per-object test below only compares file times.
flags_stamp="$out/.flags"
flags_now="$stdlib|$odbc_include|$sanitize"
if [ -f "$flags_stamp" ] && [ "$(cat "$flags_stamp")" != "$flags_now" ]; then
  echo "  compile flags changed; removing old objects"
  rm -rf "$out/obj" "$out"/*.o
fi
printf '%s\n' "$flags_now" > "$flags_stamp"
jobs=$(sysctl -n hw.ncpu 2>/dev/null || nproc)
common="-std=c++17 $stdlib -g -O1 $sanitize -DFOURSTORY_NATIVE_MODULE -DFOURSTORY_NATIVE_PACKET -DFOURSTORY_NATIVE_ODBC \
 -Inative/server/win32/include -I$odbc_include"
# Original sources: Windows-only diagnostics are disabled; real 64-bit hazards
# (pointer truncation, format mismatches) stay visible as errors or warnings.
legacy="-Wno-everything -Wformat -Wint-to-pointer-cast -Wpointer-to-int-cast -Wno-c++11-narrowing"
compile() { # name source object flags...
  name=$1; src=$2; obj=$3; shift 3
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$(find native/server/win32 "$(dirname "$src")" native/server/*.hpp "source/4Story 5.0 Source Files/Server/TNetLib" \( -name '*.h' -o -name '*.hpp' -o -name '*.inc' \) -newer "$obj" 2>/dev/null | head -1)" ]; then
    echo "  compile $(basename "$src")"; mkdir -p "$(dirname "$obj")"
    clang++ $common "$@" -DFOURSTORY_SERVICE_NAME="\"$name\"" -c "$src" -o "$obj"
  fi
}
# Compatibility layer (strict).
for unit in win32 crypto minidump module_main; do
  compile compat "native/server/win32/$unit.cpp" "$out/$unit.o" -Wall -Wextra -Wpedantic -Werror -Wno-unknown-warning-option -Wno-invalid-utf8 -Wno-nonportable-include-path
done
# Original TNetLib (CryptographyExt.cpp is replaced by win32/crypto.cpp).
mkdir -p "$out/obj/TNetLib"
for unit in Packet Session SqlBase SqlDatabase SqlQuery SqlDirect BindDesc; do
  compile TNetLib "$source_dir/Server/TNetLib/$unit.cpp" "$out/obj/TNetLib/$unit.o" $legacy \
    -I"$source_dir/Server/TNetLib" -I"$source_dir/Lib/Own/TProtocol/include" &
done
wait
roles=${*:-TControlSvr TLoginSvr TWorldSvr TMapSvr}
for role in $roles; do
  echo "$role"
  mkdir -p "$out/obj/$role"
  # Compilation units from the original project, minus Windows-only units.
  units=$(LC_ALL=C grep -a -o '<ClCompile Include="[^"]*\.cpp"' "$source_dir/Server/$role/$role.vcxproj" |
    sed 's/<ClCompile Include="//; s/"$//' | grep -v -E '^(stdafx|TMiniDump)\.cpp$' | tr '\\' '/')
  objects=""
  for unit in $units; do
    obj="$out/obj/$role/${unit%.cpp}.o"
    objects="$objects $obj"
    compile "$role" "$source_dir/Server/$role/$unit" "$obj" $legacy \
      -I"$source_dir/Server/$role" -I"$source_dir/Server/TNetLib" -I"$source_dir/Lib/Own/TProtocol/include" &
    while [ "$(jobs -r | wc -l)" -ge "$jobs" ]; do sleep 0.2; done
  done
  wait
  # Object paths are generated here and contain no whitespace.
  clang++ $stdlib -g -o "$out/$role" $objects "$out"/obj/TNetLib/*.o "$out/win32.o" "$out/crypto.o" "$out/minidump.o" "$out/module_main.o" \
    -L"$odbc_prefix/lib" -lodbc -pthread $sanitize
  echo "  linked $out/$role"
done
