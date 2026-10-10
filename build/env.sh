# Source this to get the VC6-under-Wine build environment. It runs inside the
# build container (docker/Dockerfile; `make shell` gives you one), where Wine
# is on PATH and the toolchain is toolchain/ in this checkout
# (toolchain/README.md).

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MSVC="$REPO/toolchain/msvc600"
if [ ! -f "$MSVC/VC98/Bin/CL.EXE" ]; then
    echo "error: $MSVC/VC98/Bin/CL.EXE not found -- the toolchain is missing (toolchain/README.md)" >&2
    return 1 2>/dev/null || exit 1
fi
if ! command -v wine >/dev/null 2>&1; then
    echo "error: wine is not on PATH -- run this inside the build container (make shell)" >&2
    return 1 2>/dev/null || exit 1
fi
WINE=wine
WINESERVER=wineserver

export WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml="

# Windows-side view of the paths (wine maps Z: -> /)
WREPO="z:${REPO//\//\\}"
WVC="z:${MSVC//\//\\}\\VC98"
WMSDEV="z:${MSVC//\//\\}\\Common\\MSDev98"

# cl.exe / link.exe / rc.exe environment (wine imports these env vars)
export WINEPATH="$WVC\\Bin;$WMSDEV\\Bin"
export INCLUDE="$WVC\\INCLUDE;$WVC\\MFC\\INCLUDE;$WVC\\ATL\\INCLUDE"
export LIB="$WVC\\LIB;$WVC\\MFC\\LIB"

# src/nolfsrc/ (nolfsrc_debug.h) is on every compiler's include path, last,
# so any file of any target can include it. It holds nothing else, so it
# cannot change which header another #include finds.
WNOLFSRC="$WREPO\\src\\nolfsrc"
INCLUDE="$INCLUDE;$WNOLFSRC"

# Extra flags for every compile and every link, for the debug build
# (build/debug.sh sets them). Empty otherwise -- and the release build's
# command lines are then exactly what they always were.
NOLFSRC_CFLAGS="${NOLFSRC_CFLAGS:-}"
NOLFSRC_LDFLAGS="${NOLFSRC_LDFLAGS:-}"

# The compile flags go AFTER the command's own (a later flag wins: /Oy- must
# follow the /O2 that implies /Oy), but before a /link, after which the
# rest of the line belongs to the linker. Sets NOLFSRC_ARGS.
nolfsrc_cl_args() {
    local a linked=0
    NOLFSRC_ARGS=()
    for a in "$@"; do
        if [ $linked = 0 ] && [ "${a,,}" = "/link" ]; then
            NOLFSRC_ARGS+=($NOLFSRC_CFLAGS); linked=1
        fi
        NOLFSRC_ARGS+=("$a")
    done
    [ $linked = 1 ] || NOLFSRC_ARGS+=($NOLFSRC_CFLAGS)
}

run_cl()   { nolfsrc_cl_args "$@"; "$WINE" "$MSVC/VC98/Bin/CL.EXE" "${NOLFSRC_ARGS[@]}"; }

# Compiler-revision override: CL_REV=8168|8447 makes run_cl use
# toolchain/msvc-6.00-<rev> (with its own CRT headers) instead of the default
# msvc600 (cl 12.00.8804). The build scripts set it per target, to the
# revision each original binary was built with.
CLDIR=""
if [ -n "${CL_REV:-}" ] && [ -d "$REPO/toolchain/msvc-6.00-$CL_REV" ]; then
    CLDIR="$REPO/toolchain/msvc-6.00-$CL_REV"
elif [ -n "${CL_REV:-}" ] && [ -d "$REPO/toolchain/msvc-$CL_REV" ]; then
    CLDIR="$REPO/toolchain/msvc-$CL_REV"
fi
if [ -n "$CLDIR" ]; then
    WCLDIR="z:${CLDIR//\//\\}"
    run_cl() { INCLUDE="$WCLDIR\\INCLUDE;$WCLDIR\\MFC\\INCLUDE;$WNOLFSRC" \
               nolfsrc_cl_args "$@"; "$WINE" "$CLDIR/BIN/cl.exe" "${NOLFSRC_ARGS[@]}"; }
fi
# d3d.ren is built with Intel C/C++ 4.0 (toolchain/icl-4.0), as the original
# was; icl uses msvc-6.00-8168's headers and libraries beside its own.
ICLDIR="$REPO/toolchain/icl-4.0"
run_icl() {
    if [ ! -f "$ICLDIR/BIN/icl.exe" ]; then
        echo "error: $ICLDIR/BIN/icl.exe not present." >&2
        echo "       d3d.ren needs Intel C/C++ 4.0; see toolchain/README.md" >&2
        return 1
    fi
    local WI WV
    WI="z:${ICLDIR//\//\\}"
    WV="z:${REPO//\//\\}\\toolchain\\msvc-6.00-8168"
    nolfsrc_cl_args "$@"
    INCLUDE="$WI\\INCLUDE;$WV\\INCLUDE;$WNOLFSRC" LIB="$WI\\LIB;$WV\\LIB" \
        "$WINE" "$ICLDIR/BIN/icl.exe" "${NOLFSRC_ARGS[@]}"
}

run_link() { "$WINE" "$MSVC/VC98/Bin/LINK.EXE" $NOLFSRC_LDFLAGS "$@"; }
run_lib()  { "$WINE" "$MSVC/VC98/Bin/LIB.EXE"  "$@"; }
run_rc()   { "$WINE" "$MSVC/Common/MSDev98/Bin/RC.EXE" "$@"; }
