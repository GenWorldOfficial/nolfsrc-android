#!/usr/bin/env bash
# Build the three game DLLs from Monolith's NOLF v1.003 source release
# (src/game/): CRes.dll, CShell.dll, Object.lto, with VC6 under Wine. Source
# lists come from the .dsp project files (tools/dsp2files.py); the `final`
# configuration adds _REZFILE/_FINAL, which running from .rez archives needs.
# Usage: build/build.sh [cres|cshell|object|all] [release|final]
set -e
NOLFSRC_DRIVES=1    # fixed Wine drives for src/ and toolchain/ (build/env.sh)
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

TARGET="${1:-all}"
CONFIG="${2:-release}"     # release | final  (final adds _REZFILE/_FINAL for .rez use)
if [ "$CONFIG" = final ]; then
    OUTDIR=Final_Release
    FINAL_DEFS='/D _REZFILE /D _FINAL'
else
    OUTDIR=Release
    FINAL_DEFS=''
fi

# Common Release compile flags (from the .dsp files; /FD and IDE-only flags dropped)
CL_COMMON='/nologo /MT /W3 /GX /O2 /c'

winpath() { echo "$WREPO\\${1//\//\\}"; }

# The modern build (build/variant.sh modern*: NOLFSRC_MODERN among the
# compile flags) also compiles the project's own new game code -- every .cpp
# in src/modern/cshell/ into CShell.dll, src/modern/object/ into Object.lto --
# finds headers under src/modern/ (#include "cshell/p1_input.h"), links
# winmm.lib, and adds src/modern/cres/*.rc (new strings) to CRes.dll. A
# release build has none of it: these are empty and its command lines are
# the same as ever.
MODERN=0
case " ${NOLFSRC_CFLAGS:-} " in *NOLFSRC_MODERN*) MODERN=1 ;; esac
MODERN_INCS=''
MODERN_LIBS=''
if [ $MODERN = 1 ]; then
    MODERN_INCS=' /I ..\..\modern'
    MODERN_LIBS=' winmm.lib'
fi

# modern_compile <projdir> <defines> <includes> <subdir>: compile
# src/modern/<subdir>/*.cpp (modern build only) into <projdir>/$OUTDIR/modern/
# and print the objects' paths, relative to <projdir>
modern_compile() {
    local projdir="$1" defines="$2" includes="$3" sub="$4" f files=()
    [ $MODERN = 1 ] || return 0
    for f in "$REPO/src/modern/$sub"/*.cpp; do
        [ -f "$f" ] && files+=("..\\..\\modern\\$sub\\$(basename "$f")")
    done
    [ ${#files[@]} -gt 0 ] || return 0
    mkdir -p "$projdir/$OUTDIR/modern"
    ( cd "$projdir" && run_cl $CL_COMMON $defines $includes /Fo"$OUTDIR/modern/" "${files[@]}" ) >&2 || return 1
    for f in "${files[@]}"; do
        f="${f##*\\}"; echo "$OUTDIR/modern/${f%.cpp}.obj"
    done
}

# Resolve a lithshared .lib: ours, built from src/lithshared by
# build/libs.sh; a missing library stops the build here.
# Paths are relative to the project dir (src/game/CShell, src/game/ObjectDLL).
libpath() {
    if [ ! -f "$REPO/src/lithshared/out/$1.lib" ]; then
        echo "error: src/lithshared/out/$1.lib missing -- run build/libs.sh all" >&2
        return 1
    fi
    echo "..\\..\\lithshared\\out\\$1.lib"
}

# Fails the build if any library is missing (libpath runs in a pipeline
# below, where its failure would otherwise be swallowed).
check_libs() {
    local l
    for l in "$@"; do libpath "$l" >/dev/null || exit 1; done
}

gen_parsers() {
    # ObjectDLL animation lexer/parser via the bundled Win32 flex/bison
    cd "$REPO/src/game/ObjectDLL"
    if [ ! -f AnimationLex.cpp ] || [ AnimationLex.l -nt AnimationLex.cpp ]; then
        echo "== flex AnimationLex.l"
        "$WINE" ../flex.exe -Pyyanimation -oAnimationLex.cpp AnimationLex.l
    fi
    if [ ! -f AnimationParse.cpp ] || [ AnimationParse.y -nt AnimationParse.cpp ]; then
        echo "== bison AnimationParse.y"
        "$WINE" ../bison.exe --defines -v --verbose -pyyanimation -oAnimationParse.cpp AnimationParse.y
        # per the .dsp custom build step: the bison defines file becomes AnimationLexSymbols.h
        rm -f AnimationLexSymbols.h
        mv -f animationparse.cpp.h AnimationLexSymbols.h 2>/dev/null || mv -f AnimationParse.cpp.h AnimationLexSymbols.h
        rm -f animationparse.cpp.output AnimationParse.cpp.output
    fi
    cd "$REPO"
}

compile_list() {
    # compile_list <projdir> <outdir> <defines> <includes> <pch-flag> <files...>
    local projdir="$1" outdir="$2" defines="$3" includes="$4" pchflag="$5"
    shift 5
    local resp="$projdir/$outdir/cl.rsp"
    mkdir -p "$projdir/$outdir"
    : > "$resp"
    echo "$CL_COMMON $defines $includes $pchflag /Fo\"$outdir/\" /Fp\"$outdir/proj.pch\"" >> "$resp"
    for f in "$@"; do
        echo "\"${f//\//\\}\"" >> "$resp"
    done
    ( cd "$projdir" && run_cl "@$outdir/cl.rsp" )
}

# CRes.dll's resources: ClientRes.rc, plus what the retail Game of the Year
# CRES.dll has and Monolith's v1.003 source of it lacks -- the GOTY content's
# strings and its credits -- which build/all.sh extracts from the disc into
# goty/ (tools/discextract.py goty). Compiled from goty/, where rc finds the
# GOTY credits.txt before ClientRes.rc's own (rc looks in its working
# directory first); everything else ClientRes.rc names comes from /i ..
cres_rc() {  # cres_rc <outdir, relative to src/game/ClientRes>  (run in ClientRes)
    if [ ! -f goty/goty.rc ] || [ ! -f goty/credits.txt ]; then
        echo "error: src/game/ClientRes/goty/ is missing -- build/all.sh writes it" >&2
        echo "       (or: python3 tools/discextract.py goty <isodir> src/game/ClientRes/goty)" >&2
        return 1
    fi
    printf '#include "..\\ClientRes.rc"\r\n#include "goty.rc"\r\n' > goty/cres_goty.rc
    # the modern build's new strings, one file per package (src/modern/cres/)
    if [ $MODERN = 1 ]; then
        local r
        for r in "$REPO"/src/modern/cres/*.rc; do
            [ -f "$r" ] && printf '#include "..\\..\\..\\modern\\cres\\%s"\r\n' "$(basename "$r")" >> goty/cres_goty.rc
        done
    fi
    ( cd goty && run_rc /l 0x409 /d NDEBUG /i .. /fo"..\\$1\\ClientRes.res" cres_goty.rc )
}

build_cres() {
    echo "==== ClientRes -> CRes.dll"
    local D="src/game/ClientRes" OUT=Release
    mkdir -p "$D/$OUT"
    ( cd "$D" &&
      run_cl /nologo /MT /W3 /GX /O2 /I "..\\shared" /D WIN32 /D NDEBUG /D _WINDOWS /Fo"$OUT/" /c main.cpp &&
      cres_rc "$OUT" &&
      run_link /nologo /subsystem:windows /dll /machine:I386 /out:"$OUT/CRes.dll" \
        "$OUT/main.obj" "$OUT/ClientRes.res" \
        kernel32.lib user32.lib gdi32.lib advapi32.lib )
}

build_cshell() {
    echo "==== ClientShellDLL -> CShell.dll"
    local D="src/game/ClientShellDLL" OUT=$OUTDIR
    local DEFS="/D NDEBUG /D _CLIENTBUILD /D WIN32 /D _WINDOWS /D _NOLFBUILD /D NO_PRAGMA_LIBS $FINAL_DEFS"
    local INCS='/I ..\shared /I ..\..\LT2\sdk\inc /I ..\..\LT2\lithshared\incs /I .'"$MODERN_INCS"
    mkdir -p "$D/$OUT"
    check_libs RegMgr_Debug StdLith CryptMgr LithFontMgr_LT2 ltguimgr_LT2 MFCStub ButeMgrNoMFC GameSpyClientMgr

    mapfile -t SRCS < <(python3 tools/dsp2files.py "$D/ClientShellDLL.dsp" Release cpp)

    # PCH creator first
    ( cd "$D" && run_cl $CL_COMMON $DEFS $INCS /Yc"stdafx.h" /Fp"$OUT/proj.pch" /Fo"$OUT/" "..\\Shared\\Stdafx.cpp" )

    # Everything else with /Yu
    local rest=()
    for f in "${SRCS[@]}"; do
        [[ "${f,,}" == *"stdafx.cpp" ]] && continue
        rest+=("$f")
    done
    compile_list "$D" "$OUT" "$DEFS" "$INCS" '/Yu"stdafx.h"' "${rest[@]}"

    # Link
    local objs=("$OUT/Stdafx.obj")
    for f in "${rest[@]}"; do
        local b="${f##*/}"; b="${b%.*}"
        objs+=("$OUT/$b.obj")
    done
    local mobjs
    mobjs=$(modern_compile "$D" "$DEFS" "$INCS" cshell) || exit 1
    [ -n "$mobjs" ] && mapfile -t -O "${#objs[@]}" objs <<<"$mobjs"
    local resp="$D/$OUT/link.rsp"
    : > "$resp"
    {
        echo '/nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:LIBCMTD'
        echo "/out:\"$OUT/CShell.dll\" /map:\"$OUT/CShell.map\""
        for o in "${objs[@]}"; do echo "\"${o//\//\\}\""; done
        # RegMgr_Debug, not RegMgr: the GOTY CShell.dll linked the Debug
        # build of regmgr.lib, first of the libraries (build/libs.sh,
        # build_regmgr_debug; the .dsp's /nodefaultlib:"LIBCMTD" is for it)
        for l in RegMgr_Debug StdLith CryptMgr LithFontMgr_LT2 ltguimgr_LT2 MFCStub ButeMgrNoMFC GameSpyClientMgr; do
            echo "\"$(libpath $l)\""
        done
        echo 'kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib'"$MODERN_LIBS"
    } >> "$resp"
    ( cd "$D" && run_link "@$OUT/link.rsp" )
}

build_object() {
    echo "==== ObjectDLL -> Object.lto"
    gen_parsers
    local D="src/game/ObjectDLL" OUT=$OUTDIR
    local DEFS="/D NDEBUG /D WIN32 /D _WINDOWS /D _NOLFBUILD /D NO_PRAGMA_LIBS $FINAL_DEFS"
    local INCS='/I ..\..\LT2\sdk\inc /I ..\..\LT2\lithshared\incs /I ..\shared /I .'"$MODERN_INCS"
    mkdir -p "$D/$OUT"
    check_libs StdLith CryptMgr MFCStub ButeMgrNoMFC GameSpyMgr RegMgr

    mapfile -t SRCS < <(python3 tools/dsp2files.py "$D/Object.dsp" Release cpp)

    ( cd "$D" && run_cl $CL_COMMON $DEFS $INCS /Yc"stdafx.h" /Fp"$OUT/proj.pch" /Fo"$OUT/" "..\\Shared\\Stdafx.cpp" )

    local rest=() nopch=()
    for f in "${SRCS[@]}"; do
        case "${f,,}" in
            *stdafx.cpp) continue ;;
            *animationlex.cpp|*animationparse.cpp|*ltquatbase.cpp) nopch+=("$f") ;;
            *) rest+=("$f") ;;
        esac
    done
    # generated files might not be in the dsp-derived list if dsp lists them; ensure present
    local have_lex=0 have_parse=0
    for f in "${nopch[@]}"; do
        [[ "${f,,}" == *animationlex.cpp ]] && have_lex=1
        [[ "${f,,}" == *animationparse.cpp ]] && have_parse=1
    done
    [ $have_lex -eq 0 ] && nopch+=("AnimationLex.cpp")
    [ $have_parse -eq 0 ] && nopch+=("AnimationParse.cpp")

    compile_list "$D" "$OUT" "$DEFS" "$INCS" '/Yu"stdafx.h"' "${rest[@]}"
    compile_list_nopch=("${nopch[@]}")
    local resp="$D/$OUT/cl2.rsp"
    : > "$resp"
    echo "$CL_COMMON $DEFS $INCS /Fo\"$OUT/\"" >> "$resp"
    for f in "${nopch[@]}"; do echo "\"${f//\//\\}\"" >> "$resp"; done
    ( cd "$D" && run_cl "@$OUT/cl2.rsp" )

    local objs=("$OUT/Stdafx.obj")
    for f in "${rest[@]}" "${nopch[@]}"; do
        local b="${f##*/}"; b="${b%.*}"
        objs+=("$OUT/$b.obj")
    done
    local mobjs
    mobjs=$(modern_compile "$D" "$DEFS" "$INCS" object) || exit 1
    [ -n "$mobjs" ] && mapfile -t -O "${#objs[@]}" objs <<<"$mobjs"
    local lresp="$D/$OUT/link.rsp"
    : > "$lresp"
    {
        echo '/nologo /subsystem:windows /dll /machine:I386'
        echo "/out:\"$OUT/Object.lto\" /map:\"$OUT/Object.map\""
        for o in "${objs[@]}"; do echo "\"${o//\//\\}\""; done
        for l in StdLith CryptMgr MFCStub ButeMgrNoMFC GameSpyMgr RegMgr; do
            echo "\"$(libpath $l)\""
        done
        echo 'kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib'"$MODERN_LIBS"
    } >> "$lresp"
    ( cd "$D" && run_link "@$OUT/link.rsp" )
}

case "$TARGET" in
    cres)   build_cres ;;
    cshell) build_cshell ;;
    object) build_object ;;
    all)    build_cres; build_cshell; build_object ;;
    *) echo "usage: $0 [cres|cshell|object|all] [release|final]"; exit 1 ;;
esac
echo "==== done"
