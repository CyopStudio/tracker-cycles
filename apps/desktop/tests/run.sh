#!/bin/sh
# Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
# GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
# Headless key sessions: each tests/NAME.keys (key tokens, see main.cpp;
# lines starting with # are comments) runs against
# tidal/proto/example.tidal with the print backend; what it prints
# (statements sent, `last` / `msg` / `print` output) must match NAME.expected.
# Usage: tests/run.sh build/tidal-desktop [--update]
bin="$1"
update="$2"
status=0
for keys in tests/*.keys; do
    expected="${keys%.keys}.expected"
    actual=$("$bin" --backend print ../../tidal/proto/example.tidal \
        --keys "$(grep -v '^#' "$keys" | tr '\n' ' ')" 2>/dev/null)
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
