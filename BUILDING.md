# Building

`make` is the whole build. This file describes what it does, and how to run
parts of it by hand.

## What you need

* **Linux with Docker.** The compilers are Win32 programs and run under Wine
  in a container; the host needs nothing else (no Wine, no Python).
* **The disc images**, `ISOS/NOLF1-CD1.iso` and `ISOS/NOLF1-CD2.iso` (or
  `NOLF_ISOS=<dir>`), matching `ISOS/SHA256SUMS`. See `ISOS/README.md`.
* **The compilers**, the `toolchains/` submodule: `git submodule update
  --init` (about 560 MB). See `toolchains/README.md`.
* About 6 GB of disk: 2.6 GB for the docker image, 2.8 GB for `out/` (the
  game directory, its tar, and the 0.6 GB Wine prefix), a few hundred MB of
  objects.

## What `make` does

1. Checks both disc images against `ISOS/SHA256SUMS` (on the host, before
   anything else; the result is remembered in `out/.isos-verified` while the
   files are unchanged).
2. Checks the compilers are in `toolchains/`.
3. Builds the docker image `nolfsrc-build` from `docker/Dockerfile` if
   needed (a few minutes, once).
4. Runs `build/all.sh` in a throwaway container (`--rm --init`, named
   `nolfsrc-build`), as your user, with the checkout mounted at `/repo` and
   the disc images read-only at `/isos`. The Wine prefix lives in
   `out/wineprefix`.

`build/all.sh` then:

| Step | What | Output |
|---|---|---|
| 1. resources | game data off the discs: the four `.rez` archives, configs, text, movies -- with the retail game DLLs inside the archives overwritten with zeros | `out/nolf-legacy-release/` |
| 2. third-party runtime | Miles Sound System (`mss32.dll` and its `.flt` / `.m3d` / `.asi` / `.as6` providers) and Bink (`binkw32.dll`) off the discs: licensed middleware with no source, which the engine imports | `out/nolf-legacy-release/` |
| 2b. launcher art | the splash bitmap, palette, music, icon and cursors `launcher.rc` compiles into `NOLF.exe`, read out of the retail `NOLF.exe`'s resources on the disc | `src/engine/launcher/res/` |
| 2c. Game of the Year text | the strings of the Game of the Year content (the "Rest and Relaxation" mission) and the GOTY credits, which Monolith's v1.003 source of `CRes.dll` lacks, read out of the retail GOTY `CRES.dll` on the disc; compiled into our `CRes.dll` beside `ClientRes.rc`, so its resources are the retail ones | `src/game/ClientRes/goty/` |
| 3. clean | every object and image from a previous run is removed | |
| 4. libraries | `build/libs.sh all`; `RegMgr` twice, as Monolith built it for each game DLL: the Release build for `NOLF.exe` and `Object.lto`, the Debug build (`RegMgr_Debug.lib`) for `CShell.dll`, which the Game of the Year `CShell.dll` linked | `src/lithshared/out/*.lib` |
| 5. game DLLs | `build/build.sh all final`: `CRes.dll`, `CShell.dll`, `Object.lto` | `src/game/*/Release`, `Final_Release` |
| 6. engine | `build/engine.sh`: `ltmsg.dll`, `sres.dll`, `NOLF.exe`, `server.dll`, `lithtech.exe`, `d3d.ren` | `src/engine/*/Release` |
| 7. assemble | the built binaries into the game directory; the game DLLs and `sres.dll` go in `custom/`, the rez directory the engine searches last | `out/nolf-legacy-release/` |
| 8. audit | every PE file in `out/nolf-legacy-release/` listed with its origin (`out/nolf-legacy-release/MANIFEST.txt`); the build fails if one is unaccounted for, or if a `.rez` archive still holds one | |
| 9. package | `out/nolf-legacy-release/` as one tar | `out/nolf-legacy-release.tar` |

Every binary in `out/nolf-legacy-release/` is built from this repository except the 35
Miles and Bink files of step 2; the audit prints them all. The steps are
idempotent: running `make` again rebuilds every binary and leaves the data
in place.

## Other targets

```sh
make test         # tools/playtest.sh with tools/playtest-basic.txt on
                  #   Worlds\T01S01 and Worlds\M03S01, under Xvfb; passes if the
                  #   game is still running after the script on both
make test TEST_LEVELS="M06S01"   # other levels
make progress     # score the build against the retail binaries, function by
                  #   function; fails if a function recorded as byte-identical
                  #   in progress/identical.txt no longer is
make progress NOLF_SOURCE=<dir>  # the same, with the lithshared libraries
                  #   (<dir>: the NOLF Source Code v1.003 release)
make progress-update NOLF_SOURCE=<dir>   # record the score in progress/ and
                  #   README.md
make shell        # a shell in the build container, at /repo
make clean        # remove out/ and every build output in the source tree
make distclean    # clean, and remove the docker image
```

`make test` leaves each run in `out/play/test-<level>/`: `result.txt`,
`wine.log`, `script.log`, the screenshots in `shots/` and a contact sheet
`shots.png`.

`make progress` scores what the last `make` built (it compiles nothing), so
run `make` first; the retail images it compares against are extracted from
disc 2 into `out/reference/` the first time. `progress/README.md` describes
the scoring and the test.

## Running parts of the build

Inside `make shell` every script runs on its own:

```sh
build/libs.sh stdlith                 # one library (or: all)
build/build.sh cshell final           # one game DLL (cres | cshell | object | all)
build/engine.sh server                # one engine target's objects ...
build/engine.sh server-link           # ... and its link
build/all.sh                          # the whole thing
build/reference.sh                    # the retail images, into out/reference/
build/engine.sh server verify         # compile, then match every function
                                      #   against the retail server.dll
build/engine.sh server-one smgr.cpp PlaySound   # recompile one file, match it
tools/playtest.sh out/nolf-legacy-release 'Worlds\M01S01' 60   # idle for 60 s on a level
```

`build/env.sh`, which every script sources, defines `run_cl`, `run_link`,
`run_lib`, `run_rc` and `run_icl` (the compilers under Wine) for one-off
commands. The engine targets in order, and the compiler each uses, are in
`build/engine.sh`'s header. `src/engine/server/` is compiled into both
`server.dll` and `lithtech.exe`, so after changing it rebuild both (`server`,
`server-link`, `lithtech`, `lithtech-link`).

The `verify` and `*-one` targets are the reconstruction loop:
`guides/02-the-loop.md` explains it, and `guides/` the rest of the method.

## Variant builds: debug and modern

`make` builds the release game: what the retail game was, compiled to its
exact machine code, and the only build `make progress` ever scores. Two
project defines add to it, each in a build of its own:

| | define | for | into |
|---|---|---|---|
| `make debug` | `NOLFSRC_DEBUG` | symbols, logging and crash backtraces: finding out why something goes wrong | `out/nolf-legacy-debug/` |
| `make modern` | `NOLFSRC_MODERN` | the retail bugs fixed (`RETAIL_BUGS.md`), and later new features | `out/nolf-modern-release/` |
| `make modern-debug` | both | debugging the modern build | `out/nolf-modern-debug/` |

All three are `build/variant.sh <variant>`. Each copies the source (without
any build output) into `out/build-<variant>/` and builds it there with the
same `build/all.sh`, so `src/*/Release` (what `make progress` scores),
`out/nolf-legacy-release/` and the release command lines are left exactly as they were.
`build/env.sh` passes the extra flags to the compilers only inside those
copies, and refuses them anywhere else, so neither define can reach the
release build or the scoring by accident. The first run of each variant
extracts the game data again (about 1.1 GB); later runs take about three
minutes. Play them as the release game, from the directory the table gives:
`out/nolf-modern-release/run.sh`, or
`tools/playtest.sh out/nolf-modern-release 'Worlds\T01S01'`.

### The modern build: `NOLFSRC_MODERN`

`make modern` compiles everything with `/D NOLFSRC_MODERN` and nothing else
changed. Every departure from the retail game -- a bug fix, later a new
feature -- goes behind it, with the retail code kept, unchanged, in the other
branch:

```cpp
#ifdef NOLFSRC_MODERN
    if (stricmp(pFilter, pName) == 0)       // fixed: report the filter that matches
#else
    if (stricmp(pFilter, pName))            // retail: reports the first that does not
#endif
```

The release build never sees such a block: `build/libs.sh`, `build/build.sh`
and `build/engine.sh` compile a copy of `src/` in `out/release-tree/` with
every `#ifdef`/`#ifndef NOLFSRC_MODERN` (and `NOLFSRC_DEBUG`) block resolved
and its directives removed (`tools/releasesrc.py`), then copy their outputs
back into `src/`. Merely skipping the blocks is not enough: VC6 lets a file's
layout, skipped lines included, decide where some out-of-line inline
functions go (`src/modern/README.md`). So the release build compiles exactly
what it compiled before -- as long as nothing is added outside a block, not
even a blank line; `python3 tools/releasesrc.py diff master` lists anything
that is. It still matches the retail binaries and `make progress` still
passes; check that after any such change (`build/engine.sh <target>-one
<file>` shows the function still matching). Test the two macros only with
`#ifdef`/`#ifndef`: anything else stops the release build with an error.

The game DLLs are also built from the same paths wherever the checkout is:
`build/build.sh` makes the checkout's `src/` Wine's drive `S:` and its
`toolchains/` drive `T:`, so their compiles run in `S:\game\...` with
`T:\...` on the include path. VC6's output for `CShell.dll` and
`Object.lto` depends on those paths (the same source built in two
directories of different path lengths gave different binaries), so without
this a worktree, the main checkout and `out/release-tree/` would each build
their own. The engine and the libraries keep `Z:` (`/`): their output never
depended on the path, and Intel C/C++ builds a different `d3d.ren` from a
working directory on a drive of its own (`build/env.sh`). The modern build is never scored -- it differs from
the retail game on purpose. When a fix lands, mark its entry in
`RETAIL_BUGS.md` as fixed, naming the file.

### The debug build: `NOLFSRC_DEBUG`

`make debug` builds the same game with debug symbols and the project's
debug-only diagnostics compiled in, for finding out why something misbehaves
or crashes.

What differs from the release build: every compile adds `/Z7 /Oy-
/D NOLFSRC_DEBUG` (debug information, frame pointers kept so a backtrace can
walk the stack, and the project's debug define; every other flag, the
optimisation included, is the release one), and every link adds `/DEBUG
/OPT:REF /INCREMENTAL:NO`. Each binary's `.pdb` and `.map` are put beside
it.

In the source, use `src/nolfsrc/nolfsrc_debug.h` (on every compiler's include
path, so any file of any target can include it):

```cpp
#include "nolfsrc_debug.h"

NOLFSRC_LOG(("loading world %s, %d objects", pName, nObjects));  // note (( ))
NOLFSRC_ASSERT(pObj != NULL);
#ifdef NOLFSRC_DEBUG
    // ... anything else that must exist only in the debug build ...
#endif
```

Without `NOLFSRC_DEBUG` the macros expand to nothing, arguments included, so
they can stay in the source; never put a side effect in their arguments. Log
lines go to `nolfsrc.log` in the game's working directory (`$NOLFSRC_LOG`
names another file), stamped with the time and source position, and to
`OutputDebugString` (`WINEDEBUG=+debugstr` shows them).

`lithtech.exe` installs a crash handler in the debug build
(`src/nolfsrc/nolfsrc_crash.h`, from `WinMain`): an unhandled exception
anywhere in the process -- engine, renderer or game DLLs -- writes a
backtrace to `nolfsrc.log`, for example

```
[ 450171572] crash: CRASH: exception 0xc0000005 at 0x025a6d1c
[ 450171940] crash:   #0  0x025a6d1c d3d.ren+0x36d1c  (0x10036d1c in its .map)
[ 450171970] crash:   #1  0x0041ac50 lithtech.exe!r_InitRender+0x130  [clientrender.cpp:1207]
[ 450171971] crash:   #2  0x00409eef lithtech.exe!ci_SetRenderMode+0x9f  [clientdsi.cpp:398]
[ 450171998] crash:   #3  0x01a8481f cshell.dll!CGameClientShell::OnEngineInitialized+0x1a0f  [GameClientShell.cpp:1037]
```

and the crash then goes on as it would have (Wine's own crash dialog, or the
playtest's CRASHED). A frame with no name is in code without debug
information -- the static C runtime, or a `d3d.ren` file compiled with
`/Qip`, for which Intel C/C++ 4.0 writes none -- and is given at the
binary's preferred base; `tools/mapaddr.py out/nolf-legacy-debug/d3d.map
0x10036d1c` names it from the map.

The playtest runs the game in `out/play/<name>/`, so a playtest's
`nolfsrc.log` is there.

## Notes

* The build is deterministic for a given checkout: rebuilding gives the same
  bytes except for the timestamps the linker writes. (The game DLLs also
  vary by a few bytes between builds made at different times; the engine
  binaries do not.)
* The game DLLs are the `Final_Release` configuration (`_REZFILE`, `_FINAL`),
  which reading data from `.rez` archives requires.
* `out/nolf-legacy-release/nolfcmds.txt` is the command file `run.sh` and the playtest
  start the engine with: the rez stack of the retail GOTY install with
  `custom/` last, a 640x480 window, and the intro movies off (they stall
  under some Wine setups).
