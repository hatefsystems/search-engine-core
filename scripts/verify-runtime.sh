#!/usr/bin/env bash
set -euo pipefail

test -x /app/server
test -x /app/start.sh
bash -n /app/start.sh
for directory in public locales templates; do
    test -d "/app/$directory"
done
for binary in /app/server /usr/local/lib/*.so*; do
    # ldd resolves transitive dependencies, but returns zero for missing libs.
    result=$(ldd "$binary")
    if [[ "$result" == *"not found"* ]]; then
        printf 'Unresolved runtime dependency in %s:\n%s\n' "$binary" "$result" >&2
        exit 1
    fi
done
for tool in gcc g++ cmake make; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf 'Unexpected build tool in runtime: %s\n' "$tool" >&2
        exit 1
    fi
done
if find /usr/local/lib -name '*.a' -print -quit | grep -q .; then
    echo 'Unexpected static library in runtime' >&2
    exit 1
fi
