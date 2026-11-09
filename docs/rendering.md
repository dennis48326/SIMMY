# Rendering: GX through aurora

## Overview

The game renders exactly as on the GameCube: it records commands into render contexts, the renderer
executes them as **GX** calls, and **aurora** implements GX on Dawn (D3D12, D3D11, Vulkan, OpenGL, OpenGL ES via
WebGPU: the `backend` setting). The game always thinks in a **logical 640x480 frame** (EFB); aurora scales viewports, scissors
and EFB copies to the real EFB size (the internal resolution). PC features (internal resolution, MSAA,
16:9, split-screen layouts, a second window) are done by changing what aurora's frame is and by
remapping 2D draws, never by changing the game's coordinates.

## The pipeline

| Layer | Files | Notes |
|---|---|---|
| `EGraphics` / `ENgcGraphics` | [`e_graphics.cpp`](../src/engine/e_graphics.cpp), [`e_ngcgraphics.cpp`](../src/engine/e_ngcgraphics.cpp) | Frames, RC open/close, textures, render surfaces, `Flush`, censor rects |
| `ERC` / `ENgcRC` (render context) | [`e_rc.cpp`](../src/engine/e_rc.cpp), [`e_ngcrc.cpp`](../src/engine/e_ngcrc.cpp) | Records into `EDL` display lists (**16-byte entries on PC**, pointer payloads: [`edl.hpp`](../include/simmy/classes/edl.hpp)); immediate contexts execute at once |
| `ENgcScheduler` | [`e_ngcscheduler.cpp`](../src/engine/e_ngcscheduler.cpp) | Hands lists to the renderer |
| `ENgcRenderer` | [`e_ngcrenderer*.cpp`](../src/engine/e_ngcrenderer.cpp) (state, geom, gevert, particles, censor) | `Execute` (opcode dispatch), state commands, `SetShader`/`SetTexture`, TEV stages from shader layers, `BeginFrame`/`EndFrame` (open/close the aurora frame), `InitGX` (calls `GXInit`; pixel format **RGBA6_Z24**: destination alpha is needed by shadows and Apt masks) |
| `ENgcRenderStateCache::Apply` | [`e_ngcrenderstatecache.cpp`](../src/engine/e_ngcrenderstatecache.cpp) | Everything GX should be set to, applied before each primitive. Cache-owned groups (TEV, lights, channels, texgens, texture matrices, vertex descriptors) are sent only when changed; a full resend at the first Apply of a frame. `SIMMY_RS_CACHE=0` resends everything. **The one place 2D remapping happens** (`display_map_viewport`) |
| Shaders, textures, models | [`e_shader.cpp`](../src/engine/e_shader.cpp), [`e_ngcshader.cpp`](../src/engine/e_ngcshader.cpp), [`e_rshader.cpp`](../src/engine/e_rshader.cpp), [`e_ngctexture*.cpp`](../src/engine/e_ngctexture.cpp), [`e_rmodel.cpp`](../src/engine/e_rmodel.cpp), [`e_submodel*.cpp`](../src/engine/e_submodel.cpp) | GX texture formats C4/C8/CMPR/RGB5A3/RGBA8; models keep GX vertex arrays and display lists big-endian (aurora reads them as stored) |
| Level / house | [`e_rlevel.cpp`](../src/game/e_rlevel.cpp), [`e_3dwindow.cpp`](../src/engine/e_3dwindow.cpp) | `E3DWindow` = a camera view; `ERLevel::Draw` (80234C98) draws a house per player view; order tables sort translucent work |

Texture v runs up the screen: the game's images are stored bottom row first (2D rects pass uv0 = (0, 1),
uv1 = (1, 0)).

## Resolution, MSAA, aspect ([`pc_settings.cpp`](../src/platform/pc_settings.cpp), [`pc_splitscreen.cpp`](../src/platform/pc_splitscreen.cpp))

- **Internal resolution**: `VISetFrameBufferScale(height / 480)`; aurora makes the EFB 640x480 x scale
  (widened to the frame's aspect under its STRETCH viewport policy). EFB copies (frame effects, censor,
  screenshots) happen at that resolution. Live-changeable (F1).
- **MSAA** (`msaa`, restart only): aurora disables its readback system with MSAA, so screenshots use
  `aurora::gfx::resolve_pass` (pc_main.cpp `CaptureMsaaShot`); `ENgcRenderSurface::GetImageData` and
  similar CPU readbacks get nothing with MSAA.
- **16:9** (`aspect=16:9`, one player): the frame fills the window, `split_screen_aspect()` (behind
  `EGraphics::GetScreenAspect`) returns its aspect, so live mode/CAS/loading project **Hor+** (vertical
  field of view from 640x480 unchanged). Everything else drawn in 640x480 units is mapped into the 4:3
  centre by `display_map_viewport`, unless it is a perspective view already at the frame's aspect. HUD
  anchors: [ui.md](ui.md). Effect quads covering the 3D view switch `g_displayMap2D` off around them.
  `VILockAspectRatio` is not implemented by aurora: 16:9 follows the window's actual aspect.

## Split screen (two players)

The game draws both halves into one logical frame (`ELiveMode::DrawMain` -> `InitPlayerXWindow`
8008F1B8: each half at half the frame aspect). The modes only change aurora's frame and the aspect:

| `split=` | Frame | Each half |
|---|---|---|
| `gc` | 4:3 (640x480 logical, fitted) | 2:3 (the original) |
| `4x3` | 8:3 (1280x480 default) | 4:3 (as the GC "coop4x3" build) |
| `wide` | the window's aspect (1280x720 default) | 8:9 at 16:9 |
| `windows` | EFB fixed at 2 x max(window widths) x max(heights) (n x with the PC mod's extra players) | each half shown in its own window at its window's aspect (one strip per window with 3-4 players: [multiplayer.md](multiplayer.md)) |

**Two windows** ([`pc_window2.cpp`](../src/platform/pc_window2.cpp)): window 2 is an SDL window with a
WebGPU surface on aurora's device, registered as an extra present target. In split frames aurora's
primary present shows the left half in window 1, `present_to` the right half in window 2 (both via
`set_present_crop`); `split_screen_player(i)` makes `GetScreenAspect` return 2 x that window's aspect so
each half is projected for its window; 2D is fitted per window (`MapWindows`). Outside split frames window
2 mirrors window 1. Window 2 is never vsynced (mailbox/immediate). Closing it hides it (F2 / F1 bring it
back); its size/position/display/fullscreen are saved to simmy.ini.

**The aurora patch** ([`cmake/aurora_patches/0001-two-window-present.patch`](../cmake/aurora_patches/0001-two-window-present.patch),
applied by [`cmake/SimmyAuroraPatches.cmake`](../cmake/SimmyAuroraPatches.cmake) at configure time, to
fetched trees and to `SIMMY_AURORA_DIR`; skipped when already applied; fails loudly on another aurora
revision): `AuroraSetRenderSizeOverride(w, h)` (fixed EFB size) and `aurora::gfx::set_present_crop`
(present a stretched EFB rectangle). With neither set aurora behaves as upstream. The aurora revision
is pinned by `SIMMY_AURORA_TAG` in [`CMakeLists.txt`](../CMakeLists.txt).

## Effects (all from the raw decompilation, GC addresses in the code)

| Effect | Where | Notes |
|---|---|---|
| Shadows | e_rlevel.cpp, esim.cpp, e_istaticmodel.cpp, ifloor.cpp | Destination alpha as a stencil: sun-projected models outside, soft blobs inside rooms (need object data 0x1D) |
| Lighting | [`e_lightgrid.cpp`](../src/game/e_lightgrid.cpp), [`roomlights.cpp`](../src/game/roomlights.cpp), [`e_ipointlight.cpp`](../src/game/e_ipointlight.cpp), [`levellighting.cpp`](../src/game/levellighting.cpp) | 63x63 light grid, lamps (model light channel switches them), window portal lights, per-vertex floor light, sky colour from the sun; `SIMMY_HOUR`, `SIMMY_LIGHT_TRACE`. Wall parts take the grid's lights at both ends (`EIWallPart::Draw` 8002A4C0: `CalcLights4` 8022907C at the first point, the second point's lights merged in: range > 0.01, a colour channel over 1/255, no duplicate position, at most 4; on lot 9 both ends see the same room lights, so no wall changes there). A door or window takes the rooms half a tile either side as its two sides (`ISimsObjectModel::ApplyMatrix` 80052E3C, `PortalRoom` 80050C38). Placing, cancelling or selling a light source rebuilds the house and with it the grid (`RecomputeLightingGrid` 80212458) |
| Day/night shaders | `ERShader` multi-state update list (e_rshader.cpp), `ESimsApp::UpdateShaders` | Windows/lamps switch by the clock (state = hour*4 + minute/15); `SIMMY_SHADER_TRACE`, `SIMMY_SHADER_STATES=0` |
| Particles | [`psystem.cpp`](../src/engine/psystem.cpp), [`emitter.cpp`](../src/game/emitter.cpp) (PSystem, "PART"); [`fastparticleemitter.cpp`](../src/engine/fastparticleemitter.cpp), [`effectsmanager.cpp`](../src/engine/effectsmanager.cpp) (Effects::, "EEMT"); [`eparticle.cpp`](../src/game/eparticle.cpp), [`boneparticle.cpp`](../src/game/boneparticle.cpp), [`aptstring_particles.cpp`](../src/game/aptstring_particles.cpp) | Object attachments, bone particles, ghost, text particles. The particle system has its own RNG. `SIMMY_PARTICLE_TRACE`, `SIMMY_FX_OBJECT`, `SIMMY_BONE_FX` |
| Weather | [`weather.cpp`](../src/game/weather.cpp) | Rain (camera-following particles, wet floor, grey sky). Lightning is empty on GC |
| Frame effects | [`frameeffects.cpp`](../src/engine/frameeffects.cpp) | Motion blur, bloom, depth of field via EFB copies in logical units (aurora scales and resolves MSAA). Bloom has no notes in shipped data (dev only on GC). `SIMMY_FX_FRAME` forces them |
| Censor mosaic | [`e_ngcrenderer_censor.cpp`](../src/engine/e_ngcrenderer_censor.cpp) | GC peeked/poked the EFB; aurora has no peek/poke, so the rect is copied out and drawn as one-texel quads; blocks corrected for non-4:3 frames |
| Fades | [`interestfade.cpp`](../src/engine/interestfade.cpp) | Walls and objects in front of the point of interest |
| Mirror (CAS) | [`e_mirrorportal.cpp`](../src/engine/e_mirrorportal.cpp) | `EMirrorPortal` |
| Ghosts | [`ghost.cpp`](../src/game/ghost.cpp), `ESim::Draw` | `SIMMY_GHOST_TEST`. A green person (data 0x4A flag 2) is lit by one green ambient light only (`_vGhostGreen` 0.8, 1.8, 0.65; no sun or point lights) |
| Sim models | [`esim.cpp`](../src/game/esim.cpp) `initModel` | A player's sim reuses the composited model of the last house (`ReadModelFromCache` / `TransferModelToCache`, dropped at the main menu: `DeletePlayerCachedModel`); service NPCs draw their record's own model (`ServiceNpcOrderTableCallback`); a costume change flushes queued data-manager work and costume models (`UpdateSkinChange`) |
| Animation | [`e_animcontroller.cpp`](../src/engine/e_animcontroller.cpp), [`e_rmodel.cpp`](../src/engine/e_rmodel.cpp) | Layered and phase-locked tracks (`Layer`, `SetTrackPhaseLock`, `GetSlavePos`), procedural tracks (head turning), `ERModel::DrawHierarchical`. A sim's bound sphere (its lighting and sort point) is the pose's `CalcVisibilitySphere` (802EFFCC: the skeleton's sphere moved towards each animated track's root and radius by the track's intensity, `EBoundSphere::Combine` 8035A034 for layered tracks) and its bounds the box around it; once the controller has matrices, culling tests the box around the posed joints (`ESim::VisibilityTest` 800341E8, `CalcTightBoundBox` 802EA53C, `CalcNodePos` 802F0428). Vehicles (carpool, bus) take their bounds from the pose too and their shadow follows the car body (`ISimsObjectModel::Update`, `EIStaticModel::SlamOrient` 8022BFF8). Skeletons stay cached after their last reference (`ECharacterManager::CanCache` 8032333C). The release build discards animation events (`AddEventRange`, see porting-audit.md) |

## Gotchas and invariants

- **aurora reads GX array data by pointer later** (its GX processing runs after the game's calls). Do not
  free or overwrite vertex data the current frame drew; call `EGraphics::Flush()` (app thread: sends the
  open RC, `GXDrawDone` with the frame open) before freeing, as GC's `FreeVertexData` / `SimImageMaker` do.
- Each frame ends with texture stages 0 and 1 emptied (`EGraphics::DeselectTextures` 802F1034 through the
  immediate context, from `EndFrame`), so no texture stays locked by the renderer between frames.
- Skins are composited into 32-bit RGBA textures (`SimModel::GetSkinTextureDef`, a PC choice; GC used 8-bit
  palettised 0x8A skins). The 8-bit copies (`TextureUtils::CopyTexture32To8` 8030B228, `CopyTexture8To8`
  8030B960) and the 4D quantiser `ERTQuantize4D` are therefore unreachable and stay stubs; disc textures
  copied into a skin go through the ported `CopyTexture8To32` / `CopyTexture32To32`.
- `ERC::SetMipMap` (802F8270) records a mip-map bias that the renderer ignores, as on GC
  (`ENgcRenderer::SetMipMap` 8034A718 is empty in the release build).
- A texture being destroyed must leave the renderer's slots (`~ENgcTexture` clears `m_textures`; the Apt
  aux `freeTexture` unbinds first); GC tolerated use-after-free here, PC crashes.
- Headless runs never open an aurora frame: `platform_frame_tick` clears aurora's GX FIFO each frame, or it
  grows until aurora asserts (~60k frames). With a window, GX work issued between frames (render-to-texture
  skin compositing) belongs to the next frame and must not be dropped.
- A window that cannot present for a long time (minimised) may still accumulate GX work written outside
  `CanDraw` guards; not verified on Windows.
- Render surfaces (`ENgcRenderSurface`) render at their own pixel size, and their CPU readback is
  asynchronous (portraits complete a frame later, [cas-and-neighbourhood.md](cas-and-neighbourhood.md)).
- 64-bit: callbacks with 32-bit data slots in `EInstance` (shadows, lights, portals) store the object's
  **id**, not a pointer.
- The GPU pipeline cache is per build (`cache_<exe size>_<mtime>` under the pref path; older ones are
  deleted): a cache from another build can hold incompatible pipelines.
- Offscreen tests use aurora's headless surface; in a container Dawn falls back to llvmpipe (OpenGL ES),
  which renders correctly but slowly (~1 frame/s at 1080p MSAA 4: use `SIMMY_GFX_LOWRES_UNTIL` /
  `SIMMY_GFX_HIRES_SHOTS`).
- `--render-test` draws the boot shaders and three quads through the game's own renderer: the quickest
  check that shaders, textures, TEV and blending work on a new backend.
