#!/usr/bin/env bash
# The debug build: the same game as `make`, from the same source, with
# symbols, and with the diagnostics of src/nolfsrc/nolfsrc_debug.h
# (NOLFSRC_LOG, NOLFSRC_ASSERT, #ifdef NOLFSRC_DEBUG blocks) compiled in.
# For finding out why something crashes -- never for measuring: progress is
# only ever scored on the release build.
#
#   make debug              (on the host; runs this in the build container)
#   build/debug.sh          (inside `make shell`)
#
# Then play out/game-debug/ exactly like out/game/ (out/game-debug/run.sh, or
# tools/playtest.sh out/game-debug ...). A crash under Wine prints a
# backtrace with function names, arguments and source lines, read from the
# .pdb files beside each binary, and NOLFSRC_LOG lines go to
# out/game-debug/nolfsrc.log.
#
# What differs from the release build, and nothing else:
#   compile   + /Z7 /Oy- /D NOLFSRC_DEBUG   (debug information in every
#                                      object, and frame pointers kept, so a
#                                      backtrace can walk the stack; every
#                                      other flag, the optimisation included,
#                                      is the release one)
#   link      + /DEBUG /OPT:REF /INCREMENTAL:NO   (a .pdb per binary; the same
#                                      unreferenced-code removal as release)
# applied through build/env.sh's NOLFSRC_CFLAGS / NOLFSRC_LDFLAGS, which are
# empty for every release command.
#
# One gap: Intel C/C++ 4.0 emits no debug information for a file compiled
# with /Qip (the d3d.ren files whose names end in qip/qipni/...), so frames
# in those functions have no name; the crash log gives their address as the
# .map file beside d3d.ren lists it.
#
# It never touches the release build. The source is copied (without any
# build output) into out/debug/ and built there by the same build/all.sh, so
# src/*/Release -- what `make progress` scores -- and out/game/ are left
# exactly as they were. The game data are extracted into out/game-debug/ on
# the first run (about 1.1 GB, a few minutes) and kept after that; no tar is
# made.
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

if ! command -v wine >/dev/null 2>&1; then
    echo "error: no wine here -- run \`make debug\` from the repository root, or" >&2
    echo "       run this inside \`make shell\`." >&2
    exit 1
fi

DBG="$REPO/out/debug"
echo "== debug tree: src/ build/ tools/ -> out/debug/ (no build outputs)"
rm -rf "$DBG/src" "$DBG/build" "$DBG/tools" "$DBG/ISOS"
mkdir -p "$DBG"
tar -C "$REPO" -cf - \
    --exclude='src/engine/*/Release' --exclude='src/lithshared/*/Release' \
    --exclude='src/lithshared/out' --exclude='src/game/*/Release' \
    --exclude='src/game/*/Final_Release' --exclude='src/game/*/Debug' \
    --exclude='__pycache__' \
    src build tools ISOS/SHA256SUMS ISOS/README.md | tar -C "$DBG" -xf -
# the compilers, and the disc images where build/isos.sh looks for them
[ -L "$DBG/toolchain" ] || ln -s ../../toolchain "$DBG/toolchain"
export NOLF_ISOS="${NOLF_ISOS:-$REPO/ISOS}"

export NOLFSRC_CFLAGS="/Z7 /Oy- /D NOLFSRC_DEBUG"
export NOLFSRC_LDFLAGS="/DEBUG /OPT:REF /INCREMENTAL:NO"
export NOLF_GAME_OUT="$REPO/out/game-debug"
export NOLF_PACKAGE=0
bash "$DBG/build/all.sh"

echo
echo "== symbols: each built binary's .pdb (and .map) beside it in out/game-debug/"
# MANIFEST.txt has `built <source>\t<installed path>` for every binary we
# built; its .pdb is the link output's name with .pdb, and the debugger
# looks for it by that name in the binary's own directory.
grep '^built ' "$NOLF_GAME_OUT/MANIFEST.txt" | sort -u |
while IFS=$'\t' read -r src dest; do
    src="${src#built }"
    pdb="$DBG/${src%.*}.pdb"
    if [ -f "$pdb" ]; then
        cp "$pdb" "$NOLF_GAME_OUT/$(dirname "$dest")/"
        echo "   $(dirname "$dest")/$(basename "$pdb")"
    else
        echo "   (no .pdb for $dest)"
    fi
    # the linker's map, for addresses no .pdb names (see nolfsrc_crash.h)
    map="$DBG/${src%.*}.map"
    [ -f "$map" ] && cp "$map" "$NOLF_GAME_OUT/$(dirname "$dest")/"
done
echo "== done: out/game-debug/ -- play it with out/game-debug/run.sh"
