#!/usr/bin/env bash
# The game directory as one file, to copy to the machine that plays it:
#
#   build/package.sh out/nolf-<legacy|modern>-<release|debug>
#
# writes out/<name>.tar.gz, which unpacks to <name>/ (then <name>/run.sh --
# see its header; Wine needed there). build/all.sh runs it for the release
# build (step 6) and build/variant.sh for each variant, once its data and
# symbols are in place.
#
# Links are stored as the files they point to (-h); the per-machine state
# run.sh / run-launcher.sh create is left out. The archive is written beside
# and renamed into place, so a failed run never leaves a truncated one, and a
# plain .tar an older build made is removed with it.
set -e
OUT="${1:?usage: build/package.sh <game directory>}"
OUT="${OUT%/}"
NAME="$(basename "$OUT")"
PKG="$(dirname "$OUT")/$NAME.tar.gz"

tar -C "$(dirname "$OUT")" -h -cf - \
    --exclude=run-cmds.txt --exclude=cd --exclude='*.part' "$NAME" |
    gzip -6 > "$PKG.part"
mv "$PKG.part" "$PKG"
rm -f "$(dirname "$OUT")/$NAME.tar"
echo "   $PKG: $(du -h "$PKG" | cut -f1) (unpacks to $NAME/)"
