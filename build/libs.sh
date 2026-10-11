#!/usr/bin/env bash
# Build the lithshared static libraries the engine and game DLLs link, from
# src/lithshared/, into src/lithshared/out/: StdLith, CryptMgr, Lith, RezMgr,
# RegMgr (and RegMgr_Debug: build_regmgr_debug), MFCStub, ButeMgrNoMFC (the
# attribute parser plus the zTools containers), ltguimgr_LT2,
# LithFontMgr_LT2, and GameSpyMgr / GameSpyClientMgr (our own replacements
# for the GameSpy middleware). The
# LT2-era headers in src/LT2/lithshared take precedence over the newer copies
# beside the sources. Compiled with cl 12.00.8447, the revision the original
# libraries were built with, /MT /O2 like the game projects.
#
# Usage: build/libs.sh [stdlith|cryptmgr|lith|rezmgr|regmgr|mfcstub|ztools|butemgr|
#                       ltguimgr|lithfontmgr|gamespy|all]   (inside the build container)
set -e
export CL_REV="${CL_REV:-8447}"
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

LIB_CFLAGS="${LIB_CFLAGS:-/nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /c}"

build_stdlith() {
    echo "==== stdlith -> src/lithshared/out/StdLith.lib"
    local D="src/lithshared/stdlith" OUT=Release
    mkdir -p "$D/$OUT" src/lithshared/out
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS /I ..\\..\\LT2\\lithshared\\stdlith /I ..\\..\\LT2\\lithshared\\lith /I . /I sys\\win /I ..\\zlib /Fo\"$OUT/\"" >> "$resp"
    for f in abstractio dynarray fileio goodlinklist helpers l_allocator \
             linklist lithexception memory memoryio stringholder struct_bank; do
        echo "$f.cpp" >> "$resp"
    done
    echo "sys\\win\\compressedio.cpp" >> "$resp"
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
    ( cd "$D" && run_lib /nologo /out:"..\\out\\StdLith.lib" "$OUT/*.obj" )
}

build_generic() {  # build_generic <srcdir> <outlib> <extra-includes> <sources...>
    local D="src/lithshared/$1" OUTLIB="$2" INCS="$3"; shift 3
    echo "==== $D -> src/lithshared/out/$OUTLIB.lib"
    local OUT=Release
    mkdir -p "$D/$OUT" src/lithshared/out
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS $INCS /Fo\"$OUT/\"" >> "$resp"
    for f in "$@"; do echo "$f" >> "$resp"; done
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
    ( cd "$D" && run_lib /nologo /out:"..\\out\\$OUTLIB.lib" "$OUT/*.obj" )
}

build_cryptmgr() {
    build_generic CryptMgr CryptMgr '/I ..\\..\\LT2\\lithshared\\cryptmgr /I ..\\..\\LT2\\lithshared\\incs /I .' cryptmgr.cpp blowfish.cpp
}
build_rezmgr() {
    build_generic rezmgr RezMgr '/I ..\\..\\LT2\\lithshared\\incs /I .' rezmgr.cpp rezmgr2.cpp rezfile.cpp rezemu.cpp rezdir.cpp rezhash.cpp
}
build_lith() {
    build_generic lith Lith '/I .' basehash.cpp baselist.cpp virtlist.cpp
}
build_regmgr() {
    build_generic RegMgr RegMgr '/I ..\\..\\LT2\\lithshared\\incs /I .' regmgr.cpp
}
# The same regmgr.cpp built with regmgr60.dsp's *Debug* settings (/ZI /Od
# /D _DEBUG; /MTd, which is what the -defaultlib:LIBCMTD directive in the
# source release's libs/Debug/RegMgr.lib says it was built with -- the .dsp
# says /MDd). The Game of the Year CShell.dll linked this build, not the
# Release one: its five CRegMgr functions (0x100e1460..0x100e1797) are the
# unoptimised code, byte for byte, and sit ahead of StdLith, right after the
# last game object -- which is also why ClientShellDLL.dsp's Release link
# says /nodefaultlib:"LIBCMTD". Only CShell links it (build/build.sh);
# NOLF.exe and Object.lto keep the Release RegMgr.lib, which stays the one
# scored against the source release's Release library. All 14 functions
# match libs/Debug/RegMgr.lib's.
build_regmgr_debug() {
    local D="src/lithshared/RegMgr" OUT=Debug
    echo "==== $D -> src/lithshared/out/RegMgr_Debug.lib"
    mkdir -p "$D/$OUT" src/lithshared/out
    ( cd "$D" && run_cl /nologo /MTd /W3 /GX /ZI /Od /D WIN32 /D _DEBUG /D _WINDOWS /D NO_PRAGMA_LIBS \
        /I ..\\..\\LT2\\lithshared\\incs /I . /Fo"$OUT/" /Fd"$OUT/" /c regmgr.cpp )
    ( cd "$D" && run_lib /nologo /out:"..\\out\\RegMgr_Debug.lib" "$OUT\\regmgr.obj" )
}
build_mfcstub() {
    build_generic MFCStub MFCStub '/I ..\\..\\LT2\\lithshared\\mfcstub /I ..\\..\\LT2\\lithshared\\incs /I ..\\..\\LT2\\lithshared\\lith /I .' mfcs_string.cpp mfcs_rect.cpp mfcs_point.cpp
}

build_ztools() {
    local D="src/lithshared/ztools" OUT=Release
    echo "==== ztools (partial) -> $D/$OUT/*.obj"
    mkdir -p "$D/$OUT"
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS /D _NOMFC /I ..\\..\\LT2\\lithshared\\incs /I . /Fo\"$OUT/\"" >> "$resp"
    for f in "$D"/*.cpp; do echo "$(basename $f)" >> "$resp"; done
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
}

build_butemgr() {
    local D="src/lithshared/ButeMgr" OUT=Release
    echo "==== $D -> $D/$OUT/butemgr.obj"
    mkdir -p "$D/$OUT"
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS /D _NOMFC /I ..\\..\\LT2\\lithshared\\butemgr /I ..\\..\\LT2\\lithshared\\mfcstub /I ..\\..\\LT2\\lithshared\\incs /I ..\\..\\LT2\\lithshared\\lith /I ..\\..\\LT2\\lithshared\\cryptmgr /I . /Fo\"$OUT/\"" >> "$resp"
    echo "butemgr.cpp" >> "$resp"
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
}

assemble_butemgr_lib() {
    mkdir -p src/lithshared/out
    ( cd src/lithshared && run_lib /nologo /out:"out\\ButeMgrNoMFC.lib" \
        "ButeMgr\\Release\\butemgr.obj" "ztools\\Release\\*.obj" )
}

build_ltguimgr() {
    local D="src/lithshared/ltguimgr" OUT=Release
    echo "==== $D -> $D/$OUT/*.obj"
    mkdir -p "$D/$OUT"
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS /D _NOMFC /D _LITHTECH2 /I ..\\..\\LT2\\lithshared\\ltguimgr /I ..\\..\\LT2\\lithshared\\incs /I ..\\..\\LT2\\lithshared\\mfcstub /I ..\\..\\LT2\\lithshared\\lith /I ..\\..\\LT2\\sdk\\inc /I . /Fo\"$OUT/\"" >> "$resp"
    for f in "$D"/*.cpp; do echo "$(basename $f)" >> "$resp"; done
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
    mkdir -p src/lithshared/out
    ( cd "$D" && run_lib /nologo /out:"..\\out\\ltguimgr_LT2.lib" "$OUT/*.obj" )
}

build_lithfontmgr() {
    local D="src/lithshared/lithfontmgr" OUT=Release
    echo "==== $D -> $D/$OUT/*.obj"
    mkdir -p "$D/$OUT"
    local resp="$D/$OUT/cl.rsp"
    : > "$resp"
    echo "$LIB_CFLAGS /D _LITHTECH2 /I ..\\..\\LT2\\lithshared\\lithfontmgr /I ..\\..\\LT2\\lithshared\\incs /I ..\\..\\LT2\\sdk\\inc /I . /Fo\"$OUT/\"" >> "$resp"
    for f in "$D"/*.cpp; do echo "$(basename $f)" >> "$resp"; done
    ( cd "$D" && run_cl "@$OUT/cl.rsp" )
    mkdir -p src/lithshared/out
    ( cd "$D" && run_lib /nologo /out:"..\\out\\LithFontMgr_LT2.lib" "$OUT/*.obj" )
}

build_gamespy() {
    local D="src/lithshared/gamespy" OUT=Release
    echo "==== $D -> src/lithshared/out/GameSpyMgr.lib, GameSpyClientMgr.lib"
    mkdir -p "$D/$OUT" src/lithshared/out
    ( cd "$D" && run_cl $LIB_CFLAGS /I ..\\..\\LT2\\lithshared\\incs /I ..\\..\\LT2\\lithshared\\mfcstub \
        /I . /Fo"$OUT/" GameSpyMgr.cpp GameSpyClientMgr.cpp )
    ( cd "$D" && run_lib /nologo /out:"..\\out\\GameSpyMgr.lib" "$OUT\\GameSpyMgr.obj" )
    ( cd "$D" && run_lib /nologo /out:"..\\out\\GameSpyClientMgr.lib" "$OUT\\GameSpyClientMgr.obj" )
}

case "${1:-all}" in
    stdlith)  build_stdlith ;;
    cryptmgr) build_cryptmgr ;;
    rezmgr)   build_rezmgr ;;
    lith)     build_lith ;;
    regmgr)   build_regmgr; build_regmgr_debug ;;
    mfcstub)  build_mfcstub ;;
    ztools)   build_ztools ;;
    butemgr)  build_butemgr;  assemble_butemgr_lib ;;
    ltguimgr) build_ltguimgr ;;
    lithfontmgr) build_lithfontmgr ;;
    gamespy)  build_gamespy ;;
    all)      build_stdlith; build_cryptmgr; build_lith; build_rezmgr
              build_regmgr; build_regmgr_debug; build_mfcstub
              build_ztools; build_butemgr; assemble_butemgr_lib
              build_ltguimgr; build_lithfontmgr; build_gamespy ;;
    *) echo "usage: $0 [stdlith|lith|rezmgr|cryptmgr|regmgr|mfcstub|ztools|butemgr|ltguimgr|lithfontmgr|gamespy|all]"; exit 1 ;;
esac
echo "==== done"
