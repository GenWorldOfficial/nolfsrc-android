# Installing and playing

This guide is for players. It explains how to get from a release download to
a running game. To build the game from source instead, see
[`BUILDING.md`](BUILDING.md).

## What to download

Each release has these archives:

| Archive | What it is |
|---|---|
| `nolf-modern-release-public.tar.gz` | **The one you want.** The game built from source with the modern build's fixes and features. |
| `nolf-legacy-release-public.tar.gz` | The game built exactly as it shipped in 2001, bugs included. Kept for archive purposes. |

No game files are included. Each archive has an installer script that copies
the game's data from your own disc images and checks every file it writes.

## What you need

- **The two discs of _The Operative: No One Lives Forever_, Game of the Year
  Edition (Europe)**, as disc images. These are the images the installer
  expects:
  - <http://redump.org/disc/42823/>
  - <http://redump.org/disc/42561/>

  An image can be a plain `.iso` (your own rip), redump's `.bin` (or its
  `.cue`), or redump's `.zip` download. A `.bin` or `.zip` is first
  converted to a temporary `.iso`, which needs about 0.6 GB of free space per
  disc; it is removed at the end. Other editions and other regions' discs
  are not supported: the installer checks each image's checksum.
- **On Linux:** Wine with 32-bit support, or Proton (Steam), and `python3`
  if your images are `.bin` or `.zip`.
- **On Windows:** nothing else. The game is a 32-bit Windows program.

## Linux

1. Unpack the archive:

   ```sh
   tar xzf nolf-modern-release-public.tar.gz
   cd nolf-modern-release
   ```

2. Install the game data from your disc images, in either order:

   ```sh
   ./linux-install.sh /path/to/disc1.zip /path/to/disc2.zip
   ```

   With no arguments it looks for the images in the game directory and the
   directory above it. You can run it again safely: files already in place
   are checked and kept.

3. Play:

   ```sh
   ./linux-run.sh                           # as last left; at first a 640x480 window
   ./linux-run.sh --res 1920x1080 --fullscreen
   ```

   `./linux-run.sh --help` lists the options. The game uses its own Wine
   prefix, `~/.wine-nolf`, which is created on the first run, so your own
   prefix is left alone. `./linux-run-launcher.sh` starts the game through
   the original launcher instead.

Sound needs Wine's audio to work on your machine (`winecfg`, Audio tab). The
music needs nothing extra: the game directory has its own DirectMusic.

## Steam Deck and Proton

1. In Desktop Mode, unpack the archive and run `linux-install.sh` as above.
2. In Steam, use Games > Add a Non-Steam Game and pick the game directory's
   `NOLF.exe`.
3. In the shortcut's Properties > Compatibility, force a Proton version.

The controller works as an Xbox-style pad (see [Controller](#controller)).
No winetricks or Protontricks are needed.

## Windows

1. Unpack the archive (7-Zip and Windows 11's Explorer both open
   `.tar.gz`).
2. Drop both disc images onto `windows-install.bat`, or put them in the game
   folder and double-click it.
3. Start `NOLF.exe` and click Launch.

Windows support has had less testing than Linux. The programs are not
signed, so SmartScreen may warn before the first start. If something goes
wrong, please report it.

## First start

- **Screen.** The game starts in a 640x480 window. Choose the resolution and
  window mode in Options > Display. Window mode can be Full screen, Window or
  Borderless window. Wide screens are supported.
- **Renderer.** The default renderer is `d3d9.ren` (Direct3D 9). If you see
  rendering problems, pick `opengl.ren` or `d3d7.ren` (the original DirectX 7
  renderer) in Options > Display. On Intel integrated graphics with
  `d3d7.ren`, turn on "Intel graphics fix" if the screen goes black
  underwater.
- **Frame rate.** Single player is limited to 240 frames a second by default.
  Options > Interface offers 60, 120, 144, 240 or no limit. Multiplayer always
  runs at 60.
- **Crouch.** To press Duck once to crouch and once more to stand, set Options
  > Game > "Toggle crouch" to Yes.

## Controller

An Xbox-style (XInput) controller works in the menus and in the game. That
is also what Steam Input makes of the Steam Deck's controls and of most other
pads.

- In the game, the left stick moves and the right stick looks; the triggers
  fire and zoom.
- In the menus, use the d-pad or a stick to move, A to choose and B to go
  back.
- Start opens the menu; A skips a cutscene.
- Options > Controls > Controller sets the look speed, inverted look and dead
  zones. Customize Controls binds the buttons like keys.

## Console and cheats

Press the key left of `1` (backquote), or Shift+Esc, to open the developer
console in single player. Type `Help` for the commands and `Cheats` for the
cheat codes.

- [`CONSOLE.md`](CONSOLE.md) lists every command and cheat.
- [`VARIABLES.md`](VARIABLES.md) lists the settings you can change there.

If you get stuck in the level geometry, `mpclip` lets you fly out.

## Where things are

| File | What |
|---|---|
| `autoexec.cfg` | Your settings and key bindings, rewritten when the game exits. Delete it to start from the defaults (`defaults.cfg`). |
| `Save/` | Saved games. |
| `modern.log` | The modern build's log, emptied each run. Attach it when you report a problem. |

## Multiplayer

The internet server list uses the OpenSpy master server
(`master.openspy.net`) in place of GameSpy's, which is gone. The modern build
plays with retail 1.003 players and servers.
