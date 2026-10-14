#!/bin/sh
# linux-run.sh -- start No One Lives Forever (built from source) with Wine.
#
#   ./linux-run.sh [options] [engine args...]
#
# Options:
#   --fullscreen          full screen
#   --windowed            a window
#   --res WxH             the window / screen mode to start in; the menus may
#                         still draw at 640x480 inside it until a resolution
#                         is chosen in Options > Display
#                         Without these the game starts as it was last left
#                         (Options > Display: its window mode and
#                         resolution), and on the first run in a 640x480
#                         window.
#   --world <path>        go straight into a level, e.g. --world 'Worlds\T01S01'
#   --help                this text
# Anything else is handed to the engine unchanged (e.g. +DisableSound 1).
#
# Environment:
#   WINEPREFIX   the Wine prefix to use (default: $HOME/.wine-nolf, created on
#                first run, so your own prefix is left alone)
#   WINE         the wine binary (default: wine)
#   WINEDEBUG    Wine's log channels (default: fixme-all, which silences
#                DirectMusic's chatter)
#
# This does what the retail NOLF.exe launcher does: it starts
# `lithtech.exe -cmdfile <command file>` from the game directory. The command
# file (run-cmds.txt, rewritten every run) is nolfcmds.txt's rez stack plus
# the options above. On the first run autoexec.cfg -- the game's settings,
# rewritten on every exit -- starts as a copy of defaults.cfg.
#
# Sound: the game uses the Miles Sound System (mss32.dll, the one third-party
# DLL here besides Bink's binkw32.dll) through DirectSound, so it needs
# Wine's audio working on this machine (winecfg > Audio); +DisableSound 1
# runs without it. The music is DirectMusic, which Wine only partly
# implements: its own cannot load the music's styles. The modern build has
# Microsoft's DirectMusic in this directory (ms_*.dll and
# lithtech.exe.manifest) and needs nothing more. For the release build there
# is no music until Microsoft's DirectMusic is installed in the prefix (not
# winetricks' `directmusic`, which also replaces Wine's DirectSound, and the
# sound effects repeat):
#   WINEPREFIX=~/.wine-nolf winetricks dmband dmcompos dmime dmloader \
#       dmscript dmstyle dmsynth dmusic dmusic32 dswave gmdls
# The intro movies are off (+DisableMovies 1 in nolfcmds.txt) because they
# stall under some Wine setups.
set -e
cd "$(dirname "$0")"

WINDOWED=
RES=
WORLD=
EXTRA=
while [ $# -gt 0 ]; do
    case "$1" in
        --fullscreen) WINDOWED=0 ;;
        --windowed)   WINDOWED=1 ;;
        --res)        RES="${2:?--res needs WxH}"; shift ;;
        --world)      WORLD="${2:?--world needs a world path}"; shift ;;
        --help|-h)    sed -n '2,/^set -e$/{/^set -e$/d;p}' "./${0##*/}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)            EXTRA="$EXTRA $1" ;;
    esac
    shift
done
# The first run: a 640x480 window, unless asked otherwise.
if [ ! -f autoexec.cfg ]; then
    WINDOWED="${WINDOWED:-1}"
    RES="${RES:-640x480}"
fi
if [ -n "$RES" ]; then
    W="${RES%x*}"; H="${RES#*x}"
    case "$W$H" in *[!0-9]*|"") echo "linux-run.sh: --res wants WxH, e.g. 1024x768" >&2; exit 2 ;; esac
fi

WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf}"
WINEDEBUG="${WINEDEBUG:-fixme-all}"
export WINEPREFIX WINEDEBUG
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || { echo "linux-run.sh: '$WINE' not found -- install Wine (32-bit support needed)" >&2; exit 1; }

[ -f autoexec.cfg ] || cp defaults.cfg autoexec.cfg
mkdir -p Save

# The rez stack from nolfcmds.txt (its -rez arguments; build/all.sh wrote it),
# then our settings: later arguments override earlier ones.
REZ=$(tr -d '\r' < nolfcmds.txt | tr ' ' '\n' | awk '$0=="-rez"{getline n; printf "-rez %s ", n}')
{
    printf '%s+DisableMovies 1' "$REZ"
    [ -n "$RES" ] && printf ' ++screenwidth %s ++screenheight %s ++bitdepth 16' "$W" "$H"
    [ -n "$WINDOWED" ] && printf ' ++windowed %s' "$WINDOWED"
    [ -n "$WORLD" ] && printf ' +runworld %s' "$WORLD"
    printf '%s\r\n' "$EXTRA"
} > run-cmds.txt

exec "$WINE" lithtech.exe -cmdfile run-cmds.txt
