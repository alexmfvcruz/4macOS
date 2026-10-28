#!/bin/sh
# Issue folders under implementations/ (see implementations/README.md).
# Usage:
#   sh tools/issue.sh new <slug> "<title>"   create the folder from _template
#   sh tools/issue.sh worktree <slug> [--game-data]  add the worktree and runtime
#   sh tools/issue.sh list                   print each issue and its status
#   sh tools/issue.sh remove-worktree <slug> remove a clean worktree and game clone
#   sh tools/issue.sh close <slug>           archive a MERGED or CLOSED issue
#   sh tools/issue.sh reopen <slug>          restore an archived issue folder
# The worktree uses branch issue/<slug>. A sparse checkout leaves out the
# original archives (backup_source, 1.6 GB), the database backups (67 MB),
# Source Files/TClientCmd.tif (147 MB), research/ and, without --game-data,
# the tracked game data ("game", 2.9 GB). Use --game-data
# when the issue changes game files, so that the branch can commit them. Its .local links
# to the shared dependency builds of the main checkout. The client builds into
# the worktree. Game files, runs and settings live in <slug>/runtime/.
# close packs the issue folder into implementations/archive/<slug>.tar.xz
# (xz, LZMA2; macOS, Linux and Windows 11 open it without extra software),
# checks the archive against the folder, and then deletes the folder.
# ./4story.command issue <command> runs this script.
set -eu
# The main checkout, also when this copy of the script runs inside a worktree.
main=$(dirname "$(git -C "$(dirname -- "$0")" rev-parse --path-format=absolute --git-common-dir)")
base=$main/implementations
archive=$base/archive
usage() { sed -n '4,9p' "$0" | sed 's/^# //' >&2; exit 2; }
check_slug() {
  case "$1" in ''|*[!a-z0-9-]*|-*|archive) echo "slug must use a-z, 0-9 and '-' (not 'archive'): $1" >&2; exit 2 ;; esac
}
cmd=${1:-}
[ -n "$cmd" ] || usage
shift
case "$cmd" in
new)
  [ $# -eq 2 ] || usage
  slug=$1; title=$2; check_slug "$slug"
  dir=$base/$slug
  [ ! -e "$dir" ] || { echo "exists: $dir" >&2; exit 1; }
  case "$title" in *'
'*|'') echo "title must be one non-empty line" >&2; exit 2 ;; esac
  # Escape the sed replacement; <title> goes last so its text is not rewritten.
  esc=$(printf '%s' "$title" | sed -e 's/[\\|&]/\\&/g')
  tmp=$(mktemp)
  sed -e "s|<slug>|$slug|g" -e "s|<date>|$(date +%Y-%m-%d)|g" -e "s|<title>|$esc|g" \
    "$base/_template/ISSUE.md" > "$tmp" || { rm -f "$tmp"; exit 1; }
  mkdir -p "$dir/evidence/screenshots" "$dir/evidence/logs" "$dir/evidence/data"
  mv "$tmp" "$dir/ISSUE.md"
  echo "created $dir/ISSUE.md"
  ;;
worktree)
  [ $# -ge 1 ] && [ $# -le 2 ] || usage
  slug=$1; check_slug "$slug"
  game_data=no
  if [ $# -eq 2 ]; then [ "$2" = --game-data ] || usage; game_data=yes; fi
  dir=$base/$slug; wt=$dir/worktree; rt=$dir/runtime
  [ -f "$dir/ISSUE.md" ] || { echo "no issue folder: run 'new' first" >&2; exit 1; }
  branch=issue/$slug
  if [ $game_data = yes ]; then
    patterns='!/backup_source/'
  else
    patterns='!/game/'
    [ -d "$main/game" ] || { echo "missing game data in the main checkout" >&2; exit 1; }
  fi
  if [ -e "$wt" ]; then
    # Resume an interrupted setup; the steps below skip finished parts.
    git -C "$wt" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "not a worktree: $wt" >&2; exit 1; }
    echo "resuming $wt"
  else
    if git -C "$main" show-ref --verify --quiet "refs/heads/$branch"; then
      git -C "$main" worktree add --no-checkout "$wt" "$branch"
    else
      git -C "$main" worktree add --no-checkout -b "$branch" "$wt" main
    fi
    git -C "$wt" sparse-checkout set --no-cone '/*' "$patterns" '!/backup_source/' \
      '!/source/4Story 5.0 Database/' '!/source/4Story 5.0 Source Files/TClientCmd.tif' '!/research/'
    git -C "$wt" checkout
  fi
  # The .tools link is not a directory, so the "/deps/tools/" rule of .gitignore
  # misses it. info/exclude is shared by all checkouts of this repository.
  exclude=$(git -C "$wt" rev-parse --path-format=absolute --git-path info/exclude)
  mkdir -p "$(dirname "$exclude")"
  grep -qxF '/.tools' "$exclude" 2>/dev/null || echo '/.tools' >> "$exclude"
  # Shared, read-only inputs: dependency builds, tool environments, credentials.
  mkdir -p "$wt/runtime/client"
  for name in deps accounts-build accounts-cargo accounts codex-env sql-server.env equipment; do
    if [ -e "$main/runtime/legacy/$name" ] && [ ! -L "$wt/runtime/legacy/$name" ]; then ln -s "$main/runtime/legacy/$name" "$wt/runtime/legacy/$name"; fi
  done
  if [ -e "$main/.tools" ] && [ ! -L "$wt/.tools" ]; then ln -s "$main/.tools" "$wt/.tools"; fi
  # Per-issue runtime: an APFS clone of the game data (the client writes into
  # its game folder), the run logs and the client settings. Without
  # --game-data the clone comes from the main checkout's working tree.
  if [ $game_data = yes ]; then src=$wt/source/4Story\ 5.0\ Client; else src=$main/source/4Story\ 5.0\ Client; fi
  if [ $game_data = no ] && [ -n "$(git -C "$main" status --porcelain -- 'game')" ]; then
    echo "warning: the main checkout's game data has uncommitted changes; the clone includes them" >&2
  fi
  mkdir -p "$rt/runs"
  if [ ! -d "$rt/game" ]; then
    echo "cloning game data into $rt/game"
    rm -rf "$rt/game.partial"
    cp -c -R "$src" "$rt/game.partial"
    mv "$rt/game.partial" "$rt/game"
  fi
  if [ ! -d "$rt/settings" ]; then
    if [ -d "$main/runtime/client/settings" ]; then cp -R "$main/runtime/client/settings" "$rt/settings"; else mkdir -p "$rt/settings"; fi
  fi
  for name in game runs settings; do
    if [ ! -L "$wt/runtime/client/$name" ]; then ln -s "$rt/$name" "$wt/runtime/client/$name"; fi
  done
  echo "worktree $wt on $branch"
  echo "build: cd \"$wt\" && python3 native/client/app/build.py --channel dev --version <n>"
  echo "run:   cd \"$wt\" && python3 native/client/app/run.py --plain"
  ;;
list)
  for f in "$base"/*/ISSUE.md; do
    [ -e "$f" ] || continue
    d=$(dirname "$f"); s=$(basename "$d")
    [ "$s" = _template ] && continue
    status=$(sed -n 's/^\*\*Status:\*\* *//p' "$f" | head -1)
    wt=-; [ -d "$d/worktree" ] && wt=worktree
    printf '%-45s %-16s %s\n' "$s" "${status:-?}" "$wt"
  done
  for a in "$archive"/*.tar.xz; do
    [ -e "$a" ] || continue
    s=$(basename "$a" .tar.xz)
    status=$(tar -qxOf "$a" "$s/ISSUE.md" 2>/dev/null | sed -n 's/^\*\*Status:\*\* *//p' | head -1)
    printf '%-45s %-16s %s\n' "$s" "${status:-?}" archived
  done
  ;;
remove-worktree)
  [ $# -eq 1 ] || usage
  slug=$1; check_slug "$slug"
  dir=$base/$slug
  # git refuses a worktree with changes; the branch and the issue folder stay.
  git -C "$main" worktree remove "$dir/worktree"
  rm -rf "$dir/runtime/game"
  echo "removed worktree and game clone; runs, settings and branch issue/$slug kept"
  ;;
close)
  [ $# -eq 1 ] || usage
  slug=$1; check_slug "$slug"
  dir=$base/$slug; out=$archive/$slug.tar.xz
  [ -f "$dir/ISSUE.md" ] || { echo "no issue folder: $dir" >&2; exit 1; }
  status=$(sed -n 's/^\*\*Status:\*\* *//p' "$dir/ISSUE.md" | head -1)
  case "$status" in MERGED*|CLOSED*) ;; *) echo "status is '$status'; set MERGED or CLOSED in ISSUE.md first" >&2; exit 1 ;; esac
  if [ -d "$dir/worktree" ] || git -C "$main" worktree list --porcelain | grep -qxF "worktree $dir/worktree"; then
    echo "remove the worktree first: remove-worktree $slug (or git worktree prune)" >&2; exit 1
  fi
  [ ! -e "$out" ] || { echo "exists: $out" >&2; exit 1; }
  # The game clone is not archived (it is a copy of tracked data).
  rm -rf "$dir/runtime/game"
  # diff -r does not compare symbolic links, so the folder may hold only
  # regular files and directories.
  odd=$(find "$dir" ! -type f ! -type d | head -3)
  [ -z "$odd" ] || { echo "folder holds links or special files; remove them first:" >&2; echo "$odd" >&2; exit 1; }
  mkdir -p "$archive"
  partial=$out.partial
  rm -f "$partial"
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp" "$partial"' EXIT
  tar -C "$base" --no-mac-metadata --options xz:compression-level=9 -cJf "$partial" "$slug"
  # Delete the folder only when the unpacked archive equals it.
  tar -C "$tmp" -xJf "$partial"
  report=$(diff -r "$dir" "$tmp/$slug" 2>&1) || true
  [ -z "$report" ] || { echo "archive check failed; folder kept:" >&2; echo "$report" | head -5 >&2; exit 1; }
  mv -n "$partial" "$out"
  [ ! -e "$partial" ] || { echo "exists: $out; folder kept" >&2; exit 1; }
  rm -rf "$dir"
  echo "archived $slug -> $out ($(du -h "$out" | cut -f1)); folder removed"
  ;;
reopen)
  [ $# -eq 1 ] || usage
  slug=$1; check_slug "$slug"
  dir=$base/$slug; in=$archive/$slug.tar.xz
  [ -f "$in" ] || { echo "no archive: $in" >&2; exit 1; }
  [ ! -e "$dir" ] || { echo "exists: $dir" >&2; exit 1; }
  # Unpack beside the issue folders, check, then move into place.
  tmp=$(mktemp -d "$base/.reopen.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  tar -C "$tmp" -xJf "$in"
  [ "$(ls -A "$tmp")" = "$slug" ] && [ -f "$tmp/$slug/ISSUE.md" ] || { echo "archive does not hold only $slug/ with ISSUE.md; archive kept" >&2; exit 1; }
  mv "$tmp/$slug" "$dir"
  rm "$in"
  echo "restored $dir; set its status in ISSUE.md"
  ;;
*) usage ;;
esac
