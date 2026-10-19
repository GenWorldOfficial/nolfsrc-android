# native/CMakeLists.txt -- the game built for Linux and Android, without Win32.
#
# The Windows build (the Makefile at the root, through Wine and Visual C++
# 6.0) is untouched: this builds the same sources with clang, the modern
# build's way (NOLFSRC_MODERN), against native/include/ (stand-ins for the
# Windows headers) and native/plat/ (what those declare: the platform layer
# on SDL2). docs/ANDROID.md has the plan this is milestone 1 of.
#
#   cmake -S native -B out/native -G Ninja -DNOLF_ARCH=x86   (32-bit, the default)
#   cmake --build out/native
#
# The source compiles unchanged: it includes headers in any case and with
# backslashes, which a clang overlay (tools/mkoverlay.py) maps onto the real
# files, so nothing the Windows build compiles has to be edited for this one.

cmake_minimum_required(VERSION 3.16)
project(nolf_native C CXX)

set(NOLF_ARCH "x86" CACHE STRING "x86 (32-bit, retail layouts) or native (the host's own)")
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
get_filename_component(SRC ${SRC} ABSOLUTE)
set(NATIVE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "native/ needs clang (the include overlay is clang's): -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- the include overlay -----------------------------------------------------
set(OVERLAY ${CMAKE_BINARY_DIR}/overlay.yaml)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${NATIVE}/tools/mkoverlay.py ${OVERLAY} ${SRC} ${NATIVE}/include
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "mkoverlay.py failed")
endif()

# ---- flags every target shares --------------------------------------------------
# Visual C++ 6.0's C++: Microsoft extensions, templates checked only when
# used, C++98. No _WIN32/WIN32/_WINDOWS: code that tests them takes its
# non-Windows branch where it has one; the rest gets native/include/.
set(NOLF_COMMON
    -fms-extensions -fms-compatibility -fdelayed-template-parsing
    # names VC6 allowed that are C++ keywords: 'or' (zNFA's token), 'export'
    # (a zNFA method)
    -fno-operator-names -Dexport=nolf_export
    # VC's token pasting: '##' that forms no single token pastes nothing
    -Wno-invalid-token-paste
    # VC's address of a temporary (&LTVector(0, 0, 0) as an argument): it
    # lives to the end of the call, as in VC
    -Wno-address-of-temporary
    -fno-strict-aliasing -fwrapv -fsigned-char -funsigned-bitfields
    -ivfsoverlay ${OVERLAY}
    # what Microsoft's stdlib.h and string.h declare (stricmp, _MAX_PATH, ...)
    -include ${NATIVE}/include/nolfcrt.h
    -w)
if(NOLF_ARCH STREQUAL "x86")
    list(APPEND NOLF_COMMON -m32 -msse2 -mfpmath=sse)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
endif()
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_EXTENSIONS ON)
add_compile_options(${NOLF_COMMON})
add_compile_definitions(NOLF_NATIVE NDEBUG NO_PRAGMA_LIBS NOLFSRC_MODERN)
include_directories(BEFORE SYSTEM ${NATIVE}/include)

set(LT2LS ${SRC}/LT2/lithshared)
set(LS ${SRC}/lithshared)

# ---- 1. the lithshared libraries (build.mk section 1) --------------------------------
function(nolf_lib name)
    cmake_parse_arguments(A "" "" "SOURCES;INCLUDES;DEFINES" ${ARGN})
    add_library(${name} STATIC ${A_SOURCES})
    target_include_directories(${name} PRIVATE ${A_INCLUDES})
    target_compile_definitions(${name} PRIVATE ${A_DEFINES})
endfunction()

nolf_lib(nolf_zlib
    SOURCES ${LS}/zlib/adler32.c ${LS}/zlib/compress.c ${LS}/zlib/crc32.c ${LS}/zlib/deflate.c
            ${LS}/zlib/gzio.c ${LS}/zlib/infblock.c ${LS}/zlib/infcodes.c ${LS}/zlib/inffast.c
            ${LS}/zlib/inflate.c ${LS}/zlib/inftrees.c ${LS}/zlib/infutil.c ${LS}/zlib/trees.c
            ${LS}/zlib/uncompr.c ${LS}/zlib/zutil.c
    INCLUDES ${LS}/zlib
    DEFINES STDC)

nolf_lib(nolf_stdlith
    SOURCES ${LS}/stdlith/abstractio.cpp ${LS}/stdlith/dynarray.cpp ${LS}/stdlith/fileio.cpp
            ${LS}/stdlith/goodlinklist.cpp ${LS}/stdlith/helpers.cpp ${LS}/stdlith/l_allocator.cpp
            ${LS}/stdlith/linklist.cpp ${LS}/stdlith/lithexception.cpp ${LS}/stdlith/memory.cpp
            ${LS}/stdlith/memoryio.cpp ${LS}/stdlith/stringholder.cpp ${LS}/stdlith/struct_bank.cpp
            ${LS}/stdlith/sys/win/compressedio.cpp
    INCLUDES ${LT2LS}/stdlith ${LT2LS}/lith ${LS}/stdlith ${LS}/stdlith/sys/win ${LS}/zlib)

nolf_lib(nolf_cryptmgr
    SOURCES ${LS}/CryptMgr/cryptmgr.cpp ${LS}/CryptMgr/blowfish.cpp
    INCLUDES ${LT2LS}/cryptmgr ${LT2LS}/incs ${LS}/CryptMgr)

nolf_lib(nolf_lith
    SOURCES ${LS}/lith/basehash.cpp ${LS}/lith/baselist.cpp ${LS}/lith/virtlist.cpp
    INCLUDES ${LS}/lith)

nolf_lib(nolf_rezmgr
    SOURCES ${LS}/rezmgr/rezmgr.cpp ${LS}/rezmgr/rezmgr2.cpp ${LS}/rezmgr/rezfile.cpp
            ${LS}/rezmgr/rezemu.cpp ${LS}/rezmgr/rezdir.cpp ${LS}/rezmgr/rezhash.cpp
    INCLUDES ${LT2LS}/incs ${LS}/rezmgr)

nolf_lib(nolf_regmgr
    SOURCES ${LS}/RegMgr/regmgr.cpp
    INCLUDES ${LT2LS}/incs ${LS}/RegMgr)

nolf_lib(nolf_mfcstub
    SOURCES ${LS}/MFCStub/mfcs_string.cpp ${LS}/MFCStub/mfcs_rect.cpp ${LS}/MFCStub/mfcs_point.cpp
    INCLUDES ${LT2LS}/mfcstub ${LT2LS}/incs ${LT2LS}/lith ${LS}/MFCStub)

file(GLOB LTGUIMGR_SRC ${LS}/ltguimgr/*.cpp)
nolf_lib(nolf_ltguimgr
    SOURCES ${LTGUIMGR_SRC}
    INCLUDES ${LT2LS}/ltguimgr ${LT2LS}/incs ${LT2LS}/mfcstub ${LT2LS}/lith ${SRC}/LT2/sdk/inc ${LS}/ltguimgr
    DEFINES _NOMFC _LITHTECH2)

file(GLOB LITHFONTMGR_SRC ${LS}/lithfontmgr/*.cpp)
nolf_lib(nolf_lithfontmgr
    SOURCES ${LITHFONTMGR_SRC}
    INCLUDES ${LT2LS}/lithfontmgr ${LT2LS}/incs ${SRC}/LT2/sdk/inc ${LS}/lithfontmgr
    DEFINES _LITHTECH2)

file(GLOB ZTOOLS_SRC ${LS}/ztools/*.cpp)
nolf_lib(nolf_butemgr
    SOURCES ${ZTOOLS_SRC} ${LS}/ButeMgr/butemgr.cpp
    INCLUDES ${LT2LS}/butemgr ${LT2LS}/mfcstub ${LT2LS}/incs ${LT2LS}/lith ${LT2LS}/cryptmgr ${LS}/ztools ${LS}/ButeMgr
    DEFINES _NOMFC)

nolf_lib(nolf_gamespy
    SOURCES ${LS}/gamespy/GameSpyMgr.cpp ${LS}/gamespy/GameSpyClientMgr.cpp
    INCLUDES ${LT2LS}/incs ${LT2LS}/mfcstub ${LS}/gamespy)

# ---- 2. the game modules (build.mk section 2) --------------------------------------------
# Source lists from the .dsp project files, as the Windows build takes them,
# plus the modern build's own code in src/modern/<cshell|object>/.
function(nolf_dsp out dsp)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${NATIVE}/tools/dspfiles.py ${SRC}/../tools/dsp2files.py ${dsp} Release cpp
        OUTPUT_VARIABLE files OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "dspfiles.py ${dsp} failed (is the tools/ submodule checked out?)")
    endif()
    set(${out} ${files} PARENT_SCOPE)
endfunction()

set(GAME_DEFS _REZFILE _FINAL _NOLFBUILD)
set(GAME_INCS ${SRC}/LT2/sdk/inc ${LT2LS}/incs ${SRC}/modern)
# game/Shared for "..." includes only: it holds an empty unistd.h (for flex's
# output on Windows) that <unistd.h> must not find
set(GAME_IQUOTE -iquote ${SRC}/game/Shared)

set(CSHELL ${SRC}/game/ClientShellDLL)
nolf_dsp(CSHELL_SRC ${CSHELL}/ClientShellDLL.dsp)
file(GLOB CSHELL_MODERN ${SRC}/modern/cshell/*.cpp)
add_library(nolf_cshell STATIC ${CSHELL_SRC} ${SRC}/game/Shared/Stdafx.cpp ${CSHELL_MODERN})
list(REMOVE_DUPLICATES CSHELL_SRC)
target_include_directories(nolf_cshell PRIVATE ${CSHELL} ${GAME_INCS})
target_compile_definitions(nolf_cshell PRIVATE ${GAME_DEFS} _CLIENTBUILD)
target_compile_options(nolf_cshell PRIVATE ${GAME_IQUOTE})

# Object: the animation lexer and parser come from flex and bison, as the
# .dsp's custom build steps make them.
set(OBJDLL ${SRC}/game/ObjectDLL)
nolf_dsp(OBJDLL_SRC ${OBJDLL}/Object.dsp)
list(FILTER OBJDLL_SRC EXCLUDE REGEX "/(AnimationLex|AnimationParse)\\.cpp$")
find_program(FLEX flex REQUIRED)
find_program(BISON bison REQUIRED)
set(GEN ${CMAKE_BINARY_DIR}/gen/object)
add_custom_command(
    OUTPUT ${GEN}/AnimationLex.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}
    COMMAND ${FLEX} -Pyyanimation -o${GEN}/AnimationLex.cpp ${OBJDLL}/AnimationLex.l
    DEPENDS ${OBJDLL}/AnimationLex.l)
add_custom_command(
    OUTPUT ${GEN}/AnimationParse.cpp ${GEN}/AnimationLexSymbols.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}
    COMMAND ${BISON} --defines=${GEN}/AnimationLexSymbols.h -pyyanimation -o${GEN}/AnimationParse.cpp ${OBJDLL}/AnimationParse.y
    DEPENDS ${OBJDLL}/AnimationParse.y)
file(GLOB OBJECT_MODERN ${SRC}/modern/object/*.cpp)
add_library(nolf_object STATIC ${OBJDLL_SRC} ${SRC}/game/Shared/Stdafx.cpp ${OBJECT_MODERN}
    ${GEN}/AnimationLex.cpp ${GEN}/AnimationParse.cpp)
target_include_directories(nolf_object PRIVATE ${GEN} ${OBJDLL} ${GAME_INCS})
target_compile_definitions(nolf_object PRIVATE ${GAME_DEFS})
target_compile_options(nolf_object PRIVATE ${GAME_IQUOTE})
