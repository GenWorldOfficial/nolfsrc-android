#!/usr/bin/env bash
# THE build: a runnable No One Lives Forever (GOTY) in out/nolf-legacy-release/, with every
# binary compiled from this repository's source and only DATA taken from the
# retail game.
#
#   make                    (on the host: builds the docker image, installs
#                            and checks the retail game, runs this in a
#                            container)
#   build/all.sh            (inside the build container, e.g. `make shell`)
#   make test               (then: play it -- tools/playtest.sh on out/nolf-legacy-release/)
#
# Needs: the toolchain (toolchains/, see toolchains/README.md), Wine (the build
# container has it), and the retail Game of the Year Edition (Europe) as the
# rez/ submodule installs it from its two disc images (rez/games/nolf-eu-goty/,
# build/retail.sh). It only reads that directory: no retail game binary is
# run, linked or copied into the game, apart from the middleware of step 2.
#
# The steps, in order:
#   1. resources          game data from the retail game: the four .rez archives
#                         (with the retail game DLLs inside them zeroed),
#                         configs, text, movies. No binary.
#   2. third-party runtime  Miles (mss32.dll + its .flt/.m3d/.asi/.as6
#                         providers) and Bink (binkw32.dll), from the retail
#                         game.
#                         The ONLY binaries not built here: licensed
#                         middleware with no source, which lithtech.exe and
#                         NOLF.exe import -- the game cannot load without
#                         Miles. Isolated in this one step so they are easy
#                         to find and to replace -- which the modern build
#                         does (NOLF_MIDDLEWARE=open, set by build/variant.sh):
#                         OpenMiles' mss32.dll and our FFmpeg-based
#                         binkw32.dll, built from the ext/ submodules by
#                         build/ext.sh, and no Miles provider files.
#   2b. launcher art      the splash bitmap, palette, music, icon and cursors
#                         that launcher.rc compiles into NOLF.exe, read out of
#                         the retail NOLF.exe's resource section
#                         into src/engine/launcher/res/ (data, not code; the
#                         retail image is parsed, never copied or run).
#   3. clean              every build output from a previous run is removed,
#                         so nothing stale can be linked or installed.
#   4. libraries          the lithshared static libraries (build/libs.sh all),
#                         our GameSpy replacements included.
#   5. game DLLs          CRes.dll, CShell.dll, Object.lto from src/game/ (the
#                         official source release), Final Release
#                         (build/build.sh all final).
#   6. engine             ltmsg.dll, sres.dll, NOLF.exe, server.dll,
#                         lithtech.exe, d3d.ren from src/engine/
#                         (build/engine.sh: each target's objects, then its
#                         whole link).
#   7. assemble           the built binaries into out/nolf-legacy-release/, laid out like a
#                         retail install: the engine beside the data, the game
#                         DLLs in the custom/ rez directory the engine
#                         searches last-first (nolfcmds.txt: `-rez custom`).
#   8. audit              every PE file in out/nolf-legacy-release/ listed with its origin;
#                         the build fails if any is unaccounted for or if a
#                         .rez archive still holds one.
#   9. package            out/nolf-legacy-release.tar: the game directory as one file,
#                         to copy to the machine that plays it (unpack, then
#                         ./run.sh -- see its header; Wine needed there).
#
# Every other input is a committed source file (including the committed link
# inputs src/engine/*/link/*, src/engine/d3d/d3ddata.cpp and
# src/engine/launcher/libs/mss32.def). No step uses retail code: step 2b reads
# only the launcher's art out of the retail NOLF.exe's resources.
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Wine and the toolchain live in the build container: on the host, use make.
if ! command -v wine >/dev/null 2>&1; then
    echo "error: no wine here -- run \`make\` from the repository root (it runs this" >&2
    echo "       script in the build container), or run it inside \`make shell\`." >&2
    exit 1
fi
export WINEDEBUG=-all

cd "$REPO"
source build/retail.sh
OUT="${NOLF_GAME_OUT:-out/nolf-legacy-release}"
RETAIL="$(retail_dir)"
retail_verify
T0=$(date +%s)
step() { echo; echo "######## $* ($(( $(date +%s) - T0 ))s)"; }
need() {  # need <file> <what> [log]: a build output must exist
    [ -f "$1" ] && return 0
    echo "error: $2 was not built ($1 missing)" >&2
    [ -n "${3:-}" ] && [ -f "$3" ] && grep -E "error|unresolved" "$3" | head -20 >&2
    exit 1
}

step "1. resources: game data from $RETAIL -> $OUT"
python3 tools/retailextract.py resources "$RETAIL" "$OUT"
# autoexec.cfg starts as the shipped defaults (the engine rewrites it on
# exit); a rebuild keeps the player's.
[ -f "$OUT/autoexec.cfg" ] || cp "$OUT/defaults.cfg" "$OUT/autoexec.cfg"
mkdir -p "$OUT/Save" "$OUT/custom"
# The retail NOLF.exe launcher writes nolfcmds.txt and runs
# "lithtech.exe -cmdfile nolfcmds.txt"; run.sh does the same with this one.
# Rez order is the GOTY install's (later entries override earlier ones), and
# custom/ -- our game DLLs -- goes last. +DisableMovies skips the Bink intro
# logos, which retail binkw32.dll misrenders or stalls on under some Wine
# setups; the modern build's own Bink player (NOLF_MIDDLEWARE=open) plays them.
MOVIES=" +DisableMovies 1"
[ "${NOLF_MIDDLEWARE:-retail}" = open ] && MOVIES=""
printf -- '-rez nolf.rez -rez nolf2.rez -rez nolfu003.rez -rez nolfgoty.rez -rez custom ++screenwidth 640 ++screenheight 480 ++bitdepth 16 ++windowed 1%s\r\n' \
    "$MOVIES" > "$OUT/nolfcmds.txt"
# run.sh / run-launcher.sh: start the game with the host's Wine, directly or
# through our NOLF.exe (build/game-scripts/; usage in their headers).
cp build/game-scripts/run.sh build/game-scripts/run-launcher.sh "$OUT/"
chmod +x "$OUT/run.sh" "$OUT/run-launcher.sh"

if [ "${NOLF_MIDDLEWARE:-retail}" = open ]; then
    step "2. open runtime: OpenMiles + our Bink player on FFmpeg, built from ext/ (build/ext.sh)"
    bash build/ext.sh
    python3 tools/retailextract.py openruntime "${NOLF_EXT_OUT:-out/ext}" "$OUT"
else
    step "2. THIRD-PARTY RUNTIME: Miles + Bink from $RETAIL (the only binaries not built here)"
    python3 tools/retailextract.py thirdparty "$RETAIL" "$OUT"
fi

step "2b. launcher art: resources of the retail NOLF.exe in $RETAIL -> src/engine/launcher/res/"
python3 tools/retailextract.py launcherres "$RETAIL" src/engine/launcher/res

step "2c. Game of the Year text: the retail CRES.dll in $RETAIL -> src/game/ClientRes/goty/"
python3 tools/retailextract.py goty "$RETAIL" src/game/ClientRes/goty

step "3. clean: previous build outputs"
rm -rf src/engine/*/Release src/lithshared/*/Release src/lithshared/*/Debug \
       src/lithshared/out \
       src/game/*/Release src/game/*/Final_Release

step "4. libraries: build/libs.sh all"
bash build/libs.sh all
for l in StdLith CryptMgr Lith RezMgr RegMgr RegMgr_Debug MFCStub ButeMgrNoMFC ltguimgr_LT2 \
         LithFontMgr_LT2 GameSpyMgr GameSpyClientMgr; do
    need "src/lithshared/out/$l.lib" "$l.lib"
done

step "5. game DLLs: build/build.sh all final"
bash build/build.sh all final
need src/game/ClientRes/Release/CRes.dll CRes.dll
need src/game/ClientShellDLL/Final_Release/CShell.dll CShell.dll
need src/game/ObjectDLL/Final_Release/Object.lto Object.lto

step "6. engine: build/engine.sh"
bash build/engine.sh ltmsg
need src/engine/ltmsg/Release/ltmsg.dll ltmsg.dll
bash build/engine.sh sres
need src/engine/sres/Release/sres.dll sres.dll
bash build/engine.sh launcher link
need src/engine/launcher/Release/NOLF.exe NOLF.exe
bash build/engine.sh server
bash build/engine.sh server-link
need src/engine/server/Release/server.dll server.dll src/engine/server/Release/server-link.txt
bash build/engine.sh lithtech
bash build/engine.sh lithtech-link
need src/engine/lithtech/Release/lithtech.exe lithtech.exe src/engine/lithtech/Release/lithtech-link.txt
bash build/engine.sh d3d
bash build/engine.sh d3d-link
need src/engine/d3d/Release/d3d.ren d3d.ren src/engine/d3d/Release/d3d-link.txt

step "7. assemble: the built binaries -> $OUT"
install_built() {  # install_built <built file> <dest relative to $OUT>
    cp "$1" "$OUT/$2"
    printf 'built %s\t%s\n' "$1" "$2" >> "$OUT/MANIFEST.txt"
    echo "   $2  <-  $1"
}
install_built src/engine/lithtech/Release/lithtech.exe   lithtech.exe
install_built src/engine/server/Release/server.dll       server.dll
install_built src/engine/d3d/Release/d3d.ren             d3d.ren
install_built src/engine/launcher/Release/NOLF.exe       NOLF.exe
install_built src/engine/ltmsg/Release/ltmsg.dll         ltmsg.dll
# the game DLLs and sres.dll go in the custom/ rez directory, which overrides
# the (zeroed) copies inside the archives
install_built src/game/ClientRes/Release/CRes.dll            custom/cres.dll
install_built src/game/ClientShellDLL/Final_Release/CShell.dll custom/cshell.dll
install_built src/game/ObjectDLL/Final_Release/Object.lto    custom/object.lto
install_built src/engine/sres/Release/sres.dll           custom/sres.dll

step "8. audit: every PE file in $OUT"
python3 tools/retailextract.py audit "$OUT"

# (NOLF_PACKAGE=0 skips it: build/variant.sh makes no tar)
if [ "${NOLF_PACKAGE:-1}" = 1 ]; then
    step "9. package: $OUT -> ${OUT%/*}/nolf-legacy-release.tar"
    # One file to copy to the machine that plays it: unpack anywhere, ./run.sh.
    # Links are stored as the files they point to (-h); the per-machine state
    # run.sh / run-launcher.sh create is left out.
    PKG="${OUT%/*}/nolf-legacy-release.tar"
    tar -C "$(dirname "$OUT")" -h -cf "$PKG.part" \
        --exclude=run-cmds.txt --exclude=cd --exclude='*.part' "$(basename "$OUT")"
    mv "$PKG.part" "$PKG"
    echo "   $PKG: $(du -h "$PKG" | cut -f1) (unpacks to $(basename "$OUT")/)"
fi

step "done: $OUT ($(( $(date +%s) - T0 ))s) -- play it with $OUT/run.sh, or test it with make test"
