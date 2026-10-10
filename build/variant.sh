#!/usr/bin/env bash
# The game built from the same source with project defines that the release
# build never has -- never for measuring: progress is only ever scored on the
# release build (`make`, `make progress`).
#
#   build/variant.sh debug         make debug          -> out/game-debug/
#   build/variant.sh modern        make modern         -> out/game-modern/
#   build/variant.sh modern-debug  make modern-debug   -> out/game-modern-debug/
#
# (on the host the make targets run this in the build container; inside
# `make shell` run it directly). Play the result exactly like out/game/:
# out/game-<variant>/run.sh, or tools/playtest.sh out/game-<variant> ...
#
#   debug    NOLFSRC_DEBUG and symbols, for finding out why something goes
#            wrong. Compile + /Z7 /Oy- /D NOLFSRC_DEBUG (debug information,
#            frame pointers kept so a backtrace can walk the stack; every
#            other flag, the optimisation included, is the release one);
#            link + /DEBUG /OPT:REF /INCREMENTAL:NO (a .pdb per binary, the
#            same unreferenced-code removal as release). The .pdb and .map of
#            every binary go beside it. src/nolfsrc/nolfsrc_debug.h's
#            NOLFSRC_LOG lines and lithtech.exe's crash backtraces
#            (nolfsrc_crash.h) go to nolfsrc.log in the game's directory.
#   modern   NOLFSRC_MODERN: the retail bugs fixed (RETAIL_BUGS.md) and,
#            later, new features. Compile + /D NOLFSRC_MODERN, nothing else.
#   modern-debug   both.
#
# The flags reach every compile and link through build/env.sh's
# NOLFSRC_CFLAGS / NOLFSRC_LDFLAGS, which are empty for every release
# command -- and which build/env.sh refuses outside the trees this script
# makes, so they cannot reach the release build by accident.
#
# One gap in the debug information: Intel C/C++ 4.0 emits none for a file
# compiled with /Qip (the d3d.ren files whose names end in qip/qipni/...),
# so frames in those functions have no name; the crash log gives their
# address as the .map file beside d3d.ren lists it (tools/mapaddr.py).
#
# It never touches the release build. The source is copied (without any
# build output) into out/build-<variant>/ and built there by the same
# build/all.sh, so src/*/Release -- what `make progress` scores -- and
# out/game/ are left exactly as they were. The game data are extracted into
# out/game-<variant>/ on its first run (about 1.1 GB, a few minutes) and kept
# after that; no tar is made.
set -e
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

VARIANT="${1:-}"
case "$VARIANT" in
    debug)        CFLAGS="/Z7 /Oy- /D NOLFSRC_DEBUG"
                  LDFLAGS="/DEBUG /OPT:REF /INCREMENTAL:NO" ;;
    modern)       CFLAGS="/D NOLFSRC_MODERN"
                  LDFLAGS="" ;;
    modern-debug) CFLAGS="/Z7 /Oy- /D NOLFSRC_DEBUG /D NOLFSRC_MODERN"
                  LDFLAGS="/DEBUG /OPT:REF /INCREMENTAL:NO" ;;
    *) echo "usage: build/variant.sh debug|modern|modern-debug" >&2; exit 2 ;;
esac

if ! command -v wine >/dev/null 2>&1; then
    echo "error: no wine here -- run \`make $VARIANT\` from the repository root, or" >&2
    echo "       run this inside \`make shell\`." >&2
    exit 1
fi

TREE="$REPO/out/build-$VARIANT"
echo "== $VARIANT tree: src/ build/ tools/ -> out/build-$VARIANT/ (no build outputs)"
rm -rf "$TREE/src" "$TREE/build" "$TREE/tools" "$TREE/ISOS"
mkdir -p "$TREE"
tar -C "$REPO" -cf - \
    --exclude='src/engine/*/Release' --exclude='src/lithshared/*/Release' \
    --exclude='src/lithshared/out' --exclude='src/game/*/Release' \
    --exclude='src/game/*/Final_Release' --exclude='src/game/*/Debug' \
    --exclude='__pycache__' \
    src build tools ISOS/SHA256SUMS ISOS/README.md | tar -C "$TREE" -xf -
# the compilers, and the disc images where build/isos.sh looks for them
[ -L "$TREE/toolchain" ] || ln -s ../../toolchain "$TREE/toolchain"
export NOLF_ISOS="${NOLF_ISOS:-$REPO/ISOS}"

export NOLFSRC_CFLAGS="$CFLAGS"
export NOLFSRC_LDFLAGS="$LDFLAGS"
export NOLF_GAME_OUT="$REPO/out/game-$VARIANT"
export NOLF_PACKAGE=0
bash "$TREE/build/all.sh"

case "$VARIANT" in *debug)
    echo
    echo "== symbols: each built binary's .pdb (and .map) beside it in out/game-$VARIANT/"
    # MANIFEST.txt has `built <source>\t<installed path>` for every binary we
    # built; its .pdb is the link output's name with .pdb, and the debugger
    # looks for it by that name in the binary's own directory.
    grep '^built ' "$NOLF_GAME_OUT/MANIFEST.txt" | sort -u |
    while IFS=$'\t' read -r src dest; do
        src="${src#built }"
        pdb="$TREE/${src%.*}.pdb"
        if [ -f "$pdb" ]; then
            cp "$pdb" "$NOLF_GAME_OUT/$(dirname "$dest")/"
            echo "   $(dirname "$dest")/$(basename "$pdb")"
        else
            echo "   (no .pdb for $dest)"
        fi
        # the linker's map, for addresses no .pdb names (see nolfsrc_crash.h)
        map="$TREE/${src%.*}.map"
        [ -f "$map" ] && cp "$map" "$NOLF_GAME_OUT/$(dirname "$dest")/"
    done ;;
esac
echo "== done: out/game-$VARIANT/ -- play it with out/game-$VARIANT/run.sh"
