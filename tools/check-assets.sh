#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Reuse the wrapper's isolated dependency environment.
sh "$project_dir/tools/render-asset.sh" --help >/dev/null
exec "$project_dir/deps/python/assets/bin/python" "$project_dir/tools/assets/test_assets.py"
