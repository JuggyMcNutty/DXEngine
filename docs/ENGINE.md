# The engine

DXEngine is a hard fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), an
open-source reimplementation of Unreal Engine 1 that recognises this build of Deus Ex directly
(`DeusEx.exe` SHA1 `2a933e26aa9cfb33b37f78afe21434caa031f14a` is its `DEUS_EX_1112fm` database
entry), for Deus Ex.

It carries what [Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s ports need
(their launchers, embedded GPUs, pads) and what Deus Ex needs to play as the original does. Its
default branch is `main`.

What matches the original and what differs is
[`NATIVES.md`](NATIVES.md); how to work on it, [`DEVELOPMENT.md`](DEVELOPMENT.md).

## How it is kept

- **Pinned in the workspace.** Port Ex Machina's `ENGINE-PIN.txt` names the one commit the ports
  build, and its `scripts/engine.sh` fetches, checks, builds and pins the clone
  ([the repositories](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#the-repositories)).
  Builds go to `build/<port>/engine` in the parent folder of the repositories, never into the
  clone.
- **A hard fork.** Not in Surreal Engine's fork network; nothing is merged from it or sent to
  it; a fix worth having is ported by hand, as a commit of DXEngine's own. The two differ at the
  core, and Surreal Engine's contribution rules do not apply here.
- **Deus Ex only.** The engine is stripped back to focus only on Deus Ex, so other UE1 games do
  not launch, sorry.

## The engine's tools

The workspace builds, stages, runs and pins the engine
([quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start)).
In this clone, the engine's own:

```sh
tools/perf/perf.sh on|off|save         # the profiling hooks (below)
tools/perf/frame-report.py <run>...    # a PerfConsole run's frame by area and function (DEVELOPMENT.md)
tools/host-tools.sh                    # perf and the validation layer, into the repositories' deps/
tools/natives_audit.py                 # the original's natives against DXEngine's (NATIVES.md)
tools/dxcap.sh                         # scripted runs of both engines (DEVELOPMENT.md)
```

## Changing the engine

Commit the change here with the docs it affects (this one, [`NATIVES.md`](NATIVES.md) and
[`DEVELOPMENT.md`](DEVELOPMENT.md)), push it, then pin it in the workspace. Pushing and pinning
wait for the owner's go-ahead
([the workspace's rules](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#rules)).

- Each commit's message says what it changes, why, and how it was checked.
- DXEngine's history is published and never rewritten.
- Temporary debug hooks never reach a commit ([the rule](DEVELOPMENT.md#temporary-debug-hooks)).

### The profiling hooks

The frame-time profiling hooks are `tools/perf/perf-instrumentation.patch`, applied for a
profile and never committed: `tools/perf/perf.sh on`, and `off` afterwards.

- **Take them off before changing the engine**: a commit made with them on carries them. Commit
  before putting them back: `on` cannot merge over uncommitted changes to a file the hooks
  touch, and says so.
- **The patch is against DXEngine's head**, so a commit that touches the same lines moves
  them. `on` then falls back to a three-way merge (and stops if that leaves conflicts), and
  `save` rewrites the patch from the tree (all of it but `docs/` and `tools/`), so the next `on`
  and `off` apply cleanly.

With the hooks on, the engine builds with frame pointers (a little slower on the Smart Pro) and
reads [the hooks' variables](#settings-and-environment). `DXE_PERF_SAMPLE=<file>` is a
sampling profiler for devices without `perf`: it samples the main thread's CPU time, recording
each sample's program counter and the return addresses a frame-pointer walk finds, one block per
60-frame report, with `/proc/self/maps` beside it.
[`tools/perf/sample-report.py`](../tools/perf/sample-report.py) (Python 3, and the port
toolchain's `nm` for a cross build) turns that into self and inclusive time per function,
optionally under one caller (`--root ULevel::Tick`: the game tick) or with the callers of one
(`--callers`). A leaf function keeps no frame record, so its samples show its caller's caller as
the next frame. Profiling on a device:
[the Smart Pro's README](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Natives from the original

Where DXEngine has a Deus Ex native wrong or as a stub
([how stubs show](NATIVES.md#how-it-is-known)), the original is in the game's DLLs.
[dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info) is what has been read of
them, and
[working on the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#working-on-the-binaries)
says how to read more; `tools/natives_audit.py` lists the stubs.

## Running it

Each launcher starts it for every real launch: DXLauncher, every port's, the Smart Pro's
included, by its words, and `main`'s through its `run-game.sh`. By hand, from the game's
directory:

```sh
DXEngine --no-launcher /path/to/deusex --url=01_NYC_UNATCOIsland.dx
```

- `--no-launcher` (`-n`) puts the log on stderr, as `--verbose` (`-v`) does, and opens no error
  window: an error goes to stderr alone. With no game folder given, the engine runs the game
  whose `System` folder it starts in, or on Windows the Deus Ex the registry names; with
  neither, it says no game is found and exits.
- **A map is `--url=<map>` (or `-u=<map>`, `--url <map>`).** An option that takes a value
  (`--url`, `--ini`, `--userini`, `--userdir`, `--cmdline`, `--timeline`) takes the next word
  when it has no `=`, unless that word starts with `-`.
- **`--server`** runs a dedicated server of the `--url` map, as the original's `-SERVER`: no
  window, sound or player; `--lanplay` gives it the LAN tick rate
  ([multiplayer](NATIVES.md#multiplayer)).
- **SIGTERM or SIGINT quits as a closed window does**: at the next frame, the inis written back,
  exit 0, whatever the window (a dedicated server too). A second signal kills at once, as a
  stuck engine needs. Windows has no such signals: there a launcher's `Quit` (below) does so.
- `Running.ini` is the launcher's crash sentinel, not the engine's: a run under a launcher that
  is killed leaves it behind like any crash; a run by hand has none.
- `--ini=<file>` and `--userini=<file>` name the inis to read and write back, as the original's
  `INI=` and `USERINI=`; the `shot` console command writes the next `ShotNNNN.bmp` into the
  game's System folder (with `--userdir`, its `Screenshots` folder); `--timeline=<file>` gives
  keys, typed text, a virtual pad, the pointer and console commands at set times.
  [Scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines) use all three.
- **`--userdir=<dir>`** is the engine's own folder in place of `~/.config/DXEngine` (Windows'
  `%LOCALAPPDATA%\DXEngine`):
  `Settings.json`, the last run's log (`DXE-Log-LastRun.txt`), the crash reports, the shots (its
  `Screenshots` folder) and a class's own config file (a mod's, as `DXMTL.ini`), read from
  `<dir>` first, else from the game's System folder, and written to `<dir>`. The engine makes
  the folder; without the option nothing changes.
- **`--version`** prints `DXEngine <commit> (<date>)` and a `features:` line, the words for
  what a launcher can rely on (`userdir`; `loadgame-url`, a start URL of `?loadgame=N`:
  [the command line](NATIVES.md#the-command-line); `int-paths`, a mod's `.int` files read from
  its own folders: [mods](NATIVES.md#mods)), and exits before anything is loaded.
- **`--cmdline=<line>`** is the original's command line, which the launchers pass on (the
  recreated one, deusex-launcher's `main`, and DXLauncher): its start URL, `-server`, `INI=`,
  `USERINI=`, `EXEC=` and safe mode's flags, over the options above
  ([the command line](NATIVES.md#the-command-line)).
- **`DXL_LAUNCHER_FD`** in the environment is the line to the engine of the launcher that
  started it, which stays for the game's run as the original's process does: a socket's
  descriptor, on Windows an inherited pipe's handle. The engine says `hello` as it starts and
  `ready` once its first map is in, takes `TakeFocus`, `Open <url>` and `Quit` (the launcher's
  stop, as a closed window quits), and passes a `RELAUNCH` as `relaunch <line>` before it exits;
  the lines and their meaning are
  [the game and the launcher](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#the-game-and-the-launcher)
  (DXLauncher's alike:
  [the game and the launcher](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#the-game-and-the-launcher)).
  Started by hand, with nothing set, nothing changes.
- Where there is no audio device (a container), give OpenAL Soft the null driver
  ([linux-x86_64's README](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#audio)).

## Measuring it

### Profiling and validating on the desktop

A device may have no `perf` and no Vulkan validation layer (the Smart Pro has neither), so
validation runs against the desktop's build (`scripts/dx.sh build linux-x86_64 engine`), and
so can a quick CPU profile. The desktop's proportions are not a device's (the Smart Pro's
Cortex-A53 pays far more for a cache miss), so what to work on next for a device is decided by
its own samples. `tools/host-tools.sh` unpacks pinned copies of Linux `perf` and the
Khronos validation layer into the `deps/` beside the repositories without installing anything.
From their parent folder:

```sh
DXEngine/tools/host-tools.sh
cd gamefiles    # the engine is started from the game's directory
# CPU profile of Liberty Island, recording from 25 s in (after the load):
LD_LIBRARY_PATH=../deps/perf/usr/lib ../deps/perf/usr/bin/perf record -F 2000 --delay=25000 -o /tmp/dxe.data -- \
    timeout 55 ../build/linux-x86_64/engine/DXEngine --no-launcher "$PWD" --url=01_NYC_UNATCOIsland.dx
LD_LIBRARY_PATH=../deps/perf/usr/lib ../deps/perf/usr/bin/perf report -i /tmp/dxe.data --no-children --sort symbol
# Synchronization validation (add DXE_VK_NO_BINDLESS=1 for the per-batch descriptor path):
VK_LAYER_PATH=$PWD/../deps/vulkan-layers/layers VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true \
    timeout 60 ../build/linux-x86_64/engine/DXEngine --no-launcher "$PWD" --url=01_NYC_UNATCOIsland.dx
```

`--call-graph dwarf` on `perf record` gives callers (the desktop build has no frame pointers
unless the hooks are on). Against the original, the harness measures both engines' frames the
same way ([measuring both engines](DEVELOPMENT.md#measuring-both-engines)).

### Against the original

Liberty Island's level start, the player idle, both engines on the desktop's Xwayland through
OpenGL, no vsync, 1280x720 (`PerfConsole`, `DXCAP_PERF=1`), three alternating runs of each, at
engine `ebda836` on the development machine (Ryzen 9 7940HS, Radeon 780M). Milliseconds a frame
of the main thread, by area (`frame-report.py --mean`), and the same with the view turning
(`PerfTurnConsole`):

| | Original | DXEngine |
|---|---|---|
| Frame (frames a second) | 4.20 (238) | 2.75 (364) |
| Main thread on the CPU | 3.97 | 2.58 |
| Cycles, instructions a frame | 21.3 M, 28.7 M | 13.7 M, 27.3 M |
| Scene render | 1.40 (`Render.dll`), 0.10 fractal textures (`Fire.dll`) | 1.47, the fractal textures in it |
| Script VM and objects | 0.39 (`Core.dll`) | 0.31 |
| Game tick, AI, physics, collision | 0.37 (`Engine.dll`, `DeusEx.dll`) | 0.23 |
| Render device, GL driver | 0.32, 0.38 | 0.05, 0.20 |
| C runtime | 0.96 | 0.17 |
| Turning: frame (frames a second), cycles | 2.14 (468), 10.1 M | 1.51 (663), 6.9 M |
| Instructions a cycle | 1.35 | 2.0 |
| Main thread off the CPU | 0.23 | 0.17 |
| The original's own counters: its game, its client | 0.45, 3.73 | |
| DXEngine's render by part: the visibility walk, meshes, light maps | | about 0.8, 0.25, 0.1 |

DXEngine's frame is the shorter, held and turning: its main thread takes fewer cycles a frame
for about as many instructions, and is off the CPU a little less. Its render takes about the
original's: the visibility walk (`DrawPolygon` and `DrawSpan`, `ProcessNodeSurface`,
`ProcessNode`, the box tests), meshes (`DrawLodMeshFaceDX`), light maps. The walk
waits on memory, not on arithmetic, so the original's own savings gain nothing here, or lose:
each node's box tested one frame in 16 while it shows, each point transformed once a frame into
a cache, a surface outside one side rejected without a clip
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame));
nor does skipping the nodes of zones not yet seen, or asking ahead (prefetch) for a node's
surfaces and far child. The cache's misses cost more than the transforms they save.

### The software device against Mesa's

Liberty Island's level start and Hong Kong's market, the player held (`PerfConsole`) and turning
on the spot (`PerfTurnConsole`), no vsync, at 640x480 and 1280x720 in the harness's X11 window,
three alternating runs of each, at engine `1f411ca` on the development machine (Ryzen 9 7940HS,
8 cores, 16 threads): frames a second, and in brackets the 99th percentile's frame in
milliseconds. Mesa 26.2.4's CPU drivers, lavapipe under the Vulkan device and llvmpipe under the
GL device ([how](DEVELOPMENT.md#the-software-device)), use every core too.

| | Software device | Vulkan on lavapipe | GL on llvmpipe |
|---|---|---|---|
| Liberty Island, held, 640x480 | 162 (7) | 95 (12) | 95 (12) |
| Liberty Island, held, 1280x720 | 103 (11) | 53 (22) | 54 (21) |
| Liberty Island, turning, 640x480 | 255 (7) | 122 (11) | 123 (12) |
| Liberty Island, turning, 1280x720 | 120 (11) | 63 (21) | 64 (20) |
| Hong Kong's market, held, 640x480 | 216 (6) | 114 (10) | 110 (11) |
| Hong Kong's market, held, 1280x720 | 107 (11) | 60 (19) | 60 (19) |
| Hong Kong's market, turning, 640x480 | 295 (5) | 150 (9) | 148 (10) |
| Hong Kong's market, turning, 1280x720 | 139 (10) | 88 (18) | 86 (18) |

The software device draws 1.6 to 2.1 times the frames of either, its slowest within 7 ms at
640x480 and 11 ms at 1280x720 ([which device draws](#rendering)).

## Settings and environment

The engine reads `Settings.json` from its own folder ([running it](#running-it)), and never
writes it; a UTF-8 byte order mark at its start is passed over, as DXLauncher's reader passes
it (editors on Windows add one).
DXLauncher, every port's, writes one in each profile's folder of its own
(`--userdir`); `main`'s launcher, the original recreated, writes none and leaves `HOME` as it
is. DXEngine's keys:

| Key | What it does |
|---|---|
| `Performance.RenderScale` | The scene drawn at this fraction of the window's size (0.25 to 1; default 1) and scaled up to it, by the Vulkan, GL and software devices ([how](#settings-the-launcher-exposes)). |
| `"Type": "GLES"` in `RenderDevice` | The GL device on an OpenGL ES 3.2 context (DXLauncher's `[GLES]` renderer, `data/renderers.ini`). |
| `"Type": "Software"` in `RenderDevice` | The software device, which draws on the CPU (DXLauncher's `[Software]` renderer; [rendering](#rendering)). |
| `"CPUDriver": true` in `RenderDevice` | The Vulkan device on Mesa's lavapipe, or the GL device on llvmpipe, a GPU passed over (DXLauncher's `[VulkanCPU]` and `[OpenGLCPU]` renderers; [rendering](#rendering)). |
| `Gamepad` | The pad: `Enabled`, `DeadZone`, `LookSensitivityX` and `LookSensitivityY`, `InvertY`, `CursorSpeed` ([the controller](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#the-controller)). |

The environment:

| Variable | What it does |
|---|---|
| `DXE_VK_NO_BINDLESS=1` | Forces the Vulkan device's per-batch descriptor-set path, the one a GPU without descriptor indexing (the Smart Pro's, for one) takes. |
| `DXE_DISPLAY_BACKEND` | The window backend: `SDL2`, `SDL3`, `X11` or `Win32`. Unset, or naming one the build lacks: the first that starts of Wayland, X11, SDL3 and SDL2, on Windows of SDL3, Win32 and SDL2. The launchers set it to the SDL backend their port's engine is built with, the one with the pad ([which](#running-on-our-devices)). |
| `DXE_FONT`, `DXE_MONOSPACE_FONT` | The font files DXWidgets' default loader takes for its UI and monospace fonts in a build without GLib and fontconfig, before the usual DejaVu and Noto files. Unused by the engine, whose UI's fonts are `DXEngine.pk3`'s. |
| `DXL_LAUNCHER_FD` | The launcher's line ([running it](#running-it)). |
| `DXE_SW_REFERENCE=1`, `check` | The software device shades with its float reference in its pipelines' place; with `check`, with both, each from the same frame, and logs every 60 frames how far apart they came out ([rendering](#rendering)). |
| `DXE_SW_GENERAL=1` | The software device draws with its general pipeline alone, which its specialized ones match to the bit. |
| `DXE_SW_THREADS=<n>` | The software device's threads, the game's among them: n in place of every hardware thread (1, the game's alone). |
| `DXE_GC_DRYRUN=1` | Each collection marks and frees nothing: the log has what would go, by where it lives and by class, and the first holder outside them of each object of a level left behind ([objects and memory](#objects-and-memory)). |
| `DXE_GC_STRESS=<frames>` | A collection every that many frames, besides the map loads'. |
| `DXE_GC_VERIFY=1` | Each collection checks that every pointer it is handed is a live object, walks all of the code, and logs the holders of pointers that are not, of code's references to code that reach an object not permanent, and of what the list kept from the code's last walk would have missed ([objects and memory](#objects-and-memory)). |
| `DXE_PERF_LOG=1`, `DXE_PERF_DETAIL=1`, `DXE_PERF_TURN=<aBaseX>`, `DXE_PERF_SAMPLE=<file>` | Only with [the profiling hooks](#the-profiling-hooks) on: where the frame goes, every 60 frames on stderr; tick by actor class and script functions by self time (which inflates what it measures); the player turning on the spot; the sampling profiler. |

## What it changes from Surreal Engine

By area, as the code stands. Each device's numbers are its port's, such as
[the Smart Pro's Performance](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Running on our devices

- **The version**: every build stamps DXEngine's commit and its date into `dxe_version.h`
  (`cmake/version.cmake`, a step of every build that rewrites the header only when the text
  changes, so only what reads it recompiles); `GetDeusExVersion` shows it under the main menu,
  as `1.112fm DXEngine <commit> (<date>)`, and `--version` prints it. A tree with uncommitted
  changes to tracked files reads `-dirty`; one without git, `unknown`.
- **Headless and embedded**: `--no-launcher` ([running it](#running-it)); a non-zero exit after
  a caught exception, which the launcher's crash sentinel reads. The cross build for an embedded
  aarch64 device is SDL2 only (no X11, Wayland or desktop GL), and takes SDL from pkg-config and
  a host-built `zipdir`. Every Linux build is at glibc 2.28, by a Buildroot SDK. The desktop's,
  linux-x86_64's, has its SDL3 built in (the launcher's), and linux-aarch64's, cross-built too,
  its SDL2, both without the Wayland window of its own, which needs waylandpp
  ([what it is made of](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#what-it-is-made-of)).
  The UI's fonts are `DXEngine.pk3`'s.
- **Windows**: windows-x86_64's build is llvm-mingw's, Clang on the UCRT, its C++ runtime and
  SDL3 built in and OpenAL Soft beside it as `OpenAL32.dll`, the libraries named to the linker
  where MSVC's `#pragma comment(lib)` named them. It waits as long as it asks
  (`timeBeginPeriod(1)`, where Windows' timer otherwise ticks every 15.6 ms), its log's lines
  end in `\n` alone, as Linux' do, and a dedicated server's `RELAUNCH` starts it again as a new
  process, its standard handles and no others handed on. The crash reporter is MSVC's alone: a
  crash under MinGW's build leaves its exception's code, which the launcher names.
- **The gamepad**: the SDL2 and SDL3 display backends give the pad as polled state
  (`GetGamepadState`), and `GamepadInput` turns it into UE1 joystick keys and axes that
  `User.ini` binds; menus get their own controls. DXWidgets builds one SDL backend, SDL3's
  when both are on: linux-x86_64's and windows-x86_64's engines are built with SDL3's,
  linux-aarch64's and the Smart Pro's with SDL2's, the Smart Pro's display being its vendor
  SDL2's alone. Each backend also has a virtual pad of its own for the harness's timelines
  ([timelines](DEVELOPMENT.md#timelines)).
- **The launchers' lines**: `--cmdline=` (`OriginalCommandLine`) and `DXL_LAUNCHER_FD`
  (`LauncherLine`) ([running it](#running-it)), with the console's `exec` and an `open` that
  takes a map's file name; `RELAUNCH` passed to the launcher, or a dedicated server started
  again by the engine itself (`Engine::Relaunch`, `GameApp::Relaunch`;
  [the menus' settings](NATIVES.md#the-menus-settings)).
- **Fullscreen under SDL** takes the display's mode nearest the size asked for
  (`SDL_SetWindowFullscreenMode` under SDL3, `SDL_SetWindowDisplayMode` and `FULLSCREEN` under
  SDL2), or with none (0x0) the desktop's, borderless (SDL3 with no mode, SDL2's
  `FULLSCREEN_DESKTOP`). With `DXE_SDL2_BORDERLESS_FULLSCREEN` (the Smart Pro's build) it is
  always a borderless window covering the display, set up while the window is hidden:
  `SDL_SetWindowFullscreen` deadlocks on a GL window with that device's vendor SDL2. The X11,
  Wayland and Win32 windows keep the desktop's mode.
- **The mouse and the keyboard under SDL**: the game's view takes the mouse in relative mode
  (`SDL_SetWindowRelativeMouseMode` under SDL3, `SDL_SetRelativeMouseMode` under SDL2), which
  hides the desktop's pointer, keeps it in the window and gives motion past the screen's edge;
  the UI draws its own. Text input is on only while the game takes text, a window of its UI up
  or the console typing (`Engine::TickWindow`, the window's `LockKeyboard`): in play a desktop's
  input method leaves held keys alone, where KDE's press-and-hold accent picker takes a held
  letter in a Wayland window with text input on.
- **The windows' icon** is `DeusEx.exe`'s icon group 128, as the original's windows show it
  ([the frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#the-frame)),
  read from the game's `System` folder as a window opens (`DXEngine/Utils/PEIcon.cpp`): its
  32x32 and its 16x16 with the most colours. The engine carries no icon of its own, so without
  that file a window has none.

### Settings the launcher exposes

`LauncherSettings` reads the `Gamepad` and `Performance` blocks DXLauncher writes
([the keys](#settings-and-environment)). Render scale is
`RenderDevice::GetRenderScale`: a device whose `SupportsRenderScale()` says so (Vulkan, GL,
software) makes its scene buffers at `GetRenderWidth()` by `GetRenderHeight()` and scales them
to the window as it presents; the viewport, the canvas and the UI see the render size.

### Rendering

The renderer's CPU work is lighter than Surreal Engine's:

- **Light maps** are lit only where a light reaches; a map whose animating or moving lights
  changed is built, converted and uploaded again only where they reach, now or at its last
  build, each light's shadows unpacked only there (`LightmapBuilder::FindAddedLights`,
  `TextureInfo::UpdateWidth`).
- **The light tree** and each surface's lights that can move are kept while no light changes,
  the level's surfaces' apart from each mover's, with the map it last drew.
- **Fog maps** take only the fog lights a line of sight from the eye to them can cross (their
  sphere against the pyramid from the eye to the map's corner texels,
  `FogmapBuilder::Reaches`); a map no fog light reaches stays all clear, its texels neither
  placed nor built, sent once as it clears, and kept on its surface, whose passes go by
  whether it has one (`LightSystem::UpdateFogmapTexture`).
- **The clipper's occlusion grid** is one row per image row up to a 4K frame (it followed the
  image to 2048 x 1080 and scaled a larger one into that, a row in four of a 1440p frame
  unsampled), each row's spans in a cache line
  of its own while 15 fit, then on the heap (`BspClipper::ClipRow`); its non-SSE (ARM) build
  skips the clip for triangles inside the view (a Surreal Engine bug); a surface is clipped once
  and filled into the grid as one polygon, a span to a row (`BspClipper::DrawPolygon`); a span
  that closes a gap in a row goes on into the gap after it (`BspClipper::DrawSpan`; Surreal
  Engine's passes that gap over, leaving it open, so what lies behind counts as showing and is
  drawn).
- **One-sided surfaces** seen from behind are skipped before the visibility test, and a
  surface's points are gathered only when a test needs them.
- **The actors' places in the BSP**: a static actor, which does not move, is filed when the
  level sorts its actors (static first, as the original's), then a sixteenth of them a frame,
  in turn, should a script change one's look; only those with a render iterator are visited
  each frame (`RenderSubsystem::UpdateStaticActors`). The dynamic ones are filed each frame.
- **Movers' mirrors**: each frame looks only at the movers with a mirrored face, found with the
  static actors (`RenderSubsystem::MirrorMovers`).
- **Light maps in shared pages** of 1024 texels a side (`LightmapAtlas`), each map placed when
  it is first built at its size, with a border of 4 texels repeating its edges: a surface's
  points lie between its map's first and last texel centres, where the bilinear filter takes
  the edge texel, as `D3DDrv`'s does though it wraps, and the border gives the anisotropic
  filter the same past them; a map too large for a page is drawn on its own. The surface is
  given the page at the map's place (its pan moved by it), which the devices draw as any
  texture, and a map's change is sent as a part of its page. The opaque surfaces are drawn by
  zone, texture and light map ([draw order](NATIVES.md#draw-order-and-movers)), so a run of one
  texture and one page is one batch where descriptor indexing is missing (the Smart Pro's).
- **Meshes**: each vertex animated, lit and fogged once a draw; a run of faces with one texture
  drawn in one device call, set up once (the GL device's `DrawGouraudTriangles` too).
- **A portal's spans** are put in order by row in linear time, not sorted (`SortPortalSpans`).

Deus Ex's look, as `Render.dll` and `D3DDrv.dll` make it, is described by feature in
[`NATIVES.md`](NATIVES.md); its code is in `DXEngine/Render/` (`VisibleFrame.cpp`,
`VisibleMesh.cpp`), `DXEngine/Light/` and
`DXEngine/Packages/Engine/Resources/Textures/` (`FireEngine.cpp`, the fractal textures).

Every device draws a 3D frame's polygons half a pixel right of and below where its projection
puts them, as `D3DDrv` draws the original's
([the scene's pixels](NATIVES.md#the-scenes-pixels)):
`PolygonShift` before the frustum in `SetSceneNode`, for a frame with
`SceneNode::PolygonsAtFX15` (`VisibleFrame::SetupSceneFrame`'s, a mirror's, sky's, warp zone's
and viewport window's among them, and `DrawClippedActor`'s). A canvas's frame and the coronas'
take none, and a sprite's quad is moved back by it at its depth (`VisibleFrame::TileOffset`);
the frame's `Projection`, which the clipper and the HUD's projections use, keeps the middle.

**Which device draws** (`Engine::ChooseRenderDevice`): with `-nohard`, the software device, as
the original's; else the `Type` `Settings.json` names; else the device the game's ini names
(`GameRenderDevice`): the software device for `SoftDrv.SoftwareRenderDevice`, the Vulkan device
for any other. A GPU device that does not start -- no driver, or none it can use -- gives way to
the software device, the window made again for it, full screen as the settings have it but with
`-noddraw` ([the command line](NATIVES.md#the-command-line)), and the error in the log
(`Engine::CreateGameWindow`), as the original's `WindowedRenderDevice` takes
`GameRenderDevice`'s place ([the software device](NATIVES.md#the-software-device)). A GPU device
that finds only Mesa's CPU driver, lavapipe, llvmpipe or softpipe, counts as not starting, the
software device being the faster on the CPU
([against Mesa's](#the-software-device-against-mesas)); with `"CPUDriver": true` it is asked
for: the Vulkan device takes lavapipe, the GPUs passed over, and the GL device llvmpipe
(`LIBGL_ALWAYS_SOFTWARE=1`, set before GL loads). The log names the driver either draws through.

**The Vulkan device**: the game tick runs while the GPU draws the previous frame
(`CommandBufferManager` keeps one frame in flight, and `WaitForFrame()` collects it before
anything it uses is touched; the swapchain is made again for a new size asked, not for an
extent the surface clamps, after waiting for the device). Without every descriptor-indexing
feature its bindless set uses (partial binding, runtime arrays, non-uniform indexing,
update-after-bind, a variable count) and room for that set, it binds a descriptor set per
batch instead of bindless textures.
It decodes on the CPU what the GPU cannot sample or filter: BC1, RGB8, RGBA32F, and converts
the lightmaps for upload in NEON on ARM. A mip bigger than its 64 MB staging buffer goes up in
bands of rows. Its draws take no depth bias, as the GL device's take
none, nor do `D3DDrv`'s but its detail passes
([detail textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#detail-textures)):
a translucent draw biased by its slope shows through the opaque one in front of it from afar,
an augmentation canister's inner glow through its shell (`CanisterConsole`). A mover's surfaces
take a step and a slope of 1 toward the viewer on Vulkan and GL (`SurfaceInfo::DepthBias`;
the Vulkan scene pipelines' dynamic depth bias, set as it changes, GL's `glPolygonOffset`;
[draw order and movers](NATIVES.md#draw-order-and-movers)). Multisampled, its scene shader
reads a texture at the pixel's centre, as without multisampling: `TileConsole` on RADV, at 2x
and 4x, draws its grey boxes pixel for pixel as without it, the `FontMenuHeaders` text at 1.00
of `D3DDrv`'s brightness block for block. Read at a covered sample (`centroid`), as the GL
device's shader reads it, a pixel two triangles share, along a tile's diagonal, takes skewed
derivatives on RADV, whose anisotropic filter blends a point-sampled glyph's texels with its
colour 0 into dark ones; on Mesa's radeonsi the GL device's tiles stay within a level of their
picture without multisampling.

**The GL device** runs desktop GL 4.2+ and OpenGL ES 3.2 from one code path: `RenderAPI::GLES`
and `RenderDeviceType::GLES`, an ES 3.2 context from every Linux window backend (SDL2 and SDL3
through `SDL_GL_CONTEXT_PROFILE_ES`, X11 and Wayland through EGL with the ES3 bit), and `IsGLES`
read at init from `glGetString(GL_VERSION)`. It draws at the render scale, as the Vulkan device
does ([the setting](#settings-the-launcher-exposes)). The same shader sources compile as GLSL ES
3.20 (`CompileGlsl` prepends the version and highp precision; the sources keep to `u`-suffixed
masks, float literals and `std140` push-constant blocks). No desktop-only calls: `glClearDepthf`
and `glDepthRangef`, `glDrawBuffers` for every `glDrawBuffer`, no `GL_DEPTH_CLAMP` or
`GL_MULTISAMPLE` on ES, the plain blend calls, the null texture as `UNSIGNED_BYTE`. It asks for
GL errors once a frame, not at each flush of its batches and each texture upload: every
`glGetError` makes Mesa's GL thread finish all it has queued. A draw sets only the state that
differs from the last draw's (`DrawState`: program, samplers, textures, blend, depth), forgotten
wherever else that state is set.

- **ES 3.2, not 3.0**: the shaders bind their samplers with `layout(binding)`, which GLSL ES
  3.00 lacks. `gl_FragCoord.w` is 1/w on ES as on desktop, so the detail bands fall at the same
  distances on ES as on desktop.
- **Antialiasing on ES**: the multisampled scene buffers are made with ES 3.1's
  `glTexStorage2DMultisample`, as ES lacks desktop GL's `glTexImage2DMultisample`; on either,
  the samples are halved until every attachment takes them (an ES driver may take one sample of
  the integer hit buffer). It works on the desktop's ES 3.2 context; DXLauncher keeps
  antialiasing off on a PowerVR, the Smart Pro's GE8300 among them, whose resolve is speckle.
- **The desktop context**: every Linux window backend asks for a desktop 4.2 core context, as
  the shaders are GLSL 4.20 (`CompileGlsl` prepends `#version 420`): a driver without 4.2 fails
  at the context, with an error that says so, not at a shader.
- **The vertices** stream through CPU staging arrays, each flush uploading the range written
  since the last one (`glBufferSubData`): the GE8300 has no `glBufferStorage` (persistent
  mapping), and mapping the buffers' unused tails on every flush costs most of its frame.
- **The scene buffers** are RGB10_A2, by choice (`Hdr` is the Vulkan device's, from an scRGB swap
  chain; no window gives GL a float or HDR framebuffer, so the setting is not taken here): darks banded less
  than in `D3DDrv`'s 8-bit frame, as in the Vulkan device's 16-bit buffers, at 32 bits a pixel
  (`LightConsole` under 02_NYC_Underground's `Light33`, gamma-corrected: 63 levels under 64
  where RGBA8 gives 54, no gap over 2 where it leaves 6). ES's too: the Smart Pro's GE8300
  draws, resolves, samples and reads them, their ten bits kept (Port Ex Machina's
  `tools/probes/probe-gles-scene.c`), at no cost to its frame. A driver whose scene framebuffer
  of RGB10_A2 comes up incomplete gets RGBA8 (logged). Each is made with immutable storage
  (`glTexStorage2D`), which takes a format alone. They hold the frame top row first, so a scene
  drawn in a window (a security camera's view on its computer) lands where the Vulkan device
  puts it: its viewport is placed from the top, where Surreal Engine's places it from the
  bottom.
- **BC1** without `EXT_texture_compression_s3tc` and RGBA32F (the lightmaps) without
  `OES_texture_float_linear` are decoded to RGBA8 on the CPU, and BGRA8 goes up as RGBA8, red
  and blue swapped, ES taking no BGRA upload.
- **A DXT1 texture's levels**, on ES and desktop GL alike, are each its own size down to 1x1,
  made by their uploads (`glCompressedTexImage2D`), as ES takes no compressed format in
  `glTexImage2D`; a level with no mip of its own is made of zero blocks, so the chain is
  whole.
- **The samplers** set no `GL_TEXTURE_LOD_BIAS` on ES (no ES sampler has it), anisotropy only
  with `EXT_texture_filter_anisotropic`, and `GL_MIRROR_CLAMP_TO_EDGE` only where a probe at
  init finds the driver takes it (else `CLAMP_TO_EDGE`).
- **`ReadPixels`** reads the frame at the size it was drawn (`LastFrameSize`, not the
  window's), the right way up, the gamma-corrected one too (Surreal Engine's is an `#if 0`
  stub of D3D11 code, its shots black), RGB10_A2's to bytes by a 4×4
  ordered threshold, so that a region's mean is the buffer's: the driver's rounding to the
  nearest byte reads the darks a tenth of a level under, 1023 not being four times 255
  (`MeshConsole`'s pier 0.7% darker). `Exit()` runs once and tolerates a failed unmap, as the
  intro can end the game after the window's context is gone.
- **The post-process passes bind their own samplers**, as the Vulkan device's do: the present,
  the bloom and the gamma-corrected read-back sample the image linearly, held at its edges, and
  the dither texel for texel (`PresentPass.PPLinearClamp`, `PPNearestRepeat`). A sampler object
  on a unit overrides the texture's own state, and the scene's draws leave theirs bound
  (`DrawEntry`): without its own, the render-scale upscale would take the last draw's,
  point-sampled after the HUD's text.
- **Anisotropy** (8) goes on every scene sampler, as the Vulkan device's, where the driver has
  it (`CreateSceneSamplers`).
- **Depth is clamped** at the near and far planes on desktop GL, as the Vulkan device clamps it
  (the weapon close up): `DepthClipEnable` is D3D11's, off meaning a clamp. ES has no clamp and
  clips there.
- **A mirror's frame is clipped at its plane** by a clip distance, as the Vulkan device's:
  `GL_CLIP_DISTANCE0` is on only in a frame whose `NearClip` is not the default, so the main
  frame draws as it would without it, and off for the passes after the scene. ES has clip
  distances only with `EXT_clip_cull_distance` (its directive the line after the version).
  Without it, as on the Smart Pro's GE8300, a mirror's frame is drawn by twins of the scene's
  two programs that discard what lies behind the plane, and no other draw pays for a discard,
  which costs a tile-based GPU its removal of hidden surfaces before shading. The discard gives
  the clip distance's picture (`MirrorConsole`'s `Straddle` on Mesa with the extension set
  aside, within 0.03 of a level) at no cost to the GE8300's frame.
- **The screen flash** is a quad on the near plane, z -1 and depth 0, where the Vulkan device's
  z 0 is: at z 0, GL's depth 0.5, the depth test would pass it only where the windows draw
  nothing solid, leaving their text, bars and icons out of the flash.

**The software device** (`DXEngine/RenderDevice/Software/`) draws on the CPU, for a machine
whose GPU has no driver: its window is a bitmap's (`RenderAPI::Bitmap`), its frame BGRA8, as
`D3DDrv`'s 8-bit frame, with a float depth buffer, both at the render size. It records a frame's
calls, copying each scene node and vertex, and draws them on every hardware thread, the game's
among them (`SoftwareWorkers`, `DXE_SW_THREADS`): each chunk of about 128 triangles set up and
binned to the frame's tiles of 64 pixels a side, then each tile drawn by one thread, the most
binned first, its clear first, then what was binned to it in order. With three threads or more,
and not with the float reference (`DXE_SW_REFERENCE`), the chunks go to the other threads in
waves as they close, while the game's thread records what follows, the frame's clear with
`Lock`; what is left, or else all of it, is drawn at `Unlock` on all of them, and no frame is
drawn while the next is recorded. What a wave reads stays put: the recording's storage grows
only once the wave is done; a texture changed in place after a draw not yet drawn named it (a
light map's shared page, where its texels are among the draw's) has what is recorded drawn
first, so each draw reads it as it was when the draw was made, as a `Flush` within a frame draws
what is recorded before it lets the textures go; and a quad's lanes outside its primitive sample
where one inside does, so a draw reads no texel but its pixels'.

- **Triangles**, as the Vulkan device's: each in clip space by its scene node's transforms,
  clipped where a corner lies behind the node's clip plane (the Vulkan device's clip distance,
  a mirror's plane in a mirror's frame) or outside a guard band of 8192 pixels, and held to the
  viewport; a polygon a fan, (0, i - 1, i). Snapped to 1/256 pixel, its pixels by the top-left
  rule, as RADV and lavapipe cover them; its attributes over w, at each pixel's centre. Depth
  float, tested LEQUAL, clamped to the pipeline's range (the scene's 0.1 to 1, a line not depth
  cued 0 to 0.1); a mover's surfaces biased as the Vulkan device biases them, constant -1 and
  slope -1 on a float buffer. A line is a pixel wide.
- **Textures**: a copy of its own of each, by cache ID, a masked one apart, as the Vulkan
  device keeps its images: BGRA8, each format converted as the Vulkan device's uploads and CPU
  decoders convert it, each mip with a column and a row past its edges repeating its first, for
  the bilinear filter; a realtime change converted again, only the part that changed where it
  says so.
- **Tiles**: one square on the screen is a rectangle, its pixels and attributes those of the GPU
  devices' two triangles.
- **Sampling** as `D3DDrv` samples with the game's ini (`UseMipmapping=True`,
  `UseTrilinear=False`): bilinear, its weights of 8 bits, from the mip whose texels are no
  larger than the screen's pixels, no anisotropy; `PF_NoSmooth` the nearest texel; a texture
  wrapped, a tile held at its edges where its coordinates lie within it
  ([the software device](NATIVES.md#the-software-device)). The mip is the floor of log2 of a
  quad's scale factor, its bias of -0.5 fitted to `D3DDrv`'s frames
  (`SoftwareSampler::LODBias`).
- **Shading**: the scene shader (`Scene.frag`) in fixed point, 2.14 in lanes of 16 bits, a quad
  of 2x2 pixels at a time, through `SoftwareSimd.h`'s SSE2, NEON or plain loops, which give the
  same bits (the device's files built without fused multiply-adds, `-ffp-contract=off`); the
  alpha test and the blends in float, on the frame's bytes over 255. One template
  (`SoftwareDrawPrimitive`) makes every pipeline: with each of a draw's flags read as it draws,
  the general one; with them fixed as it is compiled, the specialized ones a draw's flags select
  (`SoftwarePipelines::Select`), the same bits by construction. A float transcription of the
  shader and the blends (`SoftwareReference`) is the oracle they are proven against: each draw
  within a level of it, two draws over one pixel at most two (`DXE_SW_REFERENCE=check`).
- **The present**: the frame through the gamma ramp, a table of 256 levels a channel of the
  present shader's gamma (XOpenGL's mode by each pixel's largest channel too): each tile's last
  step where the window is the frame's size, else the frame scaled to the window and letterboxed
  below a render scale of 1 as the Vulkan device's present, bilinear and held at the edges, the
  window's rows shared out in bands. Then it is handed to the window (`Widget::PresentBitmap`):
  SDL's renderer, which waits for vsync with `UseVSync`; X11's image in memory shared with the
  server (MIT-SHM) where the server takes it, else `XPutImage`, and Win32's `SetDIBitsToDevice`,
  outside `WM_PAINT` too, which do not wait.
- **`ReadPixels`** gives the frame before the ramp, but with `GammaCorrectScreenshots`.

### Script VM

DXEngine reworks Surreal Engine's `ExpressionEvaluator` and `Frame::Run` for speed: no
`dynamic_cast` or heap allocation on the call path (parameters from `Properties`, functions and
events found through `UClass::VirtualFunctionCache`); each expression node classified once
(`ExpressionEvaluator::Classify`, `Expression::TypedKind`), so conditions, `&&` and `||`, the
commonest operators (`UFunction::FastOperator`) and assignments to plain variables evaluate as
plain values, not through an `ExpressionValue`; conditions, jumps, plain assignments, calls,
`return;` and a foreach's next pass run by `Frame::Run` in place, without an
`ExpressionEvalResult` each (`Frame::ClassifyStatement`). A call leaving out an optional struct
or array argument does not copy it, where Surreal Engine crashes.

Script runs under the original's guards ([stops the game](NATIVES.md#stops-the-game),
[a singular function](NATIVES.md#small)), on from the engine's start (`Frame::Guards`): the
jumps counted by `Frame::CountJump` from each level tick and each map load but a save's
(`Frame::InitRunaway`), the nesting and each object's singular mark kept by `Frame::GuardedRun`
in `Frame::CallScript`.

### Game tick

- **Deus Ex's level tick is the original's**: the actor list in the original's order, static
  actors first and never ticked, an actor waiting for its owner's tick (`ULevel::SortActors`,
  `ULevel::TickDeusEx`, `UActor::StartTick`;
  [housekeeping](NATIVES.md#housekeeping-not-seen-directly)).
- **The actor iterators** find a class's actors from an index, not a scan of the level.
- **Collision**: a ray trace hands each BSP child only its part of the segment and tests a
  polygon's plane before its vertex count and surface; a line tests the actors of only the
  cells it crosses, however long; a box of 100 cells or more on an axis tests every actor in
  the hash instead; each cell's actors sit in an open-addressed table (`CellTable`), where
  Surreal Engine keeps a `std::unordered_map` of `std::list`s.
- **Deus Ex's traces, moves and physics are `Engine.dll`'s**, in
  `DXEngine/Collision/TopLevel/TraceTest.cpp`, `UActor_Phys*.cpp` and `UPawn_Tick.cpp`: its
  backoffs; the movers a line meets where the level's BSP last filed them, filed by
  `CollisionSystem::FileMovers` at a map's load, but a dedicated server's, and a save, and
  `RefileMovers` each frame drawn, in a `CellTable` of their own; a mover's move and turn as
  one (`UActor::MoveBrush`); `FindSpot` and encroachment, moves held off what they meet as
  `ULevel::MoveActor` holds them, landing as `processLanded`;
  walking, falling, swimming, flying, rolling, a player's ladders, the speed pawns move at and
  how they turn, as `physWalking`, `physFalling`, `physSwimming`, `physFlying`, `physRolling`,
  `calcVelocity` and `physicsRotation`, a fall ending its frame where it lands
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)).
- **Each physics takes the whole frame**, as the original's `performPhysics` gives it once a
  tick (`UActor::TickPhysics`): a pawn walks, falls, swims, flies, follows a path or climbs, any
  other actor falls, flies as a projectile, rolls, follows a path, moves as a brush or trails its
  owner, each stepping the frame as the original's does (`UPawn::DeusExPhysWalking`,
  `UActor::DeusExTickFalling`, `UActor::TickRolling`, `UActor::TickProjectile`,
  `UActor::TickInterpolating`, `UActor::TickMovingBrush`, `UActor::TickTrailer`), a fall a walk
  or a swim hands over going on with its count of steps; so its speed, its slides, where it
  comes to rest and the move it hands on are the original's at the same frame rate, but for a
  swim out from under the water at some 1,000
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). Then the
  turn by `RotationRate`, once a frame (`UActor::TickRotating`).

### Objects and memory

`DXEngine/GC`: a mark and sweep from roots, as UE1's collector
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#garbage-collection)).
A collection runs at the end of the frame, with no script running (`Engine::CollectGarbage`, in
`EngineGC.cpp`), when asked for: by every map load but Entry's, by the console's `obj garbage`,
by 2048 destroyed actors let go since the last one
([destroyed actors](NATIVES.md#housekeeping-not-seen-directly)), and every `DXE_GC_STRESS`
frames. A map load's and the console's do everything; the backlog's and the stress runs' mark
and sweep only, no level having departed since the last map load's. It logs the original's
lines (`Collecting garbage`, `Purging garbage`, `Garbage: objects: ...; refs: ...`, the refs
those followed, not the ones to code) with the milliseconds it took, the resident memory before
and after (on Windows the working set), what it freed by kind and the references it made None
by holder, and with glibc a map load's gives the freed memory back to the system
(`malloc_trim`).

- **Roots** (`Engine::MarkRoots`, in `EngineGC.cpp`): the engine's subsystems, levels and
  objects; the net drivers' players, channels and package maps; the natives; every export of a
  save info. Any export of a package but code stays only while something reaches it, and is
  loaded from its file again when next asked for. A map's or a save's package is not a root:
  what its level reaches stays.
- **Code** (a `UField` in a package) is always kept. A collection looks over the objects made
  since the last one and moves the code among them to a permanent list
  (`GCAllocation::permanent`), which the marker never visits as a target. A reference code
  holds to code -- its class, its chain of fields, the struct, class or enum a property names --
  is not handed over from code at all (`GCMarker::MarkCode`), and a struct's properties, a
  state's functions and a bytecode's constants hand over only the entries that are not
  permanent (a field made at run time in no package, `Object`'s native properties). What code
  refers to beyond itself is walked each collection for a class, whose defaults scripts write
  (`IsGCMutableCode`); the rest of the code refers to what it referred to when it was loaded,
  so it is walked only when code was loaded or found since the last such walk
  (`GC::CodeChanged`, from `UField::Load`), and the collectable objects and names it reached
  then are kept from a list until the next.
- **What an object holds**: its class, package, delay load and state frame, and the references
  its properties hold (`UStruct::RefProps`); a native class marks its own members in its `Mark`.
  An object reached only through its package keeps the package's file and tables, not its other
  objects.
- **Where objects are made**: each in the package of the level it belongs to, as the original's:
  a spawned actor in its level's (the Entry level's own pawn in Entry's), the flag base in the
  player's, and what travels with the player there too, held by the player and named by its
  class's count; the event manager in its `LevelInfo`'s; an object a script makes with `new` in
  its outer's (`new(Self)` on the player: its goals, notes and key infos), or with none in the
  transient package. A conversation's camera and flag refs are in the conversation's package,
  which outlives any level, as the original's are in `DeusEx`'s. A package played as a level is
  tagged so (`Package::IsLevel`).
- **A sound** is decoded once, when first played, for the audio device, which keeps a copy of
  its own (the mixer's 16-bit one, OpenAL's buffer); the sound keeps only its length, channels,
  rate and loop. Its lip-sync shapes decode it again once, into a buffer let go after.
- **Saving a level** writes what the level reaches (`PackageWriter`), never the rest of its
  file: an export the collector freed is not loaded back to be saved. An object of a transient
  class is not written either ([as the original's](NATIVES.md#saving-loading-and-travel)).
- **Elimination**: a reference to an object flagged `EliminateObject` is made None where the
  marker finds it (`GCMarker::Mark`), as the original's. One held where it may not be written
  (`MarkConst`: the subsystems, the net layer, what code names) keeps it, and the log says so. A
  collection flags every object of a level left behind (a package tagged as a level that is
  neither the current one nor Entry's) and, at a map load, the new level's actors that are not
  in its `Actors`, as the original's. `Object.CriticalDelete` flags the object it is given.
- **The weak holders** let go of the dying before anything is freed (`GC::IsDying`): the camera
  actor, the coronas and iterator actors, the lights the light system lists, the audio device's
  sound numbers and reverb zone, the open package files.
- **The sweep** tells each dying object first, all of them still allocated (`OnGCDestroy`): the
  properties with something to destruct are destructed (`UStruct::DestructProps`) and its
  package's export slot emptied, so a later reference loads it from its file again; a sound
  leaves the audio device, a decal its BSP nodes, a package closes its file, a freed texture has
  the render device flushed (it caches textures by address) if anything was drawn since its last
  flush. Then each is freed.
- **The names** go after the objects: a name the engine makes for an object
  (`NameString::Collectable`, through `Package::MakeUniqueObjectName`) is deleted when no live
  object holds it, its slot taken by the next name made. Each object marks its names as the
  collection reaches it (`GCObject::GCMarkNames`: its own name, its properties', its state
  frame's locals), the code as it is walked (above); an object made while the collection ran
  has them marked over every object after. A name looked up by its spelling is kept from
  then on, so no other holder can see its index taken. A cache keyed by name indexes keeps no
  collectable name (`FindScriptFunction`).

### Gameplay

What else Deus Ex needs is described by feature in [`NATIVES.md`](NATIVES.md). Where most of
its code is:

- **The path search and reachability tests**: the script's `FindPathToward`, `FindPathTo` and
  the `pointReachable`, `actorReachable` and `Reachable` they ask are `Engine.dll`'s, walking
  its own moves (`UPawn_Path.cpp`, `UPawn_ReachDeusEx.cpp`); `RouteCache` stays empty, as the
  original never fills it ([moving](NATIVES.md#moving-wandering-and-tactical-movement)).
  Surreal Engine's own search serves only `FindRandomDest` and `FindBestInventoryPath`, which no
  stock script calls ([mods](NATIVES.md#mods)).
- **The audio mixer**: `GalaxyMixer` (`DXEngine/Audio/GalaxyMixer.cpp`) mixes Deus Ex's sounds
  as `Galaxy.dll` does (pan, fall-off, sliders, resampling, its reverb) and streams them through
  one OpenAL source at `OutputRate`; the music streams through one of its own, from a thread of
  its own, where a song that fails stops and the game goes on, the main thread logging why
  (`Music: the song stopped:`). DXEngine's audio device adds `SpeechVolume` and
  `DopplerSpeed` to its ini section, and keeps Deus Ex's channel count under Galaxy's name,
  `EffectsChannels` ([sound](NATIVES.md#sound)). DXEngine's render device keeps `DetailTextures`
  beside Surreal Engine's keys; on a first run (no `DXE-DeusEx.ini`, or an `--ini` without the
  engine's sections) it takes them from the section of the device the game's ini names
  (`GameRenderDevice`) ([the menus' settings](NATIVES.md#the-menus-settings)). Who hears a sound
  an actor plays, a remote player through its client, is `UActor::HearSound` and
  `CheckHearSound` (`UActor.cpp`), from Deus Ex's `PlaySound`, `PlayOwnedSound` and
  `DemoPlaySound` natives (`NActor.cpp`).
- **The network driver**: `DXEngine/Network/` is DXEngine's own, the original's protocol
  over UDP (`NetDriver`, channels, the package map, replication by the original's lists) with a
  client that joins the original's servers, a listen and a dedicated server, downloads and
  server travel; `IpDrv`'s `InternetLink`, `TcpLink` and `UdpLink` are the original's
  ([multiplayer](NATIVES.md#multiplayer)).
