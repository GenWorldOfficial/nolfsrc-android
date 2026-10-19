# native/ -- the game without Win32

The native build: the same sources as the Windows build, compiled with clang
for Linux now and for Android next (`docs/ANDROID.md` has the plan; this is
its milestone 1). The Windows build is untouched by it.

## Building

Needs clang, CMake, Ninja, Python 3, flex and bison, and the `tools/`
submodule (for the `.dsp` source lists). For the 32-bit build, which keeps
every struct at its retail size, also the i386 C/C++ libraries:

    sudo dpkg --add-architecture i386 && sudo apt update
    sudo apt install clang cmake ninja-build python3 flex bison gcc-multilib g++-multilib libsdl2-dev:i386
    git submodule update --init tools
    CC=clang CXX=clang++ cmake -S native -B out/native -G Ninja
    ninja -C out/native

## How the source compiles unchanged

It was written for Visual C++ 6.0 on Windows. What clang needs instead:

| VC6 / Windows | Here |
|---|---|
| `#include "StdAfx.h"` for `stdafx.h`, `"..\Shared\NetDefs.h"` | `tools/mkoverlay.py` writes a case-insensitive clang VFS overlay of `src/` with every backslashed include mapped to its file (`-ivfsoverlay`), so no include is edited (their spelling reaches `__FILE__`) |
| `windows.h`, `io.h`, `winsock.h`, `mmsystem.h`, `dinput.h` (key codes), `fstream.h`, ... | stand-ins in `include/`: Windows' types at Windows' sizes, its constants, and declarations of what the platform layer implements |
| `stricmp`, `_MAX_PATH`, `_splitpath`, `_findfirst` from `stdlib.h`/`string.h`/`io.h` | `include/nolfcrt.h`, included first in every compile |
| `##` pastes that form no token; `&LTVector(...)` arguments; missing `typename` | clang's own switches: `-Wno-invalid-token-paste`, `-Wno-address-of-temporary`, `-fms-compatibility`, `-fdelayed-template-parsing` |
| `or` and `export` as names | `-fno-operator-names`, `-Dexport=nolf_export` |
| a `for` variable used after its loop | `tools/forscope.py` moved those declarations in front of their loops, on the same line (what VC6 compiled already) |
| default arguments on function pointers, a `CString` through `...`, a temporary bound to a non-const reference | `NOLF_DEFARG`, `NOLF_CSTR`, `NOLF_LVALUE` in the source: each is the original code to VC (`ltbasedefs.h`) and the standard form here (`nolfcrt.h`) |
| implicit `int`, `CBase::CBase();` as a statement, x86 `__asm` | a few `#ifdef NOLF_NATIVE` blocks, or an explicit `int` |
| Winsock's `in_addr` (`sin_addr.S_un.S_un_b.s_b1`) | `include/nolfsock.h` declares Winsock's own structs (the same bytes as the system's) and never includes `<netinet/in.h>` |
| DirectInput, COM's `IUnknown` | `include/dinput.h`, `include/nolfcom.h`: DirectInput 5's interfaces, to be implemented over SDL |
| the engine's 16-bit wide strings (`wcslen` on `uint16`) | `include/nolfwchar16.h` renames them to 16-bit versions |
| `__stdcall`, `__cdecl`, `__fastcall` | one convention for everything (`-D__stdcall=` ...) |

Every edit to `src/` keeps the Windows build's code as it was. Edits keep
their lines where `__LINE__` could reach a release binary.

## State

| Part | State |
|---|---|
| lithshared libraries (StdLith, Lith, RezMgr, RegMgr, CryptMgr, MFCStub, ButeMgr, LithFontMgr, ltguimgr, GameSpy, zlib) | compile |
| CShell (194 sources + `src/modern/cshell`) and Object (175 + the flex/bison parser + `src/modern/object`) | compile |
| the engine: `lithtech.exe`'s 125 sources, `src/engine/server`'s it compiles in, the modern gamepad device | compile |
| linking: the reconstruction's link-time aliases (`link/aliases.rsp`) and stand-in vtables (`link/vtables.txt`), reproduced for the ELF linker | next |
| the platform layer (`plat/`: what `include/` declares, on SDL2 and POSIX) | after that |
| running | after that |
