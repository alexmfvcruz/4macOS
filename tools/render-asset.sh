#!/bin/sh
# Terminal-only asset previews. Everything renders in memory.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python="$project_dir/deps/python/assets/bin/python"
requirements="$project_dir/tools/assets/requirements.txt"
marker="$project_dir/deps/python/assets/.dependencies-installed"
if [ ! -x "$python" ]; then
  python3 -m venv "$project_dir/deps/python/assets"
fi
if ! cmp -s "$requirements" "$marker"; then
  "$python" -m pip install -r "$requirements" >&2
  cp "$requirements" "$marker"
fi
exec "$python" "$project_dir/tools/assets/render_asset.py" "$@"
