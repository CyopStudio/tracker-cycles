#!/bin/sh
# Run each tests/proto/*.keys session against proto/example.tidal and compare
# what it prints (statements sent + final file) with NAME.expected.
# NAME.args (optional) adds options, e.g. --backend engine (highlighting),
# --mode cursor (sessions run in tree mode unless they say so).
# Usage: tests/proto/run_smoke.sh build/tidal_proto [--update]
bin="$1"
update="$2"
status=0
for keys in tests/proto/*.keys; do
    expected="${keys%.keys}.expected"
    args=""
    [ -f "${keys%.keys}.args" ] && args=$(cat "${keys%.keys}.args")
    # shellcheck disable=SC2086
    actual=$("$bin" proto/example.tidal --mode tree $args --keys "$(tr '\n' ' ' < "$keys")" 2>/dev/null)
    if [ "$update" = "--update" ]; then
        printf '%s\n' "$actual" > "$expected"
        echo "updated $expected"
    elif printf '%s\n' "$actual" | diff -u "$expected" - ; then
        echo "ok   $keys"
    else
        echo "FAIL $keys"
        status=1
    fi
done
exit $status
