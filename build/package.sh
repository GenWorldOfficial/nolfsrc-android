#!/usr/bin/env bash
# The game directory as one file, to copy to the machine that plays it:
#
#   build/package.sh out/nolf-<legacy|modern>-<release|debug>
#
# writes two archives, each unpacking to <name>/ (then <name>/linux-run.sh --
# see its header; Wine needed there):
#
#   out/<name>-private.tar.gz   the whole directory, the retail game's data
#                               included: for your own machines, and anyone
#                               developing on the build
#   out/<name>-public.tar.gz    the same without anything that came from the
#                               discs, which its linux-install.sh /
#                               windows-install.bat copy out of the player's
#                               own disc images (tools/mkinstall.py): the one
#                               to give to other people. It needs the images
#                               in rez/isos/; without them it is skipped.
#
# build/all.sh runs it for the release build (step 6) and build/variant.sh
# for each variant, once its data and symbols are in place.
#
# Links are stored as the files they point to (-h). Each archive is a fresh
# install, whatever has been played in the directory: no saves (an empty
# Save/), no logs, autoexec.cfg as the shipped defaults.cfg, nolfcmds.txt as
# the build writes it (build/nolfcmds.sh; the launcher rewrites the
# directory's copy), and none of the per-machine state linux-run.sh /
# linux-run-launcher.sh create. Each is written beside and renamed into
# place, so a failed run never leaves a truncated one; the <name>.tar.gz and
# plain .tar older builds made are removed.
set -e
OUT="${1:?usage: build/package.sh <game directory>}"
OUT="${OUT%/}"
NAME="$(basename "$OUT")"
PARENT="$(dirname "$OUT")"
PKG="$PARENT/$NAME-private.tar.gz"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# The fresh files, staged beside the directory and renamed into it.
STAGE=".pkg-$NAME"
rm -rf "$PARENT/$STAGE"
mkdir -p "$PARENT/$STAGE"
trap 'rm -rf "$PARENT/$STAGE"' EXIT
cp "$OUT/defaults.cfg" "$PARENT/$STAGE/autoexec.cfg"
MIDDLEWARE=retail
grep -q '^EXT .*	binkw32\.dll$' "$OUT/MANIFEST.txt" 2>/dev/null && MIDDLEWARE=open
bash "$HERE/nolfcmds.sh" $MIDDLEWARE > "$PARENT/$STAGE/nolfcmds.txt"

tar -C "$PARENT" -h -cf - \
    --exclude="$NAME/Save/*" --exclude="$NAME/autoexec.cfg" --exclude="$NAME/nolfcmds.txt" \
    --exclude='*.log' --exclude=run-cmds.txt --exclude=cd --exclude='*.part' \
    --transform="s|^$STAGE/|$NAME/|" \
    "$NAME" "$STAGE/autoexec.cfg" "$STAGE/nolfcmds.txt" |
    gzip -6 > "$PKG.part"
mv "$PKG.part" "$PKG"
rm -f "$PARENT/$NAME.tar" "$PARENT/$NAME.tar.gz"
echo "   $PKG: $(du -h "$PKG" | cut -f1) (unpacks to $NAME/)"

# The public archive: the scripts beside the rest, the disc files left out.
PUB="$PARENT/$NAME-public.tar.gz"
rc=0
python3 "$HERE/../tools/mkinstall.py" "$OUT" "$PARENT/$STAGE" || rc=$?
if [ $rc = 3 ]; then
    echo "   $PUB: skipped (no disc images)"
    rm -f "$PUB"
elif [ $rc != 0 ]; then
    exit $rc
else
    sed "s|^|$NAME/|" "$PARENT/$STAGE/disc-files.txt" > "$PARENT/$STAGE/exclude.txt"
    tar -C "$PARENT" -h -cf - \
        --exclude="$NAME/Save/*" --exclude="$NAME/nolfcmds.txt" \
        --exclude='*.log' --exclude=run-cmds.txt --exclude=cd --exclude='*.part' \
        --anchored --no-wildcards --exclude-from="$PARENT/$STAGE/exclude.txt" \
        --transform="s|^$STAGE/|$NAME/|" \
        "$NAME" "$STAGE/nolfcmds.txt" "$STAGE/linux-install.sh" "$STAGE/windows-install.bat" |
        gzip -6 > "$PUB.part"
    mv "$PUB.part" "$PUB"
    echo "   $PUB: $(du -h "$PUB" | cut -f1) (unpacks to $NAME/; its data from the discs)"
fi
