#!/bin/sh
# Launch the editor prototype window with the mock Tracker panel.
# Rebuilds it first if any source changed (needs libsdl2-dev).
#
#   tidal/run_proto.sh                    open tidal/work.tidal (made from
#                                         proto/example.tidal the first time)
#   tidal/run_proto.sh my.tidal [opts]    open another file; extra options go
#                                         to tidal_proto (e.g. --backend ghci)
#   tidal/run_proto.sh --install-shortcut add "Tracker Cycles" to the app menu
#                                         (on WSL2 it shows up in the Windows
#                                         Start menu via WSLg)
#
# Started without a terminal (from the shortcut), output goes to
# tidal/build-sdl/launch.log.
set -e
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"

if [ "$1" = "--install-shortcut" ]; then
    dir="$HOME/.local/share/applications"
    mkdir -p "$dir"
    cat > "$dir/tracker-cycles.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Tracker Cycles
Comment=Tracker Cycles editor prototype (mock Polyend Tracker)
Exec=$here/run_proto.sh
Path=$here
Terminal=false
Categories=AudioVideo;Audio;
EOF
    echo "installed $dir/tracker-cycles.desktop"
    echo "On WSL2 it appears in the Windows Start menu as \"Tracker Cycles (Ubuntu)\" (may take a minute)."
    exit 0
fi

if [ ! -t 1 ]; then
    mkdir -p build-sdl
    exec >build-sdl/launch.log 2>&1
fi

make -s proto SDL=1 BUILD=build-sdl

file=work.tidal
case "$1" in
    ""|-*) [ -f "$file" ] || cp proto/example.tidal "$file" ;;
    *) file=$1; shift ;;
esac
exec ./build-sdl/tidal_proto --window "$file" "$@"
