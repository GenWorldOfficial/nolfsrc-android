#!/usr/bin/env bash
# linux-install.sh -- install the game data from your two NOLF discs.
#
#   ./linux-install.sh <disc image> <disc image>
#   ./linux-install.sh                 (finds the images in this directory
#                                       or the one above it)
#
# The images are the two discs of The Operative: No One Lives Forever, Game
# of the Year Edition (Europe), in either order, each as any of
#
#   .iso   a plain image (2048-byte sectors), e.g. your own rip
#   .bin   redump's raw image (2352-byte sectors), or its .cue
#   .zip   redump's download as archive.org serves it (the .cue and .bin)
#
# A .bin, .cue or .zip is first converted to a temporary .iso in this
# directory (about 0.6 GB a disc, removed at the end; this needs python3).
# The script checks each .iso's SHA-256, copies the game's data out of them
# into this directory, and checks every file it writes; nothing else is
# changed. It can be run again: a file already in place is checked and kept.
# Then play with ./linux-run.sh (or ./linux-run-launcher.sh).
#
# It needs bash, dd, sha256sum, base64, head and stat (GNU coreutils; a
# Steam Deck has them). The list of files, at the end of this script, says
# for each one which bytes of which disc it is made of (tools/mkinstall.py).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SELF="$HERE/$(basename "${BASH_SOURCE[0]}")"

for t in dd sha256sum base64 head stat sed; do
    command -v "$t" >/dev/null || { echo "linux-install.sh: needs $t" >&2; exit 1; }
done

data() { sed -n '/^__NOLF_INSTALL_DATA__$/,$p' "$SELF" | tail -n +2; }

# The discs: their sizes and checksums, from the list.
declare -A DSIZE DSHA DNAME DRAW DISC
FOUND=0
while read -r tag n size sha name; do
    case "$tag" in
    D) DSIZE[$n]=$size; DSHA[$n]=$sha; DNAME[$n]=$name ;;
    R) DRAW[$n]=$size ;;
    esac
done < <(data)

# Converted images, removed however the script ends.
TEMPS=()
cleanup() { [ ${#TEMPS[@]} -eq 0 ] || rm -f "${TEMPS[@]}"; }
trap cleanup EXIT

# raw <cmd> <file> [out]: a redump image's raw (2352-byte sector) data.
#   raw size <.bin|.zip>          its size (a .zip's one .bin)
#   raw iso  <.bin|.zip> <out>    the 2048 bytes of user data of each sector
#                                 (MODE1: after the 16-byte sync and header)
raw() {
    command -v python3 >/dev/null || {
        echo "linux-install.sh: $(basename "$2"): a .bin, .cue or .zip needs python3 to convert it" >&2
        echo "  (or convert it to an .iso yourself)" >&2
        exit 1; }
    python3 - "$@" <<'PY'
import sys, zipfile
cmd, src = sys.argv[1], sys.argv[2]
try:
    z = zipfile.ZipFile(src) if zipfile.is_zipfile(src) else None
except (zipfile.BadZipFile, OSError):    # not a disc: matches no size
    z = None
    if src.lower().endswith(".zip"):
        print(0)
        sys.exit(0)
if z:
    bins = [i for i in z.infolist() if i.filename.lower().endswith(".bin")]
    if len(bins) != 1:                    # not a disc: matches no size
        print(0)
        sys.exit(0)
    size, f = bins[0].file_size, (z.open(bins[0]) if cmd == "iso" else None)
else:
    import os
    size, f = os.path.getsize(src), (open(src, "rb") if cmd == "iso" else None)
if cmd == "size":
    print(size)
    sys.exit(0)
S, K, B = 2352, 16, 512
with open(sys.argv[3], "wb") as out:
    while True:
        buf = f.read(S * B)
        if not buf:
            break
        while len(buf) % S:               # a zip stream can read short
            more = f.read(S - len(buf) % S)
            if not more:
                sys.exit(f"linux-install.sh: {src}: not whole 2352-byte sectors")
            buf += more
        out.write(b"".join(buf[i + K:i + K + 2048] for i in range(0, len(buf), S)))
PY
}

# take <image> <label>: which disc an .iso is, if any (by size, then SHA-256)
take() {
    local f=$1 label=$2 size n
    size=$(stat -L -c %s "$f")
    for n in "${!DSIZE[@]}"; do
        [ -z "${DISC[$n]:-}" ] && [ "$size" = "${DSIZE[$n]}" ] || continue
        echo "checking $label ..."
        if [ "$(sha256sum < "$f" | cut -d' ' -f1)" = "${DSHA[$n]}" ]; then
            DISC[$n]=$f
            FOUND=$((FOUND + 1))
            echo "  disc $n"
            return 0
        fi
        echo "  the size of disc $n, but not its contents (a bad copy?)"
    done
    return 1
}

CANDIDATES=("$@")
if [ ${#CANDIDATES[@]} -eq 0 ]; then
    # .iso first: one is used as it is, the others need converting.
    shopt -s nullglob nocaseglob
    CANDIDATES=("$HERE"/*.iso "$HERE"/../*.iso "$HERE"/*.bin "$HERE"/../*.bin
                "$HERE"/*.zip "$HERE"/../*.zip)
    shopt -u nullglob nocaseglob
fi
for f in "${CANDIDATES[@]}"; do
    [ $FOUND -lt ${#DSIZE[@]} ] || break                # both found
    [ -f "$f" ] || { echo "linux-install.sh: $f: no such file" >&2; exit 1; }
    label=$(basename "$f")
    case "${f,,}" in
    *.cue)
        bin=$(sed -n 's/^[[:space:]]*FILE[[:space:]]*"\(.*\)"[[:space:]].*/\1/p' "$f" | head -1)
        grep -qi 'TRACK[[:space:]]*01[[:space:]]*MODE1/2352' "$f" && [ -n "$bin" ] || {
            echo "  $label: not a one-track MODE1/2352 cue sheet; skipped"; continue; }
        f="$(dirname "$f")/$bin"
        [ -f "$f" ] || { echo "linux-install.sh: $label names $bin, which is not beside it" >&2; exit 1; }
        ;& # on as the .bin
    *.bin|*.zip)
        size=$(raw size "$f")
        for n in "${!DRAW[@]}"; do
            [ -z "${DISC[$n]:-}" ] && [ "$size" = "${DRAW[$n]}" ] || continue
            tmp="$HERE/.disc$n.iso.part"
            TEMPS+=("$tmp")
            echo "converting $label to an .iso ..."
            raw iso "$f" "$tmp"
            take "$tmp" "$label (converted)" || rm -f "$tmp"
            break
        done ;;
    *)  take "$f" "$label" || true ;;
    esac
done
missing=0
for n in $(printf '%s\n' "${!DSIZE[@]}" | sort); do
    if [ -z "${DISC[$n]:-}" ]; then
        echo "linux-install.sh: no image of disc $n: ${DNAME[$n]}" >&2
        echo "  (.iso: ${DSIZE[$n]} bytes, SHA-256 ${DSHA[$n]}; or redump's .bin/.cue or .zip)" >&2
        missing=1
    fi
done
[ $missing = 0 ] || exit 1

# The files.
ok() {  # ok <path> <size> <sha>: is the file there, and right?
    [ -f "$1" ] && [ "$(stat -c %s "$1")" = "$2" ] &&
        [ "$(sha256sum < "$1" | cut -d' ' -f1)" = "$3" ]
}
written=0 kept=0
while read -r tag a b c d; do
    case "$tag" in
    F)  name=$a; dest="$HERE/$a"; size=$b; sha=$c; flag=$d; part="$dest.part"; skip=0
        if [ "$flag" = once ] && [ -e "$dest" ]; then skip=1
        elif ok "$dest" "$size" "$sha"; then skip=1
        fi
        if [ $skip = 1 ]; then kept=$((kept + 1)); else
            echo "$a"
            mkdir -p "$(dirname "$dest")"
            : > "$part"
        fi ;;
    C)  [ $skip = 1 ] || dd if="${DISC[$a]}" of="$part" bs=1M iflag=skip_bytes,count_bytes \
            skip="$b" count="$c" oflag=append conv=notrunc status=none ;;
    Z)  [ $skip = 1 ] || head -c "$a" /dev/zero >> "$part" ;;
    L)  [ $skip = 1 ] || printf '%s' "$a" | base64 -d >> "$part" ;;
    E)  [ $skip = 1 ] && continue
        if ! ok "$part" "$size" "$sha"; then
            rm -f "$part"
            echo "linux-install.sh: $name came out wrong (a disk error?); the files before it are in place" >&2
            exit 1
        fi
        mv -f "$part" "$dest"
        written=$((written + 1)) ;;
    esac
done < <(data | sed -n '/^F /,$p')
mkdir -p "$HERE/Save"
echo "done: $written files written, $kept already in place. Play with ./linux-run.sh"
exit 0
