#!/bin/sh
# linux-run-launcher.sh -- start the game through its launcher, NOLF.exe (ours,
# built from src/engine/launcher), the way the retail install does.
#
#   ./linux-run-launcher.sh
#
# Click "Launch" (the first time, pick a performance setting first). The
# launcher writes nolfcmds.txt and starts lithtech.exe itself. ./linux-run.sh skips
# the launcher and has more options; use it if this one gives you trouble.
#
# The launcher expects what the retail installer left behind, and this script
# sets that up inside its Wine prefix (same environment variables as linux-run.sh,
# plus WINESERVER, default wineserver; default prefix $HOME/.wine-nolf):
#   * a CD-ROM drive holding \DATA\NOLF.REZ -- its disc check. Drive N: is
#     mapped to the cd/ directory beside this script, which holds
#     DATA/NOLF.REZ as a link to nolf.rez. Not in the modern build, whose
#     launcher has no disc check (MODERN.md M-P1-15).
#   * the installer's registry values under HKLM\Software\Monolith
#     Productions\No One Lives Forever\1.0: the extra rez archives the
#     launcher adds to the command line (the update, then the GOTY content
#     and -- ours -- the custom/ directory holding the game DLLs built from
#     source), and movies off (on in the modern build, whose own Bink
#     player plays them).
# It also starts the game in a window (++windowed 1: the launcher appends
# its own arguments to the command file, and full screen fails under many
# Wine setups, "Couldn't set D3D Emulation mode"), at 640x480 -- except in
# the modern build, whose launcher offers every resolution the game can run
# at (wide ones too) and starts at the largest 4:3 one that fits the
# desktop, and which is played at the resolution picked there.
#
# Sound needs Wine's audio working. The release build's music also needs
# Microsoft's DirectMusic in the prefix; the modern build carries its own
# (see linux-run.sh).
set -e
cd "$(dirname "$0")"
HERE="$(pwd)"
WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf}"
WINEDEBUG="${WINEDEBUG:-fixme-all}"
export WINEPREFIX WINEDEBUG
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || { echo "linux-run-launcher.sh: '$WINE' not found -- install Wine" >&2; exit 1; }

# The modern build: its own Bink player (MANIFEST.txt lists it as EXT).
MODERN=0
grep -q '^EXT .*	binkw32\.dll$' MANIFEST.txt 2>/dev/null && MODERN=1

[ -f autoexec.cfg ] || cp defaults.cfg autoexec.cfg
mkdir -p Save

# a prefix to set up: make it now if it does not exist yet
[ -d "$WINEPREFIX/dosdevices" ] || "$WINE" wineboot -i >/dev/null 2>&1 || true
if [ $MODERN = 0 ]; then
    mkdir -p cd/DATA
    ln -sf ../../nolf.rez cd/DATA/NOLF.REZ
    ln -sfn "$HERE/cd" "$WINEPREFIX/dosdevices/n:"
fi

KEY='HKLM\Software\Monolith Productions\No One Lives Forever\1.0'
# Both registry views: in a 64-bit prefix `wine reg` writes the 64-bit one,
# while NOLF.exe, a 32-bit program, reads HKLM\Software through the 32-bit
# one (Wow6432Node) -- written only there, it would find no update or GOTY
# archives and leave nolfu003.rez and nolfgoty.rez off the command line.
# (A 32-bit prefix has one view; the /reg:64 write then fails, harmlessly.)
reg() { "$WINE" reg add "$@" /reg:32 /f >/dev/null 2>&1
        "$WINE" reg add "$@" /reg:64 /f >/dev/null 2>&1 || true; }
# The movies: off with RAD's binkw32.dll, which misrenders or stalls under
# many Wine setups; on with the modern build's own player (MANIFEST.txt
# lists it as EXT).
MOVIES_OFF=1
MODE="++screenwidth 640 ++screenheight 480 ++bitdepth 16"
if [ $MODERN = 1 ]; then
    MOVIES_OFF=0
    MODE=""
fi
[ $MODERN = 1 ] || reg 'HKLM\Software\Wine\Drives' /v 'n:' /t REG_SZ /d cdrom
reg "$KEY" /v 'Update Command Line'  /t REG_SZ /d '-rez nolfu003.rez'
reg "$KEY" /v 'Content Command Line' /t REG_SZ /d '-rez nolfgoty.rez -rez custom'
reg "$KEY" /v 'Disable Movies'       /t REG_DWORD /d $MOVIES_OFF
# Wine reads the drive types when its server starts: restart it, or the
# first run after this setup still finds no CD-ROM.
"${WINESERVER:-wineserver}" -k >/dev/null 2>&1 || true
sleep 1

exec "$WINE" NOLF.exe ++windowed 1 $MODE "$@"
