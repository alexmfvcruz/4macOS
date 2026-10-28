#!/bin/sh
# Native C1 libraries and portable dump, with sanitizers enabled by default.
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec python3 "$TASK_ROOT/native/client/build/build.py" "$@"
