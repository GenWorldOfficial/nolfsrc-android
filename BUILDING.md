# Building

`make` is the whole build. This file describes what it does, and how to run
parts of it by hand.

## What you need

* **Linux with Docker.** The compilers are Win32 programs and run under Wine
  in a container; the host needs nothing else (no Wine, no Python).
* **The disc images**, `ISOS/NOLF1-CD1.iso` and `ISOS/NOLF1-CD2.iso` (or
  `NOLF_ISOS=<dir>`), matching `ISOS/SHA256SUMS`. See `ISOS/README.md`.
* **The compilers** in `toolchain/`: `make toolchain` downloads and assembles
  them (about 2 GB of downloads, 450 MB installed). See `toolchain/README.md`.
* About 6 GB of disk: 2.6 GB for the docker image, 2.8 GB for `out/` (the
  game directory, its tar, and the 0.6 GB Wine prefix), a few hundred MB of
  objects.

## What `make` does

1. Checks both disc images against `ISOS/SHA256SUMS` (on the host, before
   anything else; the result is remembered in `out/.isos-verified` while the
   files are unchanged).
2. Checks the compilers are in `toolchain/`.
3. Builds the docker image `nolfsrc-build` from `docker/Dockerfile` if
   needed (a few minutes, once).
4. Runs `build/all.sh` in a throwaway container (`--rm --init`, named
   `nolfsrc-build`), as your user, with the checkout mounted at `/repo` and
   the disc images read-only at `/isos`. The Wine prefix lives in
   `out/wineprefix`.

`build/all.sh` then:

| Step | What | Output |
|---|---|---|
| 1. resources | game data off the discs: the four `.rez` archives, configs, text, movies -- with the retail game DLLs inside the archives overwritten with zeros | `out/game/` |
| 2. third-party runtime | Miles Sound System (`mss32.dll` and its `.flt` / `.m3d` / `.asi` / `.as6` providers) and Bink (`binkw32.dll`) off the discs: licensed middleware with no source, which the engine imports | `out/game/` |
| 2b. launcher art | the splash bitmap, palette, music, icon and cursors `launcher.rc` compiles into `NOLF.exe`, read out of the retail `NOLF.exe`'s resources on the disc | `src/engine/launcher/res/` |
| 3. clean | every object and image from a previous run is removed | |
| 4. libraries | `build/libs.sh all` | `src/lithshared/out/*.lib` |
| 5. game DLLs | `build/build.sh all final`: `CRes.dll`, `CShell.dll`, `Object.lto` | `src/game/*/Release`, `Final_Release` |
| 6. engine | `build/engine.sh`: `ltmsg.dll`, `sres.dll`, `NOLF.exe`, `server.dll`, `lithtech.exe`, `d3d.ren` | `src/engine/*/Release` |
| 7. assemble | the built binaries into the game directory; the game DLLs and `sres.dll` go in `custom/`, the rez directory the engine searches last | `out/game/` |
| 8. audit | every PE file in `out/game/` listed with its origin (`out/game/MANIFEST.txt`); the build fails if one is unaccounted for, or if a `.rez` archive still holds one | |
| 9. package | `out/game/` as one tar | `out/nolf-game.tar` |

Every binary in `out/game/` is built from this repository except the 35
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
tools/playtest.sh out/game 'Worlds\M01S01' 60   # idle for 60 s on a level
```

`build/env.sh`, which every script sources, defines `run_cl`, `run_link`,
`run_lib`, `run_rc` and `run_icl` (the compilers under Wine) for one-off
commands. The engine targets in order, and the compiler each uses, are in
`build/engine.sh`'s header. `src/engine/server/` is compiled into both
`server.dll` and `lithtech.exe`, so after changing it rebuild both (`server`,
`server-link`, `lithtech`, `lithtech-link`).

## Notes

* The build is deterministic for a given checkout: rebuilding gives the same
  bytes except for the timestamps the linker writes.
* The game DLLs are the `Final_Release` configuration (`_REZFILE`, `_FINAL`),
  which reading data from `.rez` archives requires.
* `out/game/nolfcmds.txt` is the command file `run.sh` and the playtest
  start the engine with: the rez stack of the retail GOTY install with
  `custom/` last, a 640x480 window, and the intro movies off (they stall
  under some Wine setups).
