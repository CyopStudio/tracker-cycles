#!/bin/sh
# Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
# GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
# Build (if needed) and open the desktop app with real Tidal (GHCi).
#   apps/desktop/run.sh                 open apps/desktop/work.tidal (made from
#                                       tidal/proto/example.tidal the first time)
#   apps/desktop/run.sh my.tidal [opts] another file; options go to tidal-desktop
#                                       (--superdirt HOST:PORT, --no-panel, ...)
set -e
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"
make -s
# GHCi: ghcup's environment, also when started without the user's shell setup
[ -f "$HOME/.ghcup/env" ] && . "$HOME/.ghcup/env"
file=work.tidal
case "$1" in
    ""|-*) [ -f "$file" ] || cp ../../tidal/proto/example.tidal "$file" ;;
    *) file=$1; shift ;;
esac
exec ./build/tidal-desktop "$file" "$@"
