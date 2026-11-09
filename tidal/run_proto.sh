#!/bin/sh
# Launch the editor prototype window with the mock Tracker panel.
# Rebuilds it first if any source changed (needs libsdl2-dev).
#
#   tidal/run_proto.sh                    open tidal/work.tidal (made from
#                                         proto/example.tidal the first time)
#   tidal/run_proto.sh my.tidal [opts]    open another file; extra options go
#                                         to tidal_proto (e.g. --backend ghci)
#   tidal/run_proto.sh --install-shortcut add "Tracker Cycles" to the Linux
#                                         app menu and, on WSL2, as Windows
#                                         Desktop + Start menu shortcuts
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
    # WSL2: WSLg doesn't always list .desktop apps in the Start menu, so
    # also make Windows shortcuts that start this script via wslg.exe
    # (no console window).
    if command -v powershell.exe >/dev/null 2>&1 && [ -n "$WSL_DISTRO_NAME" ]; then
        powershell.exe -NoProfile -Command "
            \$wslg = Join-Path \$env:ProgramFiles 'WSL\\wslg.exe'
            if (-not (Test-Path \$wslg)) { \$wslg = 'C:\\Windows\\System32\\wsl.exe' }
            \$sh = New-Object -ComObject WScript.Shell
            foreach (\$d in @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('Programs'))) {
                \$l = \$sh.CreateShortcut((Join-Path \$d 'Tracker Cycles.lnk'))
                \$l.TargetPath = \$wslg
                \$l.Arguments = '-d $WSL_DISTRO_NAME --cd $here -- ./run_proto.sh'
                \$l.Description = 'Tracker Cycles editor prototype'
                \$l.IconLocation = \$wslg + ',0'
                \$l.Save()
                Write-Output ('installed ' + (Join-Path \$d 'Tracker Cycles.lnk'))
            }" | tr -d '\r'
    fi
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
