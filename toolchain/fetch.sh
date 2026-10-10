#!/usr/bin/env bash
# Assemble toolchain/ from public copies of the original compiler media.
#
#   bash toolchain/fetch.sh [--force] [msvc600] [msvc-6.00-8168] [msvc-6.00-8447] [icl-4.0]
#
# `make toolchain` runs this in the build container, which has every tool it
# needs (curl, bsdtar, 7z, cabextract, unar, unshield). With no names it
# builds all four trees. A tree that is already present and matches its
# digest is left alone; one that does not match is an error unless --force.
#
# Downloads (about 2 GB) are verified by SHA-256 and kept in
# toolchain/downloads/ so a rerun fetches nothing; delete that directory once
# the trees are built if you want the space back. Every finished tree is
# checked against a digest of its full contents (every file's SHA-256), so
# what you get is byte-for-byte the toolchain this source was matched with.
set -euo pipefail

TC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DL="$TC/downloads"
WORK="$TC/.work"
FORCE=0

# --- the media -------------------------------------------------------------
VS6RTM_URL='https://archive.org/download/visualstudiov60enterpriseedition_199807/Visual%20Studio%20v6.0%20Enterprise%20Edition%20%28Disc%201%29%28X03-78941%29%28Microsoft%20Corporation%29%28August%201998%29.iso'
VS6RTM_SHA=fbcc23765fd3377b8e05f4c1f90151e2f7b3c928b1d4f2595242c92ebe99fab8
VS6SP3_URL='https://archive.org/download/msvs6sp3/Microsoft%20Visual%20Studio%206.0%20Service%20Pack%203.iso'
VS6SP3_SHA=92122f31beb62d99eb6a0545f5af0366caaa940b46515cd393ca15fe03741814
TECHNET_URL='https://archive.org/download/ms-technet-tnsb9908/TNSB9908.iso'
TECHNET_SHA=f3eefbd0b7d7958a44ef63c2bb28bcf75ef8d5adcd2e65082f54fb399004a928
ICL_URL='https://archive.org/download/iclv.-4.0/ICLv.4.0.rar'
ICL_SHA=2a62ca821286df3d62be22482cd5241bac846efb5ca98635101f416f09a0d85b
# A portable VC6 SP6 tree, pinned to one commit. GitHub does not promise that
# its generated tarballs are byte-stable, so the archive itself is not
# pinned; the extracted tree is, by its digest below.
MSVC600_COMMIT=001c4bafdcf2ef4b474d693acccd35a91e848f40
MSVC600_URL="https://codeload.github.com/itsmattkc/MSVC600/tar.gz/$MSVC600_COMMIT"

# --- the finished trees: SHA-256 over `sha256sum` of every file, sorted -----
declare -A TREE_SHA=(
    [msvc600]=78ff2ccc2c2c5f9e38cb0155811672b0a4de56f1e673ef779442ce4e89d93edb
    [msvc-6.00-8168]=0b9efddc410c097a845a014396ad5402c355becb39478f8fc801fd6481e4607f
    [msvc-6.00-8447]=d94067004094d88bbbfd1c03b410d4a6a6e96697e6dc5bd7b53f5d3c62cd5297
    [icl-4.0]=0355308ba40347b1ec7d75ffc46736735b5230a25c27aa9e5f4f6e399a6bb8f7
)
ALL=(msvc600 msvc-6.00-8168 msvc-6.00-8447 icl-4.0)

die() { echo "toolchain: $*" >&2; exit 1; }

tree_sha() {
    (cd "$1" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum) \
        | sha256sum | cut -c1-64
}

# fetch FILE URL [SHA256]: download into downloads/ unless already there (and valid).
fetch() {
    local dest="$DL/$1" url="$2" sha="${3:-}"
    if [ -f "$dest" ] && { [ -z "$sha" ] || echo "$sha  $dest" | sha256sum -c --status; }; then
        echo "   have    $1"
        return
    fi
    mkdir -p "$DL"
    echo "   fetch   $1"
    curl --fail --location --retry 5 --retry-all-errors --connect-timeout 30 \
        --continue-at - --progress-bar --output "$dest.part" "$url" \
        || die "download failed: $url"
    if [ -n "$sha" ] && ! echo "$sha  $dest.part" | sha256sum -c --status; then
        rm -f "$dest.part"
        die "$1: SHA-256 mismatch (expected $sha)"
    fi
    mv -f "$dest.part" "$dest"
}

# Lower-case every name below the given directories, so a service-pack overlay
# replaces a file instead of creating a second copy that differs only in case.
lowercase() {
    local path name lower
    while IFS= read -r -d '' path; do
        name=$(basename "$path")
        lower=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')
        [ "$name" = "$lower" ] || mv "$path" "$(dirname "$path")/$lower"
    done < <(find "$@" -depth -mindepth 1 -print0)
}

VC_DIRS=(BIN INCLUDE LIB MFC/INCLUDE MFC/LIB)

# VC6 RTM (cl/c2 12.00.8168): VC98's command-line tools, headers, CRT and MFC
# libraries from Disc 1, plus the PDB writer from Common.
build_8168() {
    local out="$1" x="$WORK/rtm"
    fetch VS6RTM.iso "$VS6RTM_URL" "$VS6RTM_SHA"
    mkdir -p "$x" "$out/MFC"
    bsdtar -xf "$DL/VS6RTM.iso" -C "$x" \
        VC98/BIN VC98/INCLUDE VC98/LIB VC98/MFC/INCLUDE VC98/MFC/LIB \
        COMMON/MSDEV98/BIN/MSPDB60.DLL
    local d
    for d in "${VC_DIRS[@]}"; do mv "$x/VC98/$d" "$out/$d"; done
    cp "$x/COMMON/MSDEV98/BIN/MSPDB60.DLL" "$out/BIN/"
    chmod -R u+w "$out"
    lowercase "${VC_DIRS[@]/#/$out/}"
    rm -rf "$x"
}

# VC6 SP3 (back end c2 12.00.8447): the RTM tree with Service Pack 3's English
# VC98 files laid over it. The pressed SP3 disc lacks the new back end; the
# Internet/Core edition of SP3 on the August 1999 TechNet disc carries it as
# msvcep.dll, which SP3 setup installs as VC98/BIN/C2.DLL.
build_8447() {
    local out="$1" x="$WORK/sp3" d
    build_8168 "$out"
    fetch VS6SP3.iso "$VS6SP3_URL" "$VS6SP3_SHA"
    fetch TNSB9908.iso "$TECHNET_URL" "$TECHNET_SHA"
    mkdir -p "$x/pressed" "$x/technet" "$x/core"
    7z x -y -o"$x/pressed" "$DL/VS6SP3.iso" \
        'enu/vc98/bin/*' 'enu/vc98/include/*' 'enu/vc98/lib/*' \
        'enu/vc98/mfc/include/*' 'enu/vc98/mfc/lib/*' >/dev/null
    7z x -y -o"$x/technet" "$DL/TNSB9908.iso" 'VSTUDIO/SP3/*' >/dev/null
    cabextract -q -d "$x/core" "$x/technet/VSTUDIO/SP3/VS6SP3_1.EXE"
    [ -f "$x/core/msvcep.dll" ] || die "the TechNet SP3 package has no msvcep.dll"
    chmod -R u+w "$x"
    lowercase "$x/pressed/enu/vc98"
    for d in "${VC_DIRS[@]}"; do
        d=$(printf '%s' "$d" | tr '[:upper:]' '[:lower:]')
        cp -a "$x/pressed/enu/vc98/$d/." "$out/${d^^}/"
    done
    cp "$x/core/msvcep.dll" "$out/BIN/c2.dll"
    chmod -R u+w "$out"
    rm -rf "$x"
}

# VC6 SP6 (c2 12.00.9782) as a portable Common/ + VC98/ tree.
build_msvc600() {
    local out="$1" x="$WORK/msvc600"
    fetch "MSVC600-$MSVC600_COMMIT.tar.gz" "$MSVC600_URL"
    mkdir -p "$x"
    tar -xzf "$DL/MSVC600-$MSVC600_COMMIT.tar.gz" -C "$x"
    mv "$x/MSVC600-$MSVC600_COMMIT" "$out"
    rm -rf "$x"
}

# Intel C/C++ 4.0 (build 99100), the evaluation release: a RAR holding the
# InstallShield setup (COMPEVAL.EXE, a self-extracting archive) whose
# data1.cab has the compiler's file groups.
build_icl() {
    local out="$1" x="$WORK/icl"
    fetch ICLv.4.0.rar "$ICL_URL" "$ICL_SHA"
    mkdir -p "$x/setup" "$x/files" "$out/BIN" "$out/INCLUDE" "$out/LIB"
    unar -q -o "$x/rar" "$DL/ICLv.4.0.rar" >/dev/null
    7z x -y -o"$x/setup" "$x/rar/ICL4.INS/COMPEVAL.EXE" >/dev/null
    unshield -d "$x/files" x "$x/setup/data1.cab" >/dev/null
    local f="$x/files"
    cp "$f"/Compiler_Bin_Files/* "$f"/Compiler_Bin_Files_\(shared\)/* \
       "$f"/Shared_DLLs/Msvcrt.dll "$out/BIN/"
    cp "$f"/Compiler_Include_files/* "$out/INCLUDE/"
    cp "$f"/Compiler_Lib_files_\(non-shared\)/* "$f"/Compiler_Lib_files_\(shared\)/* "$out/LIB/"
    # The evaluation's licence check is one function; the archive's own
    # $ICL.COM names the byte that turns it into an immediate return
    # (offset 0xA8D0, push ebp -> ret). The original is kept beside it.
    local icl="$out/BIN/icl.exe"
    [ "$(stat -c %s "$icl")" = 151041 ] || die "icl.exe is not the expected 151,041 bytes"
    [ "$(od -An -tx1 -j $((0xA8D0)) -N1 "$icl" | tr -d ' ')" = 55 ] \
        || die "icl.exe: unexpected byte at 0xA8D0"
    cp "$icl" "$icl.orig"
    printf '\xc3' | dd of="$icl" bs=1 seek=$((0xA8D0)) conv=notrunc status=none
    chmod -R u+w "$out"
    rm -rf "$x"
}

build() {
    local name="$1" dest="$TC/$1" tmp="$WORK/out-$1" got
    if [ -e "$dest" ]; then
        got=$(tree_sha "$dest")
        if [ "$got" = "${TREE_SHA[$name]}" ]; then
            echo "== $name: present and verified"
            return
        fi
        [ "$FORCE" = 1 ] || die "$name exists but does not match (rerun with --force to rebuild it)"
    fi
    echo "== $name"
    rm -rf "$tmp"
    case "$name" in
        msvc600)        build_msvc600 "$tmp" ;;
        msvc-6.00-8168) build_8168 "$tmp" ;;
        msvc-6.00-8447) build_8447 "$tmp" ;;
        icl-4.0)        build_icl "$tmp" ;;
    esac
    got=$(tree_sha "$tmp")
    [ "$got" = "${TREE_SHA[$name]}" ] \
        || die "$name: the assembled tree does not match (digest $got); left in ${tmp#$TC/}"
    rm -rf "$dest"
    mv "$tmp" "$dest"
    echo "   verified $name ($(du -sh "$dest" | cut -f1))"
}

targets=()
for a in "$@"; do
    case "$a" in
        --force) FORCE=1 ;;
        -h|--help) sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        msvc600|msvc-6.00-8168|msvc-6.00-8447|icl-4.0) targets+=("$a") ;;
        *) die "unknown argument: $a (try --help)" ;;
    esac
done
[ ${#targets[@]} -gt 0 ] || targets=("${ALL[@]}")

for tool in curl sha256sum bsdtar 7z cabextract unar unshield; do
    command -v "$tool" >/dev/null || die "$tool is required (make toolchain runs this in the build container, which has it)"
done
mkdir -p "$WORK"
for t in "${targets[@]}"; do build "$t"; done
rmdir "$WORK" 2>/dev/null || true
echo "== toolchain ready"
