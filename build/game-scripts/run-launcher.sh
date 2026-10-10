#!/bin/sh
# run-launcher.sh -- start the game through its launcher, NOLF.exe (ours,
# built from src/engine/launcher), the way the retail install does.
#
#   ./run-launcher.sh
#
# Click "Launch" (the first time, pick a performance setting first). The
# launcher writes nolfcmds.txt and starts lithtech.exe itself. ./run.sh skips
# the launcher and has more options; use it if this one gives you trouble.
#
# The launcher expects what the retail installer left behind, and this script
# sets that up inside its Wine prefix (same environment variables as run.sh,
# plus WINESERVER, default wineserver; default prefix $HOME/.wine-nolf):
#   * a CD-ROM drive holding \DATA\NOLF.REZ -- its disc check. Drive N: is
#     mapped to the cd/ directory beside this script, which holds
#     DATA/NOLF.REZ as a link to nolf.rez.
#   * the installer's registry values under HKLM\Software\Monolith
#     Productions\No One Lives Forever\1.0: the extra rez archives the
#     launcher adds to the command line (the update, then the GOTY content
#     and -- ours -- the custom/ directory holding the game DLLs built from
#     source), and movies off.
# It also starts the game in a 640x480 window (++windowed 1 ...): the
# launcher appends its own arguments to the command file, and full screen
# fails under many Wine setups ("Couldn't set D3D Emulation mode").
#
# Sound needs Wine's audio working, and music Microsoft's DirectMusic in the
# prefix (see run.sh).
set -e
cd "$(dirname "$0")"
HERE="$(pwd)"
WINEPREFIX="${WINEPREFIX:-$HOME/.wine-nolf}"
WINEDEBUG="${WINEDEBUG:-fixme-all}"
export WINEPREFIX WINEDEBUG
WINE="${WINE:-wine}"
command -v "$WINE" >/dev/null 2>&1 || { echo "run-launcher.sh: '$WINE' not found -- install Wine" >&2; exit 1; }

[ -f autoexec.cfg ] || cp defaults.cfg autoexec.cfg
mkdir -p Save cd/DATA
ln -sf ../../nolf.rez cd/DATA/NOLF.REZ

# a prefix to set up: make it now if it does not exist yet
[ -d "$WINEPREFIX/dosdevices" ] || "$WINE" wineboot -i >/dev/null 2>&1 || true
ln -sfn "$HERE/cd" "$WINEPREFIX/dosdevices/n:"

KEY='HKLM\Software\Monolith Productions\No One Lives Forever\1.0'
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg 'HKLM\Software\Wine\Drives' /v 'n:' /t REG_SZ /d cdrom
reg "$KEY" /v 'Update Command Line'  /t REG_SZ /d '-rez nolfu003.rez'
reg "$KEY" /v 'Content Command Line' /t REG_SZ /d '-rez nolfgoty.rez -rez custom'
reg "$KEY" /v 'Disable Movies'       /t REG_DWORD /d 1
# Wine reads the drive types when its server starts: restart it, or the
# first run after this setup still finds no CD-ROM.
"${WINESERVER:-wineserver}" -k >/dev/null 2>&1 || true
sleep 1

exec "$WINE" NOLF.exe ++windowed 1 ++screenwidth 640 ++screenheight 480 ++bitdepth 16 "$@"
