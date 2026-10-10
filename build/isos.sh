# Sourced by build/all.sh and the Makefile: where the two NOLF GOTY disc
# images (NOLF1-CD1.iso, NOLF1-CD2.iso -- see ISOS/README.md) are, and whether they
# are the right ones.
#
# iso_dir     prints $NOLF_ISOS if set, else ISOS/ in this checkout
# iso_verify  checks both images against ISOS/SHA256SUMS (the Makefile runs
#             it before every build; NOLF_SKIP_ISO_CHECK=1 skips it)
iso_dir() {
    local here d
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    d="${NOLF_ISOS:-$here/ISOS}"
    if [ -f "$d/NOLF1-CD1.iso" ] && [ -f "$d/NOLF1-CD2.iso" ]; then
        echo "$d"; return 0
    fi
    echo "error: NOLF1-CD1.iso / NOLF1-CD2.iso not found in $d" >&2
    echo "       put the two disc images there (see ISOS/README.md), or set NOLF_ISOS" >&2
    return 1
}

iso_verify() {
    local here d sums
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    sums="$here/ISOS/SHA256SUMS"
    d="$(iso_dir)" || return 1
    [ "${NOLF_SKIP_ISO_CHECK:-0}" = 1 ] && return 0
    # a stamp skips re-hashing 1.1 GB on every build while the images are unchanged
    local stamp="$here/out/.isos-verified" key
    key="$(stat -L -c '%s %Y' "$d/NOLF1-CD1.iso" "$d/NOLF1-CD2.iso" | sha256sum | cut -d' ' -f1)"
    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$key" ]; then
        return 0
    fi
    echo "== checking $d/NOLF1-CD1.iso, NOLF1-CD2.iso against ISOS/SHA256SUMS"
    if ! ( cd "$d" && sha256sum --quiet -c "$sums" ); then
        echo "error: the disc images in $d do not match ISOS/SHA256SUMS." >&2
        echo "       This build needs the No One Lives Forever Game of the Year Edition" >&2
        echo "       discs, imaged exactly as ISOS/README.md describes." >&2
        return 1
    fi
    mkdir -p "$(dirname "$stamp")" && echo "$key" > "$stamp"
}
