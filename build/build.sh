#!/usr/bin/env bash
# Build the three game DLLs from Monolith's NOLF v1.003 source release
# (src/game/): CRes.dll, CShell.dll, Object.lto, with VC6 under Wine
# (build/build.mk, section 2: source lists from the .dsp project files).
# The `final` configuration adds _REZFILE/_FINAL, which running from .rez
# archives needs, and builds into Final_Release/; `release` into Release/.
#
# Usage: build/build.sh [cres|cshell|object|all] [release|final]
set -e
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

case "${2:-release}" in
    release|final) export GAME_CONFIG="${2:-release}" ;;
    *) echo "usage: $0 [cres|cshell|object|all] [release|final]"; exit 1 ;;
esac
case "${1:-all}" in
    cres|cshell|object) nolfsrc_make "$1" ;;
    all)                nolfsrc_make game ;;
    *) echo "usage: $0 [cres|cshell|object|all] [release|final]"; exit 1 ;;
esac
