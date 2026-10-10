# What is whose

This repository mixes code from different origins under different terms.
This file says which is which; it is not legal advice and grants nothing.

## In this repository

| Path | Origin | Terms |
|---|---|---|
| `src/game/` | **Monolith Productions**: *No One Lives Forever Source Code v1.003* (May 2001), unmodified, including its Win32 `flex.exe` and `bison.exe` (GNU tools, used to generate the animation parser) | Monolith's end-user licence for the source release, reproduced in full in `src/MONOLITH-README.txt` (section IV). Read it: among other things it restricts redistribution of the source and requires add-ons to say they are not made or supported by Monolith. |
| `src/LT2/` | **Monolith Productions**: the LithTech 2 SDK and shared-library headers from the same release, unmodified (Monolith's prebuilt libraries and the Miles and Bink binaries that came with it are left out) | as `src/game/` |
| `src/MONOLITH-README.txt` | Monolith's readme for that release | as `src/game/` |
| `src/lithshared/` | the LithTech shared libraries. Much of it (`stdlith/`, `CryptMgr/`, `RegMgr/`, `MFCStub/`, the `ButeMgr/` parser, `lith/`) is adapted from Monolith's later public source release for *No One Lives Forever 2*; the rest was written or reconstructed for this project, including `ztools/`, `rezmgr/` and `gamespy/` (a stand-in for the GameSpy middleware). `zlib/` is zlib | Monolith's code under the terms of the release it came from; zlib under the zlib licence; the rest as `src/engine/` |
| `src/engine/` | the LithTech engine (`lithtech.exe`, `server.dll`, `d3d.ren`), the `NOLF.exe` launcher, `ltmsg.dll` and `sres.dll`: written for this project, reconstructed from the retail binaries so that it compiles back to them. It implements Monolith's engine and interfaces, and `launcher.rc` / `ltmsg.rc` reproduce the retail resource scripts (dialogs and strings) | written for this project; no licence is declared |
| `build/`, `tools/`, `docker/`, `Makefile`, the documentation | written for this project | no licence is declared |

## Not in this repository, taken from your discs at build time

| What | Origin | Where it goes |
|---|---|---|
| the game data: `.rez` archives, configs, text, movies | Monolith Productions / Fox Interactive / Sierra | `out/game/` |
| `mss32.dll` and its `.flt`, `.m3d`, `.asi`, `.as6` providers | Miles Sound System, RAD Game Tools | `out/game/` (the only binaries in the game not built from source) |
| `binkw32.dll` | Bink Video, RAD Game Tools | `out/game/` |
| the launcher's bitmap, palette, music, icon and cursors | Monolith, from the retail `NOLF.exe`'s resources | `src/engine/launcher/res/` (gitignored), compiled into our `NOLF.exe` |

The retail game DLLs inside the `.rez` archives are overwritten with zeros
when the archives are copied; the retail executables are not copied at all.

## Not in this repository, provided by you

The compilers in `toolchain/` (Microsoft Visual C++ 6.0, Intel C/C++ 4.0;
see `toolchain/README.md`), under their own licences.

The docker image is built from Ubuntu 24.04 packages (Wine, Python, Xvfb,
ImageMagick, ...), each under its own licence.
