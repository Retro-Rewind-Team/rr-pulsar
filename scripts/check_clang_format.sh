#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BASE_DIR="$SCRIPT_DIR/.."

cd "$BASE_DIR"

FMT=""

# Borrowed from https://github.com/jidicula/clang-format-action/blob/main/check.sh
for clangfmt in clang-format{,-{2,1}{9,8,7,6,5,4,3,2,1,0}}; do
    if command -v "$clangfmt" &>/dev/null; then
        FMT="$clangfmt"
        break
    fi
done

# Check if we found a working clang-format
if [ -z "$FMT" ]; then
    echo "failed to find clang-format"
    exit 1
fi

should_error=false

set +e
while IFS= read -r line; do
    "$FMT" --style=file --dry-run -Werror "$(realpath "$line")"
    if ! [ $? -eq 0 ]; then
        "$FMT" --style=file -i -Werror "$(realpath "$line")"
        git diff "$line" | cat
        should_error=true
    fi
done <<<"$(find "$BASE_DIR" -iname "*.cpp" -o -iname "*.hpp" -o -iname "*.c" -o -iname "*.h")"
set -e

if $should_error; then
    exit 1
else
    exit 0
fi
