# Console commands

This file lists everything you can type into the game's console: the
engine's commands, the game's, the modern build's, and the cheat codes.
Variables (`Name value`) are listed in [`VARIABLES.md`](VARIABLES.md).
The modern build's other changes are described in [`MODERN.md`](MODERN.md).

Commands and variable names are not case-sensitive. Arguments are separated by
spaces; quote an argument that contains spaces.

## Opening the console

| Build | How |
|---|---|
| Retail | No console a player can open. The engine has one, but it only gets keys during demo playback. You can still run commands from the command line (`lithtech.exe +DisableMovies 1 ...`), from `autoexec.cfg` (run at start-up), or from a file with `Exec`. Cheat codes are typed in the chat line (`T`). |
| Modern | A drop-down console, opened and closed with the `ToggleConsole` control (default: the key left of `1`, backquote), or opened with Shift+Esc on any keyboard layout. Esc also closes it. It opens in single player only, and pauses the game while it is open. Up/Down recall the last 32 lines. PgUp/PgDn/Home/End scroll. `Help` lists every command. |

What happens when you press Enter in the modern console:

1. If the line is a cheat code, the cheat runs.
2. Otherwise the line runs as a client console command, or sets a variable.
3. In single player, or when you host, the same line is also sent to the
   server's console. So server commands (section 2) work without `serv`.

## 1. Engine client commands

| Command | Arguments | What it does |
|---|---|---|
| `quit` | | Quit the game. |
| `World` | `<name>` | Start a game on a world (`<name>.dat`, or `worlds\<name>.dat`). |
| `serv` | `<command ...>` | Run the rest of the line on the server's console. |
| `Exec` | `<file>` | Run a file of console commands. |
| `Set` | | Print every client console variable and its value. |
| `ListCommands` | | Print the engine's built-in commands (not the game's; use `Help` in the modern build). |
| `Bind` | `<device> <trigger> <action> [...]` | Bind a key or button to one or more actions. Clears the trigger's old bindings first. |
| `RangeBind` | `<device> <trigger> <low> <high> <action> [...]` | Bind a range of an axis to actions. |
| `Scale` | `<device> <trigger> <scale>` | Set an axis' scale. |
| `RangeScale` | `<device> <trigger> <scale> <min> <max> <precenter>` | Set an axis' scale and range. (Retail bug: it checks for 5 arguments but reads 6.) |
| `AddAction` | `<name> <number>` | Create an input action. |
| `EnableDevice` | `<device>` | Turn on an input device, e.g. `"##gamepad"`. |
| `ListInputDevices` | | List the input devices. |
| `TimeDemo` | `<demo>` | Play a recorded demo as a benchmark. Give the file name without its extension. |
| `SSFile` | `<name>` | Base name for screenshots (default `Screenshot`). |
| `UpdateServer` | `0\|1` | `0` stops updating the local server each frame, which freezes the game world. |
| `RenderCommand`, `RCom` | `<args ...>` | Pass a command to the renderer DLL. |
| `RestartRender` | `[dll]` | Restart the renderer, optionally with another `.ren` file. |
| `ResizeScreen` | `<width> <height>` | Change the resolution. Goes back to the old one if the new one fails. |
| `RestartConsole` | | Reset the console's display. |
| `MoveConsole` | `[Top\|Bottom\|Middle\|Full\|l t r b]` | Print or set the engine console's rectangle. |
| `EnvMap` | `<texture>` | Set the environment-map texture. |
| `RebindTextures` | | Free the loaded textures, so they are loaded again. |
| `ModelAdd` | `<r> <g> <b>` | Light added to every model, each 0-250. |
| `ModelDirAdd` | `<r> <g> <b>` | Light added to every model's directional lighting, each 0-250. |
| `ShowTicks` | `ALL\|NONE\|SUMMARY\|RENDER\|GAME\|ENGINE\|GRAPH ...` | Turn timing counters on or off. |
| `ConsoleHistory` | | Print the command history. |
| `ClearHistory` | | Clear the command history. |
| `WriteHistory` | `<file>` | Save the command history to a file. |
| `ReadHistory` | `<file>` | Load the command history from a file. |
| `HeapCompact` | | Compact the memory heap. |
| `BlastServer` | | Does nothing. |

## 2. Engine server commands

These run on the server. In retail, run them from the client with `serv`,
e.g. `serv objectinfo`. In the modern console they work without `serv`
(see above).

| Command | Arguments | What it does |
|---|---|---|
| `world` | `<name> [mode]` | Start a world on the server. |
| `SpawnObject` | `<class> ["props"]` | Create an object 200 units above the first player, e.g. `SpawnObject PlayerVehicle`. |
| `objectinfo` | | Count the objects by class: total, active, and memory. |
| `ShowGameVars` | | Print every server console variable. |
| `ShowUsedFiles` | | List every file the server has opened. |
| `Portal` | `<name> 0\|1` | Close or open a portal. |
| `DisableWMPhysics` | | Give every object box physics. |
| `ExhaustMemory` | | Does nothing. |

## 3. Game commands (retail)

| Command | Arguments | What it does |
|---|---|---|
| `Cheat` | `<code>` | Run a cheat code (section 5). |
| `ChaseToggle` | | Switch between first and third person. |
| `FragSelf` | | Kill yourself. |
| `ExitLevel` | | Single player only: finish the current level. |
| `ChangeTeam` | `[team]` | Multiplayer: change team. |
| `Sunglass` | | Cycle the sunglasses' camera modes. Prints the mode. |
| `Music` | `I\|PS\|PM\|V\|SS\|SM ...` | Control the DirectMusic score: intensity, play a segment or motif, volume, stop a segment or motif. Type `Music` alone for help. |
| `InitSound` | | Restart the sound. |
| `Record` | `<world> <file>` | Start a world and record a demo. |
| `PlayDemo` | `<file>` | Play a recorded demo. |
| `ReloadWeapons` | | Reload the weapon attributes. |
| `ReloadSurfaces` | | Reload the surface attributes. |
| `ReloadFX` | | Reload the FX attributes, then the weapons and surfaces. |
| `LeakFile` | `<file>` | Load a level-compiler leak file and show it. |
| `Cmd` | `<command> <args ...>` | Send a command to the server's command manager, e.g. one a level's trigger would send. |
| `Commands` | | List the commands `Cmd` accepts. |
| `Trigger` | `<object> <message>` | Send a message to a named object in the level. |
| `Edit`, `Select`, `List` | | Object editing. Edit mode's key is compiled out of the final build, so these do nothing. |
| `AddClient`, `AddABunch`, `ListClients`, `RemoveClient`, `AddFrag` | | Scoreboard tests: add or remove fake entries and frags. |

## 4. Modern-build commands

| Command | Arguments | What it does |
|---|---|---|
| `Help` | | List every command: the game's and the engine's. |
| `Cheats` | | List the cheat codes and what they do. |
| `GetPos` | | Print the player's position. It is also written to `modern.log`. |
| `SetPos` | `<x> <y> <z>` | Single player only: move the player to a position. Use with `GetPos` to come back to a spot. |
| `ConsoleHistoryToLog` | | Write the console's scroll-back to `modern.log`. |

## 5. Cheat codes

Type a cheat code in the modern console, or in the chat line (`T`), or use
`Cheat <code>`. Every code starts with `mp`. Once you use a cheat, the game
records that you cheated.

**Final** means the cheat works in the retail game; the retail build left
out the others. **Modern** means it works in the modern build. In
multiplayer only `mpasscam` and `mpexorbitantamounts` work.

| Code | Effect | Final | Modern |
|---|---|---|---|
| `mpimyourfather` | God mode on/off | yes | yes |
| `mpdrdentz` | Full health | yes | yes |
| `mpwonderbra` | Full armor | yes | yes |
| `mpwegotdeathstar` | Full ammunition | yes | yes |
| `mpmimimi` | Every weapon | yes | yes |
| `mpsanta` | Weapons, ammunition, armor and health | yes | yes |
| `mpkingoftehmonstars` | Every weapon, and ammunition never runs out, on/off | yes | yes |
| `mpgoattech` | Every modification for the weapons you carry | yes | yes |
| `mpyoulooklikeyouneedamonkey` | Every gadget | yes | yes |
| `mpracerboy` | Spawn a motorcycle | yes | yes |
| `mprosebud` | Spawn a snowmobile | yes | yes |
| `mpasscam` | First/third person view (works in multiplayer) | yes | yes |
| `mppos` | Show your position on/off | yes | yes |
| `mpmaphole` | Finish the current level | yes | yes |
| `mpbuild` | Show the build's version | yes | yes |
| `mpexorbitantamounts` | Much more blood on/off (works in multiplayer) | yes | yes |
| `mpmiked` | An "EXORBITANT amounts of money?" box: Yes **quits the game**, No plays a voice line. Not in the `Cheats` list. | yes | yes |
| `mpclip` | Fly through walls on/off. Also gets you out of a stuck spot. | no | yes |
| `mpteleport` | Go back to the level's start point | no | yes |
| `mphookmeup` | The mission's starting equipment | no | yes |
| `mpboyisuck` | Remove every AI from the level on/off | no | yes |
| `mpbeenthere` | Every mission selectable on/off | no | yes |
| `mpwhoami` | Clear the player's record | no | yes |
| `mpcampos` | Show the camera's position and rotation | no | no |
| `mpwpos`, `mpwmpos` | Adjust the weapon's / muzzle's position | no | no |
| `mpcamera`, `mpcam` | Adjust the first-person camera offset | no | no |
| `mplightscale`, `mplightadd` | Adjust the level's light scale / light add | no | no |
| `mpfov` | Adjust the field of view | no | no |
| `mppoly` | Adjust the menu's polygrid | no | no |
| `mptriggers` | Show trigger boxes on/off | no | no |
| `mpbreach` | Adjust the weapon's breach offset | no | no |

Codes in the last group are still recognized, but do nothing in either build.

Cheats can also be applied at start-up: list them in `ClientButes.txt`
under `[Cheats]`, as `Cheat0 = "mpimyourfather"`, `Cheat1 = ...`.
