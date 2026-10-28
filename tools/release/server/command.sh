#!/bin/sh
# 4Story server: @TITLE@. Double-click to run (see README.md).
cd "$(dirname "$0")" || exit 1
/usr/bin/python3 lib/server.py @COMMAND@ "$@"
status=$?
echo
printf 'Press Return to close this window. '
read -r _
exit $status
