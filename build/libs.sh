#!/usr/bin/env bash
# Build the lithshared static libraries the engine and game DLLs link, from
# src/lithshared/, into src/lithshared/out/: StdLith, CryptMgr, Lith, RezMgr,
# RegMgr (and RegMgr_Debug, the Debug build CShell.dll links), MFCStub,
# ButeMgrNoMFC (the attribute parser plus the zTools containers),
# ltguimgr_LT2, LithFontMgr_LT2, and GameSpyMgr / GameSpyClientMgr (our own
# replacements for the GameSpy middleware). The flags are build/build.mk's
# (section 1): cl 12.00.8447, the revision the original libraries were
# built with, /MT /O2 like the game projects.
#
# Usage: build/libs.sh [stdlith|cryptmgr|lith|rezmgr|regmgr|mfcstub|butemgr|
#                       ltguimgr|lithfontmgr|gamespy|all]   (inside the build container)
set -e
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

L=src/lithshared/out
case "${1:-all}" in
    stdlith)     t=$L/StdLith.lib ;;
    cryptmgr)    t=$L/CryptMgr.lib ;;
    lith)        t=$L/Lith.lib ;;
    rezmgr)      t=$L/RezMgr.lib ;;
    regmgr)      t="$L/RegMgr.lib $L/RegMgr_Debug.lib" ;;
    mfcstub)     t=$L/MFCStub.lib ;;
    ztools|butemgr) t=$L/ButeMgrNoMFC.lib ;;
    ltguimgr)    t=$L/ltguimgr_LT2.lib ;;
    lithfontmgr) t=$L/LithFontMgr_LT2.lib ;;
    gamespy)     t="$L/GameSpyMgr.lib $L/GameSpyClientMgr.lib" ;;
    all)         t=libs ;;
    *) echo "usage: $0 [stdlith|lith|rezmgr|cryptmgr|regmgr|mfcstub|butemgr|ltguimgr|lithfontmgr|gamespy|all]"; exit 1 ;;
esac
nolfsrc_make $t
