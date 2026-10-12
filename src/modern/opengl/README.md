# src/modern/opengl/ -- opengl.ren, an OpenGL renderer for the modern build

A render DLL the engine can load in place of `d3d.ren` (MODERN.md, M-P0-04):
the first step towards a game that runs on Linux without Direct3D. It is
new code, written from the Mac release's OpenGL renderer, which is the same
renderer for the same engine version and whose functions all kept their
names (`tools/pef.py` reads it), and from what the engine expects of
`d3d.ren` on the PC. Fixed-function OpenGL 1.x plus a few extensions, which
is what the Mac renderer uses and what Wine maps to any host's OpenGL.

It is built by `build/ext.sh` with `zig c++` (no VC6: nothing here has to
match anything), into `out/ext/opengl.ren`, and installed beside `d3d.ren`
in the modern game directories. `d3d.ren` stays the default.

## Using it

Pick it like any renderer: `"RenderDll" "opengl.ren"` (and `"CardDesc"
"OpenGL"`) in `autoexec.cfg`, `+RenderDll opengl.ren` on the command line,
or the launcher, which lists it as "LithTech OpenGL Renderer". A playtest:

    printf '"RenderDll" "opengl.ren"\n"CardDesc" "OpenGL"\n' > out/gl.cfg
    PLAYTEST_CFG=out/gl.cfg tools/playtest.sh out/nolf-modern-release -

Console variables it reads besides the engine's own (the names and defaults
`d3d.ren` and the Mac renderer share): `Bilinear`, `Trilinear`,
`Anisotropic`, `MipmapOffset`, `GroupOffset0`..`9`, `FilterOptimized`,
`MaxTextureSize`, `S3TCEnable`, `LightMap`, `Saturate`, `DetailTextures`, `DetailTextureAdd`, `DetailTextureScale`, `DrawWorld`, `DrawSky`, `DrawWorldModels`, `DrawAll`, `DrawSorted`, `SkyScale`, `FixTJunc`, `LightAddPoly`, the `Fog*` and `SkyFog*` variables, `DrawModels`, `DrawGuns`, `TextureModels`, `ModelSaturation`, `MaxModelLights`, `ModelLODOffset`, `32BitTextures`, `RenderDebug` (3 and up also writes
`opengl.log` beside the game, 4 logs every 2D call), and `VSync` (ours).
`RenderCommand info` prints the GL in use.

## Status

| stage | what | state |
|---|---|---|
| 1 | device and 2D: the WGL context on the engine's window (windowed, or full screen through `ChangeDisplaySettings`), the mode list, clear and flip, the engine's surfaces and every blit (keyed, scaled, warped, Optimized-2D colours and the seven blend modes), the screen lock and `FillScreenRect`, `BlitFromScreen`, screenshots (F8), DTX texture upload (32-bit, S3TC compressed or decoded) | done: menus' 2D, the console and the HUD's 2D draw |
| 2 | the world: BSP polygons with their textures and lightmaps, visibility, world models, the sky, fog | done but for portal views and panning-sky textures (`docs/world.md`): the engine's own visibility query (the PVS walk, with the renderer's leaf, object and frustum callbacks), the main world and world models (solid, chromakey, and translucent ones back to front through their BSP) with textures, vertex colours, lightmaps (static and animated, shadow-map animations included, in 512x512 atlases) and detail textures, at the retail PC's brightness (`Saturate`: lightmaps and vertex colours doubled); the sky objects' world models from the squashed sky camera; fog; the global light scale and the camera's light add |
| 3 | objects: models (with the engine's animation code compiled in, as `d3d.ren` does), sprites, particles, poly grids, line systems, canvases, dynamic lights, shadows | in progress (`docs/objects.md`): sprites (billboards, rotatable, glow, sprite bias, lit from the world's light grid and dynamic lights), models (the engine's trackers through our copy of its transform maker, CPU skinning, model-space lighting from the light grid, the visible dynamic lights and the game's model hook; LOD; translucent, additive and chromakey ones), particle systems, poly grids, line systems and canvases (the game's `ILTCustomDraw` callbacks) draw, and really-close objects (the player's weapon, the menus' 3D parts) in camera space over a depth buffer of their own. The main menu and M03S01's cabin match `d3d.ren`, the sky through the windows included. To do: dynamic lights on the world, the sun's light on models, chrome (environment maps), model shadows. Open: the first-person view model's hands show, which `d3d.ren` under Wine never draws |
| 4 | movies | the engine's screen-mode Bink and Smacker players ask the renderer for DirectDraw objects (`GetHook`), which an OpenGL renderer does not have, so movies are skipped; needs a texture path for them |
| 5 | Linux | the renderer is one part: `lithtech.exe` still needs Win32 for its window, input, sound and networking (the Mac engine shows where those seams are) |

## Files

| file | what |
|---|---|
| `ltrender.h` | the engine <-> renderer contract: `RenderStruct` (the 15 engine callbacks and the 37 renderer slots, with the Mac renderer's names), `RenderStructInit`, `RMode`, `PFormat`, `RenderBlitRequest`, `SharedTexture`, `TextureData`; every size checked at compile time |
| `glr.h` | the renderer's own globals and declarations |
| `gl_main.cpp` | exports, Init/Term, window and context, modes, clear/flip, console variables, the small slots |
| `gl_2d.cpp` | surfaces, blits, Optimized 2D, the screen lock |
| `gl_texture.cpp` | textures |
| `ltworld.h` | the engine's scene and world data as the renderer reads them: `SceneDesc`, `MainWorld`, `WorldBsp`, `Leaf`, `Node`, `WorldPoly`, `Surface` |
| `glscene.h` | the 3D half's shared declarations: the per-world context, lightmap atlases, multitexture |
| `gl_scene.cpp` | the world context, the camera and `RenderScene` |
| `gl_lightmap.cpp` | lightmaps: decoding, composing light animations, atlases |
| `globjects.h`, `gl_objects.cpp` | the visible set: objects by kind and the order they are drawn in |
| `gl_sprites.cpp` | sprites, and the world's light grid |
| `gl_models.cpp` | models: the transform maker, skinning, lighting |
| `gl_effects.cpp` | particle systems, poly grids, line systems, canvases |
| `opengl.def`, `opengl.rc` | the three exports; the launcher's name for the renderer (string 5) |
| `docs/` | the research the renderer is written from: `device.md` (the device and 2D half of the contract), `world.md` (the world's data and how it is drawn), `objects.md` (objects, models and the engine services a render DLL reaches), `mac-cvars.txt` (the Mac renderer's console variables) |

## The Mac renderer, for reference

`rez/games/nolf-mac-usa/NOLF/LithTech` (rez/README.md) is one PowerPC
executable: the engine with the renderer linked in where the PC loads
`d3d.ren`. The renderer is 514 functions, 225 KB of code, from
`opengl_init.cpp` to `lightmap_dynamic.cpp`; CodeWarrior's static
initialisers (`__sinit_<file>`) mark where each source file ends:
`opengl_init`, `common_stuff`, `common_draw`, `opengl_draw`, `drawsprite`,
`setupmodel`, `drawobjects`, `tagnodes`, `drawworldmodel`,
`drawparticles_A`, `drawparticles`, `drawsky`, `opengl_drawsky`, `vb_mgr`,
`opengl_texture`, `drawmodeltris`, `drawmodel`, `drawmodelshadows`,
`drawmodelshadowsproj`, `drawpolygrid`, `draw_canvas`, `lm_dualpass`,
`lightmap_surface`, `lightmap_light`, `lightmap_dynamic`.

`glRenderDLLSetup` fills the same 37 slots as `d3d.ren`'s `RenderDLLSetup`,
at the same offsets:

| slot | Mac function | | slot | Mac function |
|---|---|---|---|---|
| +0x6c | `r_Init(RenderStructInit*)` | | +0xb8 | `r_GetHook(char*)` (PC: GetRenderObject) |
| +0x70 | `r_Term` | | +0xbc | `r_SwapBuffers(uint)` (PC: FlipScreen) |
| +0x74 | `rgl_BindTexture(SharedTexture*, uint)` | | +0xc0 | `r_GetInfoFlags` |
| +0x78 | `rgl_UnbindTexture(SharedTexture*)` | | +0xc4 | `r_GetScreenFormat(PFormat*)` |
| +0x7c | `r_CreateContext(RenderContextInit*)` (PC: CreateWorld) | | +0xc8 | `r_CreateSurface(int, int)` |
| +0x80 | `r_DeleteContext(RenderContext*)` | | +0xcc | `r_DeleteSurface(void*)` |
| +0x84 | `r_Clear(LTRect*, uint32)` | | +0xd0 | `r_GetSurfaceInfo(void*, uint32*, uint32*, int32*)` |
| +0x88 | `r_Start3D` | | +0xd4 | `r_LockSurface(void*)` |
| +0x8c | `r_End3D` | | +0xd8 | `r_UnlockSurface(void*)` |
| +0x90 | `r_IsIn3D` | | +0xdc | `r_OptimizeSurface(void*, uint32)` |
| +0x94 | `r_StartOptimized2D` | | +0xe0 | `r_UnoptimizeSurface(void*)` |
| +0x98 | `r_EndOptimized2D` | | +0xe4 | `r_LockScreen(int, int, int, int, void**, int32*)` |
| +0x9c | `r_IsInOptimized2D` | | +0xe8 | `r_UnlockScreen` |
| +0xa0 | `r_SetOptimized2DBlend(LTSurfaceBlend)` | | +0xec | `r_BlitToScreen(BlitRequest*)` |
| +0xa4 | `r_GetOptimized2DBlend(LTSurfaceBlend&)` | | +0xf0 | `r_WarpToScreen(BlitRequest*)` |
| +0xa8 | `r_SetOptimized2DColor(uint32)` | | +0xf4 | `r_MakeScreenShot(char*)` |
| +0xac | `r_GetOptimized2DColor(uint32&)` | | +0xf8 | `r_ReadConsoleVariables` |
| +0xb0 | `r_RenderScene(SceneDesc*)` | | +0xfc | `r_BlitFromScreen(BlitRequest*)` |
| +0xb4 | `r_RenderCommand(int, char**)` | | | |

The engine's own 15 callbacks at `+0x00`..`+0x38` are named by the Mac's
`r_InitRenderStruct` (in `ltrender.h`).

Where the Mac renderer and the PC engine disagree, the PC wins: the Mac
stubs the screen lock (`r_LockScreen` returns 0) and `WarpToScreen`, which
the PC game uses, and its 2D colour conversion loses a bit of green.
