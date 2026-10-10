#!/usr/bin/env bash
# The scoreboard: how much of the build is byte-identical to the retail game,
# held to what has already been recorded. See progress/README.md.
#
#   make progress           (on the host: runs `build/progress.sh check` in a
#                            container, with the disc images and $NOLF_SOURCE)
#   make progress-update    (`build/progress.sh update`: record the run)
#   build/progress.sh check|update   (inside the build container)
#
# Scores what the last `make` left in src/engine/*/Release/ and
# src/lithshared/out/; it compiles nothing. Needs the disc images (the retail
# images are extracted into out/reference/ the first time) and, to score the
# lithshared libraries, NOLF_SOURCE: the NOLF Source Code v1.003 release.
#
#   check   tools/progress.py, then tools/ratchet.py check: fails if a
#           function recorded in progress/identical.txt no longer matches
#           (COMPLETE=1: also unless every function matches; COVERED=1:
#           unless nothing is unwritten or unplaced)
#   update  tools/progress.py, then tools/ratchet.py update: records the run
#           in progress/ and rewrites README.md's table (ACCEPT_LOSSES=1 to
#           record a run that lost functions)
set -e
cd "$(dirname "${BASH_SOURCE[0]}")/.."

mode="${1:-check}"
case "$mode" in
    check|update) ;;
    *) echo "usage: build/progress.sh check|update" >&2; exit 2 ;;
esac

# the images, and the tables build/reference.sh derives from them
for f in lithtech.exe server.dll d3d.ren NOLF.exe ltmsg.dll \
         CShell.dll Object.lto CRes.dll \
         server-functions.csv lithtech-functions.csv d3d-functions.csv \
         NOLF-functions.csv CShell-functions.csv Object-functions.csv \
         server-vtables.csv lithtech-vtables.csv d3d-vtables.csv; do
    if [ ! -f "out/reference/$f" ]; then
        echo "== retail images -> out/reference/ (for scoring only)"
        bash build/reference.sh
        break
    fi
done

python3 tools/progress.py
echo
if [ "$mode" = check ]; then
    python3 tools/ratchet.py check ${COMPLETE:+--complete} ${COVERED:+--covered}
else
    python3 tools/ratchet.py update ${ACCEPT_LOSSES:+--accept-losses}
fi
