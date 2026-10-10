# No One Lives Forever, built from source

Source code and a build for **The Operative: No One Lives Forever** (Game of
the Year Edition, PC, 2000/2001): every executable and DLL of the game --
the LithTech engine (`lithtech.exe`, `server.dll`, the Direct3D renderer
`d3d.ren`), the `NOLF.exe` launcher, `ltmsg.dll`, `sres.dll` and the three game
DLLs -- compiled from the source in this repository with the original
2000-era compilers.

<p align="center">
  <img src="screenshots/screenshot01.png" alt="The main menu, running from this build" width="49%">
  <img src="screenshots/screenshot02.png" alt="In game, running from this build" width="49%">
</p>

The engine source in `src/engine/` is a reconstruction: Monolith never
released it, so it was written to recompile to the retail machine code. The
game code in `src/game/` is Monolith's own NOLF Source Code v1.003 release,
unmodified. See `LICENSE-NOTES.md` for what is whose.

**No game data is included.** You need the two original CDs: the build reads
the game's data (and the Miles and Bink sound/video runtime, the only
binaries not built from source) straight from images of them.

## Roadmap

1. **Byte-for-byte compatibility, for preservation.** Every binary compiles
   to exactly the machine code that shipped, so this source is a faithful
   record of the game as released. The whole game already builds from this
   source and plays. Of the engine's functions, 6,759 already compile to the
   retail bytes, covering 62% of its code by size: 3,028 in `lithtech.exe`,
   2,027 in `server.dll`, 739 in `d3d.ren` and 172 in `NOLF.exe`, and nearly
   all of the shared libraries. The rest are written and working, but not yet
   identical.
2. **Integrate the [NOLF Modernizer](https://haekb.itch.io/nolf-modernizer)
   patch**, so its improvements are part of the source rather than a patch
   applied on top.
3. **Fix the bugs found during reverse engineering.** Rebuilding the engine
   to the exact bytes means reproducing its original bugs as well; once the
   byte-exact version is preserved, the fixes can go on top of it.
4. **Move the renderer from Direct3D 7 to Vulkan**, for native play on every
   platform, without Wine.

## Build

You need Linux with Docker, the two disc images, and the compilers:

1. Put `NOLF1-CD1.iso` and `NOLF1-CD2.iso` in `ISOS/` (see `ISOS/README.md`; they
   are checked against `ISOS/SHA256SUMS`).
2. Run `make toolchain` once: it downloads the original compilers from
   public archives into `toolchain/` (about 2 GB; see `toolchain/README.md`).
3. Run

```sh
make            # -> out/game/ and out/nolf-game.tar  (about 3 minutes,
                #    plus about 3 for the docker image the first time)
make test       # optional: plays two levels under Xvfb with scripted input
```

`make help` lists the other targets. `BUILDING.md` explains the build in
detail.

## Play

`out/game/` is a complete game directory. On a Linux machine with Wine
(32-bit support needed):

```sh
out/game/run.sh                      # windowed, 640x480
out/game/run.sh --res 1024x768
out/game/run.sh --world 'Worlds\T01S01'
```

To play elsewhere, copy `out/nolf-game.tar` (one file, about 1.1 GB), unpack
it and run `game/run.sh`. `game/run-launcher.sh` starts the game through the
`NOLF.exe` launcher instead. Both scripts document their options in their
headers. Sound needs Wine's audio to work on that machine.

## Layout

| Path | What |
|---|---|
| `src/engine/` | the engine, launcher and resource DLLs (reconstructed source) |
| `src/lithshared/` | the LithTech shared static libraries the engine and game link |
| `src/game/` | Monolith's NOLF Source Code v1.003 (client, server and resource DLLs) |
| `src/LT2/` | the LithTech SDK and shared-library headers from the same release |
| `build/` | the build scripts (`all.sh` is the whole build) |
| `tools/` | the extraction and link helpers the build uses, and the playtest |
| `docker/` | the build image (Ubuntu 24.04 + Wine) |
| `ISOS/` | where the disc images go, and their checksums |
| `toolchain/` | the compilers: `make toolchain` assembles them (not in the repository) |
| `out/` | everything the build produces |
