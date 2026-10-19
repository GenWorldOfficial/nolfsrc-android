# Android port

A native build of the game for Android 13 and up, on 64-bit ARM
(arm64-v8a), aimed first at Snapdragon phones with Adreno GPUs. This file
is the plan and its state. The port is not running yet: the first piece,
the OpenGL ES renderer backend, is written and tested on its own.

## Why native is hard here

The engine in `src/engine/` is a reconstruction written to recompile,
byte for byte, to the retail x86 binaries of 2000. That makes it
faithful, and it makes it a Win32 program through and through. Measured
over `src/engine/lithtech`, `src/engine/server`, `src/lithshared`, `src/LT2`
and `src/game` (the old Direct3D 7 renderer, `src/engine/d3d`, is left out:
Android uses the modern renderer):

| What | Where | Size of it |
|---|---|---|
| Win32 headers and handles | 51 files include `windows.h`; 33 use `HWND`/`HINSTANCE`/`WNDPROC` | the window, the message loop, the launcher hand-off (`clientwin.cpp`, 2,400 lines) |
| DirectInput | 4 engine files (`clientinput.cpp`, 3,200 lines) | keyboard, mouse, joystick |
| Key state read by the game | `GetAsyncKeyState` (`IsKeyDown`) and `VK_` key codes in 26 `ClientShellDLL` files | needs an engine-side key table |
| DirectMusic | 6 engine files (`clientmusic.cpp`, `dmloader.cpp`, `dmgrow.cpp`, `dmextra.cpp`, ...) | the game's adaptive music |
| Winsock | `tcpdriver.cpp`, `clientstreams.cpp`, `tcpqarray.cpp`, GameSpy | maps to BSD sockets almost call for call |
| Threads, timers, registry | 16, 15 and 7 files | pthreads, `clock_gettime`, a config file |
| DLL loading | `LoadLibrary`/`GetProcAddress` in 13 files: the renderer, `CShell.dll`, `Object.lto`, `ltmsg.dll`, `CRes.dll`, Bink, Smacker, music | one library with the modules linked in, or `dlopen` |
| Calling conventions | `__declspec`/`__stdcall`/`__cdecl` in 66 files | macros |
| Inline x86 assembly | 2 files outside the Direct3D 7 renderer | rewrite in C |
| **32-bit layouts** | **1,637 padding fields** in the reconstructed structs; **about 140 reads at raw byte offsets** (`*(uint32*)((uint8*)p + 0x4c)`); about 110 pointer-to-integer casts in engine and game; the renderer core's mirrors of engine structs (`ltrender.h`, `ltworld.h`) assert their 32-bit sizes | the largest single job (below) |

Android phones are 64-bit. Recent Snapdragon chips (8 Gen 3 and later)
cannot run 32-bit code at all, so a 32-bit ARM build is not a way round
the layout work, only a stepping stone on older phones.

## The plan

Each milestone is runnable and testable before the next starts. The order
puts the platform work and the 64-bit work in different milestones, so a
crash is never ambiguous about which caused it.

### M1. Native Linux, 32-bit x86

The engine, `CShell` and `Object` built with clang `-m32` from CMake, with
no Win32 underneath: every struct keeps its retail layout, so only the
platform changes.

- **Platform layer on SDL2.** SDL2 replaces the window, message loop and
  input (`clientwin.cpp`, `clientinput.cpp`, DirectInput), and later gives
  the Android activity glue, touch, game controllers and the audio device
  for free. A small `nolfplat` library behind the engine's existing seams
  (the Mac release's engine shows where they are, as it did for the
  renderer) holds SDL, sockets, threads, timers, file finding and the
  config file that stands in for the registry.
- **Files.** Case-insensitive lookup for loose files (Android and Linux
  file systems are case-sensitive; the game asks for `Interface\Menu\...`
  in any case) and `\` to `/` (about 300 backslashed paths in the game
  code). The `.rez` archives already look up without case.
- **Modules.** `CShell` and `Object` linked into the program, the engine's
  `LoadLibrary`/`GetProcAddress` calls answered from a table; `CRes`'s
  strings compiled into a string table.
- **Renderer.** The modern renderer as a library, on `opengl.ren`'s GL
  backend or this port's GLES one (both run on Linux), `mr_window.cpp`
  replaced by the SDL window.
- **Sound.** OpenMiles (already a submodule, on miniaudio) built
  natively in place of `mss32.dll`. **Music** starts as silence (see M5).
- **Movies.** The modern build's FFmpeg Bink player (`src/modern/binkw32`)
  built natively.

Done when: `make test`'s two levels (`T01S01`, `M03S01`) play through natively on Linux.

### M2. Native Linux, 64-bit

The layout work, on a desktop where it can be debugged.

- **One set of struct definitions.** The renderer core reads the engine's
  objects through its own mirrors (`ltworld.h`: `LTObject`, `WorldPoly`,
  `MainWorld`, ...). In a native build the engine and the renderer are one
  program: both include the same headers, and the size asserts apply only
  to the 32-bit Windows build (`#if UINTPTR_MAX == 0xffffffff`).
- **Raw offsets to named fields.** Every `*(T*)((uint8*)p + 0xNN)` becomes
  a field of the right struct; a padding field that is really a pointer
  becomes a pointer. The `progress/` tooling that checks the reconstruction
  against retail still runs on the 32-bit build, so the Windows build stays
  byte-identical where it is today.
- **Pointer casts.** Handles stored in `uint32`/`DWORD` become
  `uintptr_t` or real pointers. clang's `-Wpointer-to-int-cast` and
  `-Wshorten-64-to-32` list them.
- **What crosses a file or the network.** Save games and network messages
  that write structs whole must keep the 32-bit shape on disk and on the
  wire, so saves move between PC and phone and multiplayer works against PC
  servers. Each one gets an explicit read/write.
- Sanitizers (ASan, UBSan) on every playtest run.

Done when: all 60 single-player worlds load and the two playtest levels
play through in the 64-bit build, under ASan.

### M3. Android, arm64

- **App.** SDL2's Android project (Gradle, `SDLActivity`), one native
  library `libmain.so`, landscape, immersive full screen, `minSdk 33`.
- **Renderer.** `gles.ren`'s backend (done, below), with the window SDL
  gives as its native window.
- **Game data.** The player supplies it: the `rez/` installer run on a PC
  produces the game directory, which is copied to the app's storage (or
  picked with the storage access framework on first start). The app ships
  no game data.
- **Lifecycle.** Pausing drops the EGL surface: the engine already reloads
  the renderer on focus changes (the core recreates its textures when
  `nContextGen` moves), so pause and resume map onto that.
- **Controls.** Game controllers through SDL and the modern build's P7
  controller code. A touch overlay: move stick, look area, fire, jump,
  crouch, use, reload, weapon and gadget wheels.

Done when: the first mission plays start to finish on a Snapdragon 8-series
phone.

### M4. Adreno performance

- **Batching.** The core draws through immediate mode, so the backend now
  issues one draw per `rd_Begin`/`rd_End`. Batching consecutive primitives
  that share state cuts draw calls, which cost more on a phone driver than
  on a desktop.
- **Textures.** Adreno has no S3TC, so the core decodes DXT textures to
  32-bit, four to eight times their size in memory. Transcode them to
  ETC2 or ASTC at load (or once, into a cache) instead.
- **Resolution.** The engine's display mode is already an internal
  resolution that the backend scales to the screen; the mode list offers
  1/2, 2/3 and 3/4 of the screen.

### M5. Music

The game's music is DirectMusic (styles, bands and segments that change
with the action), and no open-source player plays it correctly. Two ways:

1. **Pre-rendered** (first): render each mood of each music set to Ogg on
   a PC with Microsoft's DirectMusic (the modern build already carries it
   for Wine), and switch between them on the game's mood changes with
   crossfades. Loses DirectMusic's composed transitions.
2. **A DirectMusic player**: segments, styles, patterns, bands and the DLS
   instruments, on TinySoundFont (already in OpenMiles). Faithful, and a
   large project of its own.

## State

| Piece | State |
|---|---|
| OpenGL ES 3 backend for the renderer core (`src/modern/render/gles/gles_device.cpp`) | **done, tested off-device**: all of `rdev.h`. Fixed-function state kept by the backend and drawn with generated shaders, one per combination in use (up to four texture units with modulate/replace/combine, alpha test, linear fog); BGRA/RGBA/RGB/5:6:5 uploads converted to what GLES takes; push/pop of state; matrix stacks; multisampled antialiasing; screen reads from the back or the last presented frame. Depth clamping with `EXT_depth_clamp`, or in the shader without it; `rd_NearDepth` in the vertex shader, exact by 1/w and drawn once (`opengl.ren` draws such batches twice). Draws at the engine's mode and scales to the screen. `gles/run_test.sh`: 24 pixel checks, single-sampled and 4x, with and without the driver's depth clamp, all pass on Mesa's llvmpipe (OpenGL ES 3.2). Builds without warnings for arm64 |
| `mrplatform.h` on Android (`gles/android_platform.cpp`) | done: the mode list from the screen's size, no mode switching |
| Renderer core (`src/modern/render/mr_*.cpp`) on 64-bit | blocked on M2: its engine-struct mirrors are 32-bit; otherwise it needs `__stdcall` macros and a non-DirectDraw movie path |
| M1: CMake build, 32-bit Linux (`native/`) | **libraries and game modules compile**: every lithshared library, CShell and Object (with `src/modern/`'s code and the flex/bison parser) with clang `-m32`, from unedited include lines (a case-insensitive overlay) and small source edits that leave the Windows build's code as it was. `native/README.md` lists how |
| M1: platform layer, engine, link | next |
| Everything else | M1 to M5 above |

## Building and testing what exists

The backend's tests run on any Linux with Mesa:

    sudo apt install g++ libx11-dev libgles-dev libegl-dev xvfb libegl-mesa0
    src/modern/render/gles/run_test.sh

For arm64 (a compile check until the Android build exists):

    sudo apt install g++-aarch64-linux-gnu libgles-dev libegl-dev
    aarch64-linux-gnu-g++ -std=c++17 -fPIC -c src/modern/render/gles/gles_device.cpp

## Licences

`src/game/` and `src/LT2/` are Monolith's NOLF source release, whose
licence (`src/MONOLITH-README.txt`, section IV) restricts redistributing
the source and requires anything built from it to say it is not made or
supported by Monolith. A build for your own phone is in the same position
as the Windows builds this repository already makes. Publishing an APK
(on a store or elsewhere) distributes compiled Monolith code and is a
separate question; read that licence first. OpenMiles is GPL-3.0, which
would apply to an app linking it; SDL2 is zlib; FFmpeg as configured
here is LGPL-2.1.
