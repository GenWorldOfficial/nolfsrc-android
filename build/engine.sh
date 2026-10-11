#!/usr/bin/env bash
# Build the engine binaries from src/engine/: ltmsg.dll, sres.dll, NOLF.exe
# (the launcher), server.dll, lithtech.exe and d3d.ren. build/all.sh runs the
# targets in order; each compiles a target's objects, and each *-link target
# (`launcher link` for NOLF.exe) links the whole image.
#
# The compiler revision and flags differ per binary, and for some files
# within one binary, because they reproduce what the original build used:
#   ltmsg, sres, launcher   cl 12.00 (toolchains/msvc-6.00-9782), /O2
#   server                  cl 12.00.8447, /O1, no /GX; localdriver.cpp /Os
#   lithtech                cl 12.00.8447, /O2; a few files with their own
#                           flags (NOPOOL_TUS, OD_TUS, GX_TUS below)
#   d3d                     Intel C/C++ 4.0 (toolchains/icl-4.0), with the
#                           per-file flag groups below; new files encode
#                           their group in their name (w<N>_*_<flags>.cpp)
#                           -- except D3D_CL_TUS, StdLith's l_allocator.cpp,
#                           which is VC6 library code: cl 12.00.8447 /O2 /GX
# src/engine/server/*.cpp is compiled into both server.dll and lithtech.exe.
#
# Each link reads committed link inputs in src/engine/<target>/link/: an
# alias list (aliases.rsp, turned into an object of weak externals by
# tools/mkaliasobj.py), and for lithtech.exe the stand-in vtables
# (vtables.txt, tools/mkvtabobj.py), for d3d.ren the data views
# (d3dviews.txt, tools/mkviewobj.py). Import libraries for Miles and DirectDraw
# are generated from their export lists (tools/mkimplib.py,
# src/engine/launcher/libs/mss32.def). Each link's report is
# src/engine/<target>/Release/<target>-link.txt.
#
# Each target can also be checked against the retail binaries, function by
# function (tools/exematch.py), which is the reconstruction loop of guides/:
#   build/engine.sh server verify          compile, then match every object
#   build/engine.sh server-one smgr.cpp [symbol]
#                                          recompile ONE file and match it
#                                          (also lithtech-one, d3d-one)
#   build/engine.sh server-link test       link, then load it under Wine
# The checks read the retail images from out/reference/ (build/reference.sh);
# building never does.
#
# src/engine/server/*.cpp is compiled into BOTH server.dll and lithtech.exe:
# after changing one, rebuild both (`server`, `lithtech`, or both *-one
# loops), or lithtech.exe's objects go stale.
#
# Usage: build/engine.sh <target> [verify|link|test]   (in the build container)
set -e
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"    # compile src/ without its variant blocks (build/env.sh)
cd "$REPO"

# The retail images the checks compare against (build/reference.sh).
REF="${NOLF_REFERENCE:-out/reference}"
ref() {  # ref <file> -> its path under $REF, or a loud error
    if [ ! -f "$REF/$1" ]; then
        echo "error: $REF/$1 missing -- run build/reference.sh" >&2
        return 1
    fi
    echo "$REF/$1"
}

# match <image> <obj|lib>... [-- symbol-filter]: tools/exematch.py, its report
# optionally narrowed to the lines naming one symbol (and the totals)
match() {
    local img="$1"; shift
    local args=() filter=""
    while [ $# -gt 0 ]; do
        if [ "$1" = "--" ]; then filter="${2:-}"; break; fi
        args+=("$1"); shift
    done
    if [ -n "$filter" ]; then
        python3 tools/exematch.py "$img" "${args[@]}" | grep -Ei "${filter}|^matched"
    else
        python3 tools/exematch.py "$img" "${args[@]}"
    fi
}

CL_COMMON='/nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /c'

build_ltmsg() {
    echo "==== src/engine/ltmsg -> ltmsg.dll"
    local D="src/engine/ltmsg" OUT=Release
    mkdir -p "$D/$OUT"
    ( cd "$D" &&
      run_cl $CL_COMMON /Fo"$OUT/" ltmsg.cpp &&
      run_rc /l 0x409 /d NDEBUG /fo"$OUT/ltmsg.res" ltmsg.rc &&
      run_link /nologo /subsystem:windows /dll /machine:I386 \
          /out:"$OUT/ltmsg.dll" /map:"$OUT/ltmsg.map" \
          "$OUT/ltmsg.obj" "$OUT/ltmsg.res" kernel32.lib )
}

# ltmsg.dll is resources around an empty DllMain: the resources are compared
# payload for payload, the code as a whole (both relocation-normalised).
verify_ltmsg() {
    python3 tools/bindiff.py src/engine/ltmsg/Release/ltmsg.dll "$(ref ltmsg.dll)"
    python3 tools/rsrcdiff.py src/engine/ltmsg/Release/ltmsg.dll "$(ref ltmsg.dll)"
}

build_launcher() {
    echo "==== src/engine/launcher -> objects"
    local D="src/engine/launcher" OUT=Release
    mkdir -p "$D/$OUT"
    ( cd "$D" &&
      run_cl /nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\lithshared\\incs /c /Fo"$OUT/" \
          stdafx.cpp launcher.cpp gamedlg.cpp gamehelp.cpp dlgconfig.cpp dlgcustom.cpp dlghelp.cpp dlgsound.cpp dlgdisplay.cpp preview.cpp cdcheck.cpp dib.cpp dibmgr.cpp dibpal.cpp RezFind.cpp Utils.cpp globals.cpp &&
      run_rc /l 0x409 /d NDEBUG /fo"$OUT/launcher.res" launcher.rc )
}

link_launcher() {
    echo "==== src/engine/launcher -> NOLF.exe (whole-exe link)"
    local D="src/engine/launcher" OUT=Release
    mss32_implib || return 1
    python3 tools/mkimplib.py --keep-decoration "$D/$OUT/mss32dec.lib" mss32.dll \
        $(sed -n "s/^ *\(AIL_[A-Za-z0-9_]*@[0-9]*\) *$/_\1/p" "$D/libs/mss32.def") || return 1
    ( cd "$D" &&
      run_link /nologo /subsystem:windows /machine:I386 \
          /out:"$OUT/NOLF.exe" /map:"$OUT/NOLF.map" \
          $OUT/launcher.obj $OUT/gamedlg.obj $OUT/gamehelp.obj \
          $OUT/dlgconfig.obj $OUT/dlgcustom.obj $OUT/dlghelp.obj \
          $OUT/dlgsound.obj $OUT/dlgdisplay.obj $OUT/preview.obj \
          $OUT/cdcheck.obj $OUT/dib.obj $OUT/dibmgr.obj $OUT/dibpal.obj \
          $OUT/RezFind.obj $OUT/Utils.obj $OUT/globals.obj $OUT/stdafx.obj \
          $OUT/launcher.res \
          ..\\..\\lithshared\\out\\RegMgr.lib $OUT\\mss32dec.lib $OUT\\mss32.lib \
          nafxcw.lib libcmt.lib \
          kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib \
          comdlg32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib \
          odbc32.lib odbccp32.lib )
}

# Every launcher function, matched in the retail NOLF.exe. The whole-exe link
# is refreshed first: tools/progress.py uses it as a second, unmasked check.
verify_launcher() {
    local D=src/engine/launcher/Release
    python3 tools/exematch.py "$(ref NOLF.exe)" \
        $D/launcher.obj $D/gamedlg.obj $D/gamehelp.obj $D/dlgconfig.obj $D/dlgcustom.obj \
        $D/dlghelp.obj $D/dlgsound.obj $D/dlgdisplay.obj $D/preview.obj $D/cdcheck.obj \
        $D/dib.obj $D/dibmgr.obj $D/dibpal.obj $D/RezFind.obj $D/Utils.obj \
        src/lithshared/out/RegMgr.lib
}

mss32_implib() {
    local D="src/engine/launcher" OUT=Release
    mkdir -p "$D/$OUT"
    ( cd "$D" && run_lib /nologo /machine:i386 /def:libs\\mss32.def /out:"$OUT\\mss32.lib" >/dev/null ) &&
        [ -f "$D/$OUT/mss32.lib" ] || { echo "error: could not build $D/$OUT/mss32.lib" >&2; return 1; }
}

build_sres() {
    echo "==== src/engine/sres -> sres.dll"
    local D="src/engine/sres" OUT=Release
    mkdir -p "$D/$OUT"
    ( cd "$D" &&
      run_cl $CL_COMMON /Fo"$OUT/" sres.cpp &&
      run_link /nologo /subsystem:windows /dll /machine:I386 \
          /out:"$OUT/sres.dll" "$OUT/sres.obj" kernel32.lib )
}

build_engine_scaffold() {  # build_engine_scaffold <dir> <incs> <srcs...>
    local D="src/engine/$1" INCS="$2"; shift 2
    echo "==== $D -> objects (cl /O1)"
    mkdir -p "$D/Release"
    ( cd "$D" && run_cl /nologo /MT /W3 /O1 /D WIN32 /D NDEBUG /D _WINDOWS /D LT_SERVER_TIMERS /D LT_SERVER_BUILD \
        /D NO_PRAGMA_LIBS $INCS /c /FoRelease/ "$@" )
}

# Per-file exceptions to a target's flags. server.dll: localdriver.cpp is
# /Os without /Og. lithtech.exe: clientmusic.cpp is /O2 without string pooling
# (/GF), its copy of localdriver.cpp is unoptimised (/Od, with intrinsics and
# inline expansion), and controlfile.cpp is built with C++ exceptions (/GX).
OS_TUS="localdriver.cpp"

NOPOOL_FLAGS="/Ot /Og /Oy /Ob1 /Oi /Gs /Gy"
NOPOOL_TUS="clientmusic.cpp"

OD_TUS="..\\server\\localdriver.cpp"
GX_TUS="controlfile.cpp"
GX_FLAGS="/GX /O2"
OD_FLAGS="/Od /Oi /Ob1"
build_engine_scaffold_os() {  # same (defines included), /Os without /Og
    local D="src/engine/$1" INCS="$2"; shift 2
    echo "==== $D -> objects (/Os TUs)"
    mkdir -p "$D/Release"
    ( cd "$D" && run_cl /nologo /MT /W3 /Os /Ob1 /D WIN32 /D NDEBUG /D _WINDOWS /D LT_SERVER_TIMERS /D LT_SERVER_BUILD \
        /D NO_PRAGMA_LIBS $INCS /c /FoRelease/ "$@" )
}

# d3d.ren's per-file icl flag groups. Files not in any group build /O2.
# /Qip: interprocedural register calling between functions of one file;
# /Qip_no_inlining: the same without inlining; /Os: favour size; /Oi-: CRT
# intrinsics called, not inlined; /Qrcd: fast float-to-int conversion;
# /Qsfalign16: realign the stack only for 16-byte locals; /G5: Pentium
# scheduling. Files named w<N>_*_<flags>.cpp join a group by their suffix.
D3D_QIP_TUS=$'d3dqip.cpp\nd3dqip2.cpp'
D3D_QIP_OS_TUS=$'d3dqip3.cpp\nbig7cc90.cpp\nd3dr5b.cpp'
D3D_RCD_TUS=$'big8fd0.cpp\nbig2b1b0.cpp\nbig051a0.cpp\nbig5dca0.cpp\nbig40e10.cpp\nw4_100234d0.cpp\nw16_10042e80.cpp'
D3D_OI_TUS=$'big4b720.cpp'
D3D_QIP_OI_TUS=''
D3D_OS_TUS=$'big729c0.cpp\nd3d.cpp\nd3dr5a.cpp\nd3dr5d_os.cpp\nd3dbatchb_os.cpp\nd3dmid.cpp\nd3dr2a_os.cpp\nd3dr2c_os.cpp\nd3dr3a_os.cpp\nd3ddraw_os.cpp\nd3dmid_os.cpp\nd3dneutral.cpp\nd3dopt2d.cpp\nd3dlow.cpp\nd3dhigh.cpp\nd3dbatchc.cpp\nd3dbatcha.cpp\nd3dr2b.cpp\nd3dr3b.cpp\nd3dr3d.cpp\nd3dr4a.cpp\nd3dr4c.cpp\nd3dr4b.cpp\nd3dr3c_os.cpp'
D3D_OS_OI_TUS=$'d3dr5c.cpp\nd3dr2d_os.cpp\nd3dbatchd.cpp\nbig4e3e4.cpp'
D3D_QIP_NI_TUS=$'d3dqip4.cpp'
D3D_QIP_NI_SF_TUS=$'big24090.cpp'
_d3dw() { ( cd "$(dirname "$0")/../src/engine/d3d" 2>/dev/null && ls $1 2>/dev/null ) || true; }
D3D_OS_TUS="$D3D_OS_TUS"$'\n'"$(_d3dw 'w[0-9]*_os.cpp')"
D3D_OS_OI_TUS="$D3D_OS_OI_TUS"$'\n'"$(_d3dw 'w[0-9]*_osoi.cpp')"
D3D_QIP_TUS="$D3D_QIP_TUS"$'\n'"$(_d3dw 'w[0-9]*_qip.cpp')"
D3D_QIP_OS_OI_TUS=$'d3dblit.cpp\nbig1ef30.cpp'$'\n'"$(_d3dw 'w[0-9]*_osoiqip.cpp')"
D3D_QIP_NI_TUS="$D3D_QIP_NI_TUS"$'\n'"$(_d3dw 'w[0-9]*_qipni.cpp')"
D3D_QIP_OS_NI_TUS="$(_d3dw 'w[0-9]*_osqipni.cpp')"$'\n'big6d910.cpp
D3D_OS_G5_TUS="$(_d3dw 'w[0-9]*_osg5.cpp')"
D3D_OS_SF_TUS="$(_d3dw 'w[0-9]*_ossf.cpp')"
D3D_QIP_OS_OI_SF_TUS="$(_d3dw 'w[0-9]*_osoiqipsf.cpp')"
D3D_QIP_SF_TUS="$(_d3dw 'w[0-9]*_qipsf.cpp')"
D3D_QIP_NI_SF_TUS="$D3D_QIP_NI_SF_TUS"$'\n'"$(_d3dw 'w[0-9]*_qipnisf.cpp')"
D3D_QIP_OS_OI_NI_TUS="$(_d3dw 'w[0-9]*_osoiqipni.cpp')"
# Not icl at all: StdLith's l_allocator.cpp (LAlloc and g_DefAlloc,
# 0x1007faa0..0x1007fb1d), VC6 library code linked into d3d.ren. It is built
# as StdLith is (build/libs.sh: cl 8447, LIB_CFLAGS); see w12_7fae0.cpp.
D3D_CL_TUS=$'w12_7fae0.cpp'
D3D_CL_FLAGS='/nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D NO_PRAGMA_LIBS /c'
d3d_cl() {  # d3d_cl <tu.cpp>...: compile with cl 8447 into src/engine/d3d/Release
    ( export CL_REV=8447; source "$(dirname "$0")/env.sh"
      cd src/engine/d3d && run_cl $D3D_CL_FLAGS /I ..\\..\\LT2\\lithshared\\stdlith /FoRelease/ "$@" )
}
d3d_flags() {  # d3d_flags <tu.cpp> -> the icl optimisation flags that file is built with
    local t="$1"
    if   grep -qxF "$t" <<<"$D3D_OS_G5_TUS";        then echo "/Os /G5"
    elif grep -qxF "$t" <<<"$D3D_OS_SF_TUS";        then echo "/Os /Qsfalign16"
    elif grep -qxF "$t" <<<"$D3D_QIP_OS_OI_SF_TUS"; then echo "/Os /Oi- /Qip /Qsfalign16"
    elif grep -qxF "$t" <<<"$D3D_QIP_SF_TUS";       then echo "/O2 /Qip /Qsfalign16"
    elif grep -qxF "$t" <<<"$D3D_QIP_NI_SF_TUS";    then echo "/O2 /Qip /Qip_no_inlining /Qsfalign16"
    elif grep -qxF "$t" <<<"$D3D_QIP_OS_OI_NI_TUS"; then echo "/Os /Oi- /Qip /Qip_no_inlining"
    elif grep -qxF "$t" <<<"$D3D_QIP_OS_OI_TUS";    then echo "/Os /Oi- /Qip"
    elif grep -qxF "$t" <<<"$D3D_QIP_OS_NI_TUS";    then echo "/Os /Qip /Qip_no_inlining"
    elif grep -qxF "$t" <<<"$D3D_RCD_TUS";          then echo "/O2 /Qrcd"
    elif grep -qxF "$t" <<<"$D3D_OI_TUS";           then echo "/O2 /Oi-"
    elif grep -qxF "$t" <<<"$D3D_QIP_OI_TUS";       then echo "/O2 /Oi- /Qip /Qip_no_inlining"
    elif grep -qxF "$t" <<<"$D3D_OS_OI_TUS";        then echo "/Os /Oi-"
    elif grep -qxF "$t" <<<"$D3D_QIP_NI_TUS";       then echo "/O2 /Qip /Qip_no_inlining"
    elif grep -qxF "$t" <<<"$D3D_OS_TUS";           then echo "/Os"
    elif grep -qxF "$t" <<<"$D3D_QIP_TUS";          then echo "/O2 /Qip"
    elif grep -qxF "$t" <<<"$D3D_QIP_OS_TUS";       then echo "/Os /Qip"
    else echo "/O2"; fi
}

case "${1:-}" in
    ltmsg) build_ltmsg; [ "${2:-}" = verify ] && verify_ltmsg ;;
    sres)  build_sres ;;
    launcher) build_launcher
              case "${2:-}" in
                  link)   link_launcher ;;
                  verify) link_launcher; verify_launcher ;;
              esac ;;
    server)   export CL_REV="${CL_REV:-8447}"; source "$(dirname "$0")/env.sh"
              build_engine_scaffold server "/I ..\\..\\LT2\\sdk\\inc /I ." server.cpp servermgr.cpp servermsg.cpp netmgr.cpp servermath.cpp commonphysics.cpp commonmodel.cpp sharedcommon.cpp servermodel.cpp serverlightanim.cpp servercommon.cpp servermessage.cpp packet.cpp lthread.cpp smgr.cpp counter.cpp dalloc.cpp modelanim.cpp streamsim.cpp fsstream.cpp basestream.cpp objbank.cpp objtypes.cpp collection.cpp worldbsp.cpp worldmodels.cpp saveio.cpp worldsim.cpp serverengine.cpp ltobject.cpp lmessage.cpp spritecontrol.cpp tcpdriver.cpp stringbank.cpp dsi.cpp genericprop.cpp dhash.cpp filemgr.cpp console.cpp structbank.cpp csbase.cpp sdkforce.cpp netobj.cpp savrest.cpp shade.cpp smgrnew.cpp modelnew.cpp objmover.cpp smgrmisc.cpp svnew.cpp svlast.cpp svtiny.cpp worldtree.cpp
              build_engine_scaffold_os server "/I ..\\..\\LT2\\sdk\\inc /I ." $OS_TUS
              cp -u src/lithshared/rezmgr/Release/*.obj src/engine/server/Release/ 2>/dev/null || true
              [ "${2:-}" = verify ] && match "$(ref server.dll)" src/engine/server/Release/*.obj
              ;;
    # The reconstruction loop: recompile ONE file, match only its object.
    #   build/engine.sh server-one smgr.cpp PlaySound
    server-one)
              export CL_REV="${CL_REV:-8447}"; source "$(dirname "$0")/env.sh"
              TU="${2:?usage: engine.sh server-one <tu.cpp> [symbol-filter]}"
              # a failed compile under Wine can exit 0 and leave the old
              # object in place, so a broken edit would measure as no change
              rm -f "src/engine/server/Release/${TU%.cpp}.obj"
              case " $OS_TUS " in
                  *" $TU "*) build_engine_scaffold_os server "/I ..\\..\\LT2\\sdk\\inc /I ." "$TU" || exit 1 ;;
                  *)         build_engine_scaffold server "/I ..\\..\\LT2\\sdk\\inc /I ." "$TU" || exit 1 ;;
              esac
              match "$(ref server.dll)" "src/engine/server/Release/${TU%.cpp}.obj" -- "${3:-}" ;;
    d3d-link)
              export CL_REV="${CL_REV:-8168}"; source "$(dirname "$0")/env.sh"
              python3 tools/mkaliasobj.py src/engine/d3d/link/aliases.rsp src/engine/d3d/link/aliases.obj
              python3 tools/mkviewobj.py src/engine/d3d/link/d3dviews.txt src/engine/d3d/link/d3dviews.obj
              python3 tools/mkimplib.py src/engine/d3d/link/ddraw7.lib DDRAW.dll _DirectDrawCreateEx@16
              ( cd src/engine/d3d &&
                objs=$(ls Release/*.obj | tr "\n" " ") &&
                LIB="$(wpath "$ICLDIR")\\LIB;$LIB" run_link /nologo /dll /machine:I386 /out:Release/d3d.ren /map:Release/d3d.map \
                    /export:RenderDLLSetup /export:GetSupportedModes /export:FreeModeList \
                    link/aliases.obj link/d3dviews.obj $objs ..\\..\\lithshared\\out\\StdLith.lib \
                    libcmt.lib kernel32.lib user32.lib winmm.lib ddraw.lib link/ddraw7.lib dxguid.lib \
                    > Release/d3d-link.txt 2>&1 || true ) &&
              grep -o "unresolved external symbol .*" src/engine/d3d/Release/d3d-link.txt | sort -u | wc -l | sed 's/^/unresolved externals: /'
              # "d3d-link test": load the linked d3d.ren under Wine with its
              # preferred base occupied (so the loader relocates it), list the
              # display modes and have RenderDLLSetup fill a RenderStruct
              if [ "${2:-}" = test ]; then
                  ren=src/engine/d3d/Release/d3d.ren
                  [ -f "$ren" ] || { echo "no $ren (the link did not complete)"; exit 1; }
                  mkdir -p out/d3dtest
                  cp tools/d3dloadtest.c out/d3dtest/ && cp "$ren" out/d3dtest/d3d.ren
                  ( cd out/d3dtest && run_cl /nologo /O1 d3dloadtest.c \
                        /link /out:d3dloadtest.exe kernel32.lib user32.lib >/dev/null &&
                    "$WINE" d3dloadtest.exe d3d.ren rebase 2>/dev/null ) || exit 1
              fi ;;
    lithtech-link)
              export CL_REV="${CL_REV:-8447}"; source "$(dirname "$0")/env.sh"
              python3 tools/mkaliasobj.py src/engine/lithtech/link/aliases.rsp src/engine/lithtech/link/aliases.obj
              # the modern build's ILTClient has slots the retail one lacks
              # (vtables.txt's %ifdef NOLFSRC_MODERN block); empty otherwise
              vtabdefs=""
              case " ${NOLFSRC_CFLAGS:-} " in *NOLFSRC_MODERN*) vtabdefs="--define NOLFSRC_MODERN" ;; esac
              python3 tools/mkvtabobj.py $vtabdefs src/engine/lithtech/link/vtables.txt src/engine/lithtech/link/vtables.obj
              mss32_implib || exit 1
              python3 tools/mkimplib.py --keep-decoration src/engine/lithtech/link/mss32dec.lib mss32.dll \
                  $(sed -n "s/^ *\(AIL_[A-Za-z0-9_]*@[0-9]*\) *$/_\1/p" src/engine/launcher/libs/mss32.def)
              ( cd src/engine/lithtech &&
                objs=$(ls Release/*.obj | grep -v -E "/(rezhash|rezdir|rezemu|rezfile|rezmgr|rezmgr2)\.obj" | tr "\n" " ") &&
                run_link /nologo /subsystem:windows /machine:I386 /out:Release/lithtech.exe /map:Release/lithtech.map \
                    link/vtables.obj link/aliases.obj $objs \
                    ..\\..\\lithshared\\out\\RezMgr.lib ..\\..\\lithshared\\out\\StdLith.lib ..\\..\\lithshared\\out\\Lith.lib \
                    link\\mss32dec.lib ..\\launcher\\Release\\mss32.lib \
                    libcmt.lib kernel32.lib user32.lib gdi32.lib winmm.lib ole32.lib wsock32.lib dinput.lib dxguid.lib uuid.lib \
                    > Release/lithtech-link.txt 2>&1 || true ) &&
              grep -o "unresolved external symbol .*" src/engine/lithtech/Release/lithtech-link.txt | sort -u | wc -l | sed 's/^/unresolved externals: /' ;;
    server-link)
              python3 tools/mkaliasobj.py src/engine/server/link/aliases.rsp src/engine/server/link/aliases.obj
              ( cd src/engine/server &&
                objs=$(ls Release/*.obj | grep -v -E "/(rezhash|rezdir|rezemu|rezfile|rezmgr|rezmgr2)\.obj" | tr "\n" " ") &&
                run_link /nologo /dll /machine:I386 /out:Release/server.dll /map:Release/server.map \
                    /export:CreateServer /export:DeleteServer link/aliases.obj $objs \
                    ..\\..\\lithshared\\out\\RezMgr.lib ..\\..\\lithshared\\out\\StdLith.lib ..\\..\\lithshared\\out\\Lith.lib \
                    libcmt.lib kernel32.lib user32.lib winmm.lib ole32.lib wsock32.lib > Release/server-link.txt 2>&1 || true ) &&
              grep -o "unresolved external symbol .*" src/engine/server/Release/server-link.txt | sort -u | wc -l | sed 's/^/unresolved externals: /'
              # "server-link test": load the linked DLL under Wine with its
              # preferred base occupied (so the loader relocates it), then
              # CreateServer and DeleteServer (needs `engine.sh ltmsg` first)
              if [ "${2:-}" = test ]; then
                  mkdir -p out/linktest
                  cp tools/loadtest.c src/engine/server/Release/server.dll out/linktest/
                  cp src/engine/ltmsg/Release/ltmsg.dll out/linktest/ || exit 1
                  ( cd out/linktest && run_cl /nologo /O1 loadtest.c \
                        /link /out:loadtest.exe kernel32.lib user32.lib ole32.lib >/dev/null &&
                    "$WINE" loadtest.exe server.dll rebase 2>/dev/null ) || exit 1
              fi ;;
    d3d)
              export CL_REV="${CL_REV:-8168}"; source "$(dirname "$0")/env.sh"
              D=src/engine/d3d
              echo "==== $D -> objects (Intel C/C++ 4.0, per-file flag groups)"
              mkdir -p "$D/Release"
              ( cd "$D" && run_icl /nologo /MT /W3 /O2 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $(ls *.cpp | grep -vxF "$D3D_OS_TUS" | grep -vxF "$D3D_QIP_TUS" | grep -vxF "$D3D_QIP_OS_TUS" | grep -vxF "$D3D_OI_TUS" | grep -vxF "$D3D_QIP_OI_TUS" | grep -vxF "$D3D_RCD_TUS" | grep -vxF "$D3D_OS_OI_TUS" | grep -vxF "$D3D_QIP_NI_TUS" | grep -vxF "$D3D_QIP_OS_OI_TUS" | grep -vxF "$D3D_QIP_OS_NI_TUS" | grep -vxF "$D3D_OS_G5_TUS" | grep -vxF "$D3D_OS_SF_TUS" | grep -vxF "$D3D_QIP_OS_OI_SF_TUS" | grep -vxF "$D3D_QIP_SF_TUS" | grep -vxF "$D3D_QIP_NI_SF_TUS" | grep -vxF "$D3D_QIP_OS_OI_NI_TUS" | grep -vxF "$D3D_CL_TUS") )
              [ -n "$D3D_RCD_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Qrcd /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_RCD_TUS )
              [ -n "$D3D_OI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Oi- /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_OI_TUS )
              [ -n "$D3D_QIP_OI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Oi- /Qip /Qip_no_inlining /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OI_TUS )
              [ -n "$D3D_OS_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_OS_TUS )
              [ -n "$D3D_OS_OI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Oi- /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_OS_OI_TUS )
              [ -n "$D3D_QIP_NI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Qip /Qip_no_inlining /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_NI_TUS )
              [ -n "$D3D_QIP_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Qip /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_TUS )
              [ -n "$D3D_QIP_OS_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Qip /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OS_TUS )
              [ -n "$D3D_QIP_OS_OI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Oi- /Qip /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OS_OI_TUS )
              [ -n "$D3D_QIP_OS_NI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Qip /Qip_no_inlining /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OS_NI_TUS )
              [ -n "$D3D_OS_G5_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /G5 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_OS_G5_TUS )
              [ -n "$D3D_OS_SF_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Qsfalign16 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_OS_SF_TUS )
              [ -n "$D3D_QIP_OS_OI_SF_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Oi- /Qip /Qsfalign16 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OS_OI_SF_TUS )
              [ -n "$D3D_QIP_SF_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Qip /Qsfalign16 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_SF_TUS )
              [ -n "$D3D_QIP_NI_SF_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /O2 /Qip /Qip_no_inlining /Qsfalign16 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_NI_SF_TUS )
              [ -n "$D3D_QIP_OS_OI_NI_TUS" ] && ( cd "$D" && run_icl /nologo /MT /W3 /Os /Oi- /Qip /Qip_no_inlining /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ $D3D_QIP_OS_OI_NI_TUS )
              d3d_cl $D3D_CL_TUS
              [ "${2:-}" = verify ] && match "$(ref d3d.ren)" src/engine/d3d/Release/*.obj
              ;;
    # One file, with the flags its group gives it.
    #   build/engine.sh d3d-one d3d.cpp RenderDLLSetup
    d3d-one)
              export CL_REV="${CL_REV:-8168}"; source "$(dirname "$0")/env.sh"
              TU="${2:?usage: engine.sh d3d-one <tu.cpp> [symbol-filter]}"
              rm -f "src/engine/d3d/Release/${TU%.cpp}.obj"
              if grep -qxF "$TU" <<<"$D3D_CL_TUS"; then
                  echo "==== $TU (cl 8447 $D3D_CL_FLAGS)"
                  d3d_cl "$TU" || exit 1
              else
                  FLAGS=$(d3d_flags "$TU"); echo "==== $TU ($FLAGS)"
                  ( cd src/engine/d3d && run_icl /nologo /MT /W3 $FLAGS /D WIN32 /D NDEBUG /D _WINDOWS \
                      /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I . /c /FoRelease/ "$TU" ) || exit 1
              fi
              match "$(ref d3d.ren)" "src/engine/d3d/Release/${TU%.cpp}.obj" -- "${3:-}" ;;
    lithtech) export CL_REV="${CL_REV:-8447}"; source "$(dirname "$0")/env.sh"
              D=src/engine/lithtech
              echo "==== $D -> objects (cl 8447 /O2; shared engine sources)"
              mkdir -p "$D/Release"
              ( cd "$D" && run_cl /nologo /MT /W3 /O2 /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I ..\\server /I . /c /FoRelease/ \
                  lithtech.cpp ..\\server\\msgterm.cpp ..\\server\\sdkforce.cpp ..\\server\\netobj.cpp ..\\server\\smgrnew.cpp ..\\server\\modelnew.cpp ..\\server\\objmover.cpp ..\\server\\smgrmisc.cpp ..\\server\\shade.cpp wmrotate.cpp collelem.cpp cchandler.cpp cmodelload.cpp dmextra.cpp csvis.cpp diblit.cpp savobj.cpp conline.cpp nettrace.cpp modchunk.cpp partfm.cpp ccobj.cpp demodtx.cpp cissurf.cpp cltapi.cpp sndinst2.cpp sndmgr2.cpp predtick.cpp cmgrinit.cpp wgrid.cpp dmgrow.cpp cmgsurf.cpp warpblit.cpp startload.cpp fntdraw.cpp predobj.cpp fmcvt32.cpp fmconvert.cpp ccclient.cpp clientshell.cpp ccline.cpp binkvideo.cpp smackvideo.cpp lvideomgr.cpp soundinst2.cpp tcpqarray.cpp clientmgr3.cpp clientwin.cpp worldtree2.cpp transform.cpp clientinterface.cpp clientphysics.cpp clientsound.cpp dmloader.cpp clientmgr2.cpp stdlithalloc.cpp clientsoundinst.cpp clientworld.cpp cltcursor.cpp clientconsole.cpp clientdsi.cpp lttexmod.cpp clienttexmgr.cpp clientstreams.cpp clientinput.cpp clientsoundmgr.cpp clientworldinfo.cpp clientblit.cpp clientrender.cpp ltnew1.cpp cltnew.cpp cltrest.cpp lt2new1.cpp lt2new2.cpp lt2new3.cpp lt2new4.cpp ltnew2.cpp ..\\server\\csbase.cpp ..\\server\\servermgr.cpp ..\\server\\servermsg.cpp ..\\server\\netmgr.cpp ..\\server\\servermath.cpp ..\\server\\commonphysics.cpp ..\\server\\commonmodel.cpp ..\\server\\sharedcommon.cpp ..\\server\\servermodel.cpp ..\\server\\serverlightanim.cpp ..\\server\\servercommon.cpp ..\\server\\servermessage.cpp ..\\server\\packet.cpp ..\\server\\lthread.cpp ..\\server\\smgr.cpp ..\\server\\ltobject.cpp ..\\server\\lmessage.cpp ..\\server\\spritecontrol.cpp ..\\server\\server.cpp ..\\server\\stringbank.cpp ..\\server\\dsi.cpp ..\\server\\genericprop.cpp ..\\server\\dhash.cpp ..\\server\\filemgr.cpp ..\\server\\console.cpp ..\\server\\structbank.cpp ..\\server\\counter.cpp ..\\server\\dalloc.cpp ..\\server\\modelanim.cpp ..\\server\\basestream.cpp ..\\server\\collection.cpp ..\\server\\fsstream.cpp ..\\server\\objbank.cpp ..\\server\\objtypes.cpp ..\\server\\saveio.cpp ..\\server\\serverengine.cpp ..\\server\\streamsim.cpp ..\\server\\tcpdriver.cpp ..\\server\\worldbsp.cpp ..\\server\\worldmodels.cpp ..\\server\\worldsim.cpp worldtree.cpp )
              ( cd "$D" && run_cl /nologo /MT /W3 $NOPOOL_FLAGS /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I ..\\server /I . /c /FoRelease/ $NOPOOL_TUS )
              ( cd "$D" && run_cl /nologo /MT /W3 $OD_FLAGS /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I ..\\server /I . /c /FoRelease/ $OD_TUS )
              ( cd "$D" && run_cl /nologo /MT /W3 $GX_FLAGS /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I ..\\server /I . /c /FoRelease/ $GX_TUS )
              cp -u src/lithshared/rezmgr/Release/*.obj src/engine/lithtech/Release/ 2>/dev/null || true
              [ "${2:-}" = verify ] && match "$(ref lithtech.exe)" src/engine/lithtech/Release/*.obj
              ;;
    # One file (a bare name not in src/engine/lithtech resolves against
    # src/engine/server), with the flags its group gives it.
    #   build/engine.sh lithtech-one smgr.cpp PlaySound
    lithtech-one)
              export CL_REV="${CL_REV:-8447}"; source "$(dirname "$0")/env.sh"
              TU="${2:?usage: engine.sh lithtech-one <tu.cpp> [symbol-filter]}"
              SRC="$TU"; [ -f "src/engine/lithtech/$TU" ] || SRC="..\\server\\$TU"
              rm -f "src/engine/lithtech/Release/${TU%.cpp}.obj"   # see server-one
              case " $NOPOOL_TUS " in
                  *" $TU "*) FLAGS="$NOPOOL_FLAGS" ;;
                  *)         case " $OD_TUS " in
                                 *"\\$TU "*) FLAGS="$OD_FLAGS" ;;
                                 *)           case " $GX_TUS " in
                                                  *" $TU "*) FLAGS="$GX_FLAGS" ;;
                                                  *)         FLAGS="/O2" ;;
                                              esac ;;
                             esac ;;
              esac
              ( cd src/engine/lithtech && run_cl /nologo /MT /W3 $FLAGS /D WIN32 /D NDEBUG /D _WINDOWS \
                  /D NO_PRAGMA_LIBS /I ..\\..\\LT2\\sdk\\inc /I ..\\server /I . /c /FoRelease/ "$SRC" ) || exit 1
              match "$(ref lithtech.exe)" "src/engine/lithtech/Release/${TU%.cpp}.obj" -- "${3:-}" ;;
    *) echo "usage: $0 ltmsg [verify] | sres | launcher [link|verify]"
       echo "       $0 server|lithtech|d3d [verify]"
       echo "       $0 server-link [test] | lithtech-link | d3d-link [test]"
       echo "       $0 server-one|lithtech-one|d3d-one <file.cpp> [symbol]"; exit 1 ;;
esac
echo "==== done"
