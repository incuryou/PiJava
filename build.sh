#!/bin/sh
# Desktop is the default. Request Switch explicitly with ./build.sh --switch.
set -eu
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
exec python3 "$script_dir/tools/build.py" "$@"
