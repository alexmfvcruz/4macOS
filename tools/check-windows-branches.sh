#!/bin/sh
# Verify that native-only preprocessor branches leave the Windows build of every
# changed original source byte-identical to a base revision (default HEAD).
# The Windows view of a file is produced by unifdef with all native macros undefined.
# FOURSTORY_CLIENT_MOD marks our own additions to the Windows client (see
# research/client-windows-build.md); the original view excludes them too.
# FOURSTORY_CLIENT_64 marks corrections for 64-bit pointers (x64 and native builds).
# FOURSTORY_CLIENT_NATIVE marks branches for the native macOS client only.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
base=${1:-HEAD}
macros="-UFOURSTORY_NATIVE_MODULE -UFOURSTORY_NATIVE_PACKET -UFOURSTORY_NATIVE_ODBC -UFOURSTORY_NATIVE_SERVER -UFOURSTORY_CLIENT_MOD -UFOURSTORY_CLIENT_64 -UFOURSTORY_CLIENT_NATIVE"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
status=0
count=0
intended=0
allow="$project_dir/tools/windows-build-changes.txt"
# Remove C and C++ comments, keeping string and character literals.
strip_comments() {
  # A comment counts as one space; a // comment continues over a trailing backslash.
  perl -0777 -pe 's{//(?:\\\r?\n|[^\r\n])*|/\*.*?\*/|("(?:\\\r?\n|\\.|[^"\\\n])*"|\x27(?:\\\r?\n|\\.|[^\x27\\\n])*\x27)}{defined $1 ? $1 : " "}gse' "$1"
}
git diff --name-only -z "$base" -- 'source/4Story 5.0 Source Files' > "$tmp/files"
while IFS= read -r -d '' file; do
  [ -f "$file" ] || continue
  count=$((count+1))
  git show "$base:$file" > "$tmp/base"
  # unifdef exits 1 when it changed the input; only 2 is an error.
  unifdef $macros "$tmp/base" > "$tmp/base.win" || [ $? -eq 1 ]
  unifdef $macros "$file" > "$tmp/work.win" || [ $? -eq 1 ]
  if ! cmp -s "$tmp/base.win" "$tmp/work.win"; then
    sum=$(shasum -a 256 "$file" | cut -d' ' -f1)
    listed=0
    [ -f "$allow" ] && grep -v '^#' "$allow" | sed 's/ # .*//' | grep -Fqx -- "$sum  $file" && listed=1
    if [ $listed -eq 1 ] && case "$file" in *.vcxproj) true ;; *) false ;; esac; then
      echo "INTENDED WINDOWS BUILD CHANGE: $file"; intended=$((intended+1))
    elif [ $listed -eq 1 ] &&
       strip_comments "$tmp/base.win" > "$tmp/base.code" &&
       strip_comments "$tmp/work.win" > "$tmp/work.code" &&
       cmp -s "$tmp/base.code" "$tmp/work.code"; then
      # A listed source file may differ only inside comments (no code change).
      echo "INTENDED COMMENT CHANGE: $file"; intended=$((intended+1))
    else
      echo "WINDOWS VIEW CHANGED: $file"; status=1
    fi
  fi
done < "$tmp/files"
[ $status -eq 0 ] && echo "PASS Windows view unchanged for $((count-intended)) changed original files, $intended intended build changes listed in tools/windows-build-changes.txt (base $base)"
exit $status
