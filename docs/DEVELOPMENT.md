# Working on the engine

Driving both engines with the dxcap harness, debugging a crash, proving an engine change. This
clone is `DXEngine/` in the parent folder of the workspace's repositories (`<parent>` below),
beside the game (`gamefiles/`), the SDK (`reference/`) and the builds (`build/`). A `tools/`
command runs from this clone's root, anything else from the parent folder. How to work in the
workspace:
[its `DEVELOPMENT.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md).
Keeping, running and profiling DXEngine, and what it changes: [`ENGINE.md`](ENGINE.md).

## When a fidelity change is done

A fidelity change is done when `tools/natives_audit.py --runs <logs>` shows no stub firing
on its path in the maps that reached one, a side-by-side
[scripted run](#scripted-runs-of-both-engines) against the original proves it, and
[`NATIVES.md`](NATIVES.md) says what differs. The audit reads `AICanSmell` "empty": the
original's answer is always 0 too. The runs say whether a stub fired.

## Temporary debug hooks

A temporary debug hook (a screenshot from the renderer, extra logging) carries a
`TEMPORARY DEBUG TOOL` comment. Remove it before committing by replacing its exact text, never by
a looser scripted cut: a cut can take live code with it and still compile. Prove the change with
a run of 60 s or more that looks at what was drawn, not only at the log:
`tools/dxcap.sh prove <map>`.

## Scripted runs of both engines

`tools/dxcap.sh` runs the original game (under Proton's `wine`, in the development container)
and DXEngine alike, each driven by a console class of the DXCapture package: UnrealScript in
`tools/dxcap/Classes`, compiled by the SDK's `UCC.exe` (`reference/ReleaseSDK1112f`) into
`build/dxcap`, with test sounds and textures of kinds the game ships none
of, which `tools/dxcap/testsounds.py` and `testtextures.py` write for it
(their headers name them).

```sh
tools/dxcap.sh setup                          # once, and again after the parent folder moves
tools/dxcap.sh compile                        # after setup, and after changing tools/dxcap
tools/dxcap.sh prove 01_NYC_UNATCOIsland.dx   # DXEngine: shots at 20 s and 60 s, exit at 65 s
tools/dxcap.sh dxe <console> <map> [<secs>]   # DXEngine, straight into <map> or a server's address; - the stock console
tools/dxcap.sh original <console> [<secs>]    # the original, from its menu map (or DXCAP_START's URL)
tools/dxcap.sh live <address> [<secs>]        # DXEngine on a live server (below)
tools/dxcap.sh dedicated <original|dxe> <actor> <map> [<secs>]  # either engine as a dedicated server
```

A run stops after `<secs>`, 120 by default. `prove` fails on an unclean exit or a missing or
nearly black shot, and lists the log's errors and `Unimplemented` lines. `setup` links the
game's files into `build/dxcap` by absolute path, and after a move `compile` does not notice they
are gone.

- **The ini.** Each run gets a private ini made from the game's `DeusEx.ini` and `User.ini`: the
  console class, the run's window size, `FirstRun` 1100 (an install never started keeps 0, and
  the original's launcher then opens its first-run wizard), 333networks' master server for the
  Join Internet screen (the game names GameSpy's, which is closed), no uplink, `OpenGLDrv` for
  the original and `D3DDrv` for DXEngine, whose device is the settings file's, never a game
  ini's `SoftDrv` ([which device draws](ENGINE.md#rendering)). Both engines take the game's
  settings from it; the game's inis are never written.
- **`Settings.json`.** DXEngine's device and its settings
  ([the keys](ENGINE.md#settings-and-environment)) come from
  `~/.config/DXEngine/Settings.json` (a Windows build's, the prefix's `%LOCALAPPDATA%\DXEngine`),
  an input of every dxe run that no run's ini changes, or from the file `DXCAP_SETTINGS` names.
  Without one DXEngine runs on Vulkan with 4x MSAA. `tools/dxcap` has one for each device a run
  compares, each without MSAA or vsync: `perf-settings.json` (GL), `vulkan-settings.json`,
  `software-settings.json`, and Mesa's CPU drivers' (`CPUDriver`), `lavapipe-settings.json` and
  `llvmpipe-settings.json`.
- **Results.** A run's log (`engine.log` or `DeusEx.log`), shots and recording land in
  `build/dxcap/runs/<engine>-<console>-<date>-<time>/`.

| Variable | Effect |
|---|---|
| `DX_ROOT=<dir>` | the parent folder, if not the one this clone is in |
| `DXCAP_DISPLAY=<display>` | another X display than `$DISPLAY`, the desktop's ([the display](#the-display)) |
| `DXCAP_WINDOW=<w>x<h>` | both engines' window that size ([the display](#the-display)): at 4:3 their views are the same ([the view's width](NATIVES.md#the-views-width)) |
| `DXCAP_AUDIO=1` | DXEngine with real audio; it is silent otherwise |
| `DXCAP_RECORD=1` | either engine's sound recorded into the run's `audio.wav` (a private null sink, `PULSE_SINK`, and `parecord`); the music off |
| `DXCAP_MUSIC=1` | a `DXCAP_RECORD=1` recording with the music on, at the game's `MusicVolume` |
| `DXCAP_RENDERER=D3D` | the original through `D3DDrv`, the game's own renderer, not `OpenGLDrv` |
| `DXCAP_PREFIX=<dir>` | another Wine prefix than `build/dxcap/prefix` |
| `DXCAP_PROTON=<name>` | another Proton build in `~/.local/share/Steam/compatibilitytools.d` than `Proton-CachyOS Latest` |
| `DXCAP_CONFIG="<key>=<value> ..."` | those keys in the run's console class's section of the ini, as `DXCAP_MISSION_MAP` gives `TargetMap` (`LightConsole`'s `Effect`, `Pick`, `LightName`, `View`, `Aim`, `ShotAt`, `Macro`, `NoDetail`, `Spawn`, `SpawnAt`, `SpawnType`; `CoronaConsole`'s `Coronas`; `CoronaSkinConsole`'s `SkinName`, `SkinClass`, `Rate`; `GlassConsole`'s `Pick`, `GlassName`, `View`, `Behind`, `Flip`, `Destroy`; `DebrisConsole`'s `Dirs`, `Keep`, `TrashX`, `TrashY`, `TrashZ`, `TrashRadius`, `TrashHeight`, `Steps`, `ContainerName`; `FlushConsole`'s `Pick`, `DoorName`, `View`, `Angle`; `MirrorConsole`'s `MarkTime`, `ShinyOff`, `Straddle`; `FaceConsole`'s `MeshOf`, `Ahead`, `Distance`, `Turn`, `Raise`; `MeshConsole`'s `LODBias`, `NoDetail`, `Enviro`; `LaserConsole`'s `NoDetail`; `SpriteConsole`'s `Styles`, `NoParticles`; `DrugConsole`'s `HudWindows`; `CanisterConsole`'s `Distances`, `MarkTime`; `LoopConsole`'s `Jumps`, `LoadAfter`; `CallConsole`'s `Depth`; `ReturnConsole`'s `Items`; `SaveConsole`'s, `LoadConsole`'s and `StartConsole`'s `Via`; `SaveConsole`'s `Fresh`; `LoadConsole`'s `Open`; `ThemeConsole`'s `Direct`; `CensusConsole`'s `From`, `Wait`, `Load`, `Seconds`, `Notify`, `Open`; `DetailConsole`'s `Heights`; `MoveConsole`'s `Watch`, `WatchFrom`, `WatchTo`; `SwimConsole`'s `Only`; `MultiEditConsole`'s `Keys`; `LipConsole`'s `Beep`, `Speech`; `OverlayConsole`'s `Wire`, `Solid`; `AmbientConsole`'s `Volume`, `Factor`; `MoverLightConsole`'s `DoorName`, `Distance`, `Ahead`; `GlowConsole`'s `Ahead`, `Dilation`, `Shots`, `Glow`; `OpsConsole`'s `ListRadius`; `IntroConsole`'s `First`, `Every`, `Shots`, `FrameFrom`, `FrameTo`; `IntroDoorConsole`'s `First`, `Every`, `Shots`; `AxisConsole`'s `Axis`, `Ticks`; `URLConsole`'s `Bare`; `ServeConsole`'s `Kick`; `SleepConsole`'s `LoadByActor`; `LootConsole`'s `Hover`) |
| `DXCAP_USERINI=<file>` | a run's `User.ini` made from that file, in place of the game's (`IniConsole`'s keys given twice) |
| `DXCAP_USERINI_FIRST=1` | the run's inis named `USERINI=` then `INI=`: on the original's command line, and on DXEngine's by `--cmdline` in place of `--ini` and `--userini` ([the command line](NATIVES.md#the-command-line)) |
| `DXCAP_FLAGS="<flags>"` | the original's flags (`-norunaway`) on its command line, and on DXEngine's by `--cmdline` ([the command line](NATIVES.md#the-command-line)) |
| `DXCAP_INI_CASE=1` | the run's `INI=` and `USERINI=` with their names in capitals, the files on disk as they are: on the original's command line, and on DXEngine's by `--cmdline` ([the command line](NATIVES.md#the-command-line)) |
| `DXCAP_MISSION_MAP=<map>` | the map a run's console travels to, its `TargetMap` (`MissionConsole`'s, `StaticConsole`'s, `WadeConsole`'s, `ScreenConsole`'s, `PerfConsole`'s and `PerfTurnConsole`'s, `EndgameConsole`'s, `VisibleConsole`'s, `MoverConsole`'s, `MoverLightConsole`'s, `GlowConsole`'s, `FogConsole`'s, `LightConsole`'s, `GlassConsole`'s, `DebrisConsole`'s, `FlushConsole`'s, `WallConsole`'s, `MirrorConsole`'s, `DoorConsole`'s, `OpeningConsole`'s, `MatrixConsole`'s, `IntroDoorConsole`'s): an original run starts at its menu map, a dxe run at the map it is given |
| `DXCAP_START=<url>` | an original run started there, not at its menu map: its command line's first word, with `-hax0r` ([running the original](#running-the-original)); `?loadgame=9` for `StartConsole`, and for `CensusConsole` with `Load=False` |
| `DXCAP_SETTINGS=<file>` | a dxe run's `Settings.json`, in place of `~/.config/DXEngine`'s (through a `HOME` of the run's own; the caches stay the usual ones); a Windows build's through a `--userdir` of the run's own ([a Windows build](#a-windows-build)) |
| `DXCAP_USERDIR=<dir>` | a dxe run's `--userdir`: its `Settings.json`, log, shots and a class's own config file there ([running it](ENGINE.md#running-it)); not with `DXCAP_SETTINGS`, the folder's own `Settings.json` being the run's |
| `DXCAP_NOVSYNC=1` | Mesa's GL draws either engine without waiting for the display (`vblank_mode=0`): the original's `OpenGLDrv` asks for no swap interval and gets the driver's vsync |
| `DXCAP_FPS=<n>` | either engine held to n frames a second: MangoHud's limiter, its display off (`mangohud`, and `lib32-mangohud` for the original's 32-bit process); not the software device in its X11 window, whose present (`XShmPutImage`) MangoHud does not hook |
| `DXCAP_PERF=1` | the engine's main thread sampled by perf into the run's `perf.data`, on the wall clock, its cycles and instructions counted each 100 ms (`stat.csv`), with the process's `maps.txt` ([measuring both engines](#measuring-both-engines)) |
| `DXCAP_TIMELINE=<file>` | a `live` run's timeline, not `JoinConsole`'s walk; a `dxe` run's [timeline](#timelines) |
| `DXCAP_ENGINE=<binary>` | a dxe run on that build, not linux-x86_64's (an [ASan build](#crashes)); a Windows build's `DXEngine.exe` under the original's `wine` ([a Windows build](#a-windows-build)) |
| `DXCAP_BACKEND=SDL3`, `SDL2`, `Win32` | a dxe run's window DXWidgets' SDL3 or SDL2 one, the launchers', not its X11 one: SDL3 on linux-x86_64's build, SDL2 on one built with it (`DXCAP_ENGINE`); the keys of a timeline's `keydown` and `keyup` go through its own events, its `pad` and `padaxis` through its virtual pad. A Windows build's is SDL3's, or with `Win32` Windows' own ([a Windows build](#a-windows-build)) |
| `DXCAP_DXVK=1` | a Windows build's D3D11 drawn through Proton's DXVK, in place of Wine's own ([a Windows build](#a-windows-build)) |
| `DXCAP_MEMLOG=1` | either engine's resident memory each second in the run's `memlog.txt`: seconds since its process started, MB |
| `DXCAP_UPLINK=<host>:<port>` | the server announces itself to that master (`DoUplink`); otherwise to none |
| `DXCAP_PORT=<port>` | the server listens there (`[URL] Port`), not on 7790: for `netrelay.py` between it and a client ([net tests](#net-tests)) |
| `DXCAP_STATS=<password>` | on both runs: the server logs world stats (`bWorldLog`); the player holds that world stats password, so its login carries the checksum (a dxe server logs the login's URL: `login request`) |
| `DXCAP_PRECACHE=1` | the run's render device's `UsePrecache` on (`D3DDrv`'s): the frame after each map's load shows `PRECACHING` (the original's with `DXCAP_RENDERER=D3D`) |
| `DXCAP_CACHEEXT=<ext>` | the run's `[Core.System]` `CacheExt`, the extension its download cache looks packages up by |
| `DXCAP_SERVERPKGS=<dir>` | the server offers the packages in `<dir>` (`ServerPackages`) for a client to download |

### The console classes

What each console does is its class's header comment in `tools/dxcap/Classes`; the table
names it. A dedicated server runs no console: `dedicated` makes the run's class, an actor, the
first of the server's `ServerActors`, which drives the run and ends it. The scripts are in
`tools/dxcap/`. *Both*: run in each engine and compare, side by side where no script reads
the run. *Either*: one engine on each side of a pair. *Original*: the original alone, for its
launcher's part, which DXEngine leaves to the launchers.

| Console | Engines | Drives and logs | Prefix | Read with |
|---|---|---|---|---|
| `ProveConsole` | dxe | the URL's map: two shots, an exit | `DXPROVE:` | `prove` |
| `CaptureConsole` | both | tripwires, coronas, comment-jump conversations | `DXCAP:` |  |
| `ViewConsole` | both | the weapon in view; the level's light | `DXVIEW:` |  |
| `CoronaConsole` | both | lamp coronas; a movable corona light of its own; with `Coronas=False` the render device's Coronas set off first | `DXCORONA:` |  |
| `CoronaSkinConsole` | both | Liberty Island's `Light169` given a skin that changes (`SkinName`, `SkinClass`; `Rate` its frame rate): its corona shot three times 4 s apart, the skin's current frame logged at each | `DXSKIN:` | the lines diffed; `shots.py --region` of the corona at `DXCAP_WINDOW=1024x768`, the original's shots named by their marks (1 to 3) |
| `LaserConsole` | both | a laser fence's beams and its trigger's actors; the beams with a still texture, unlit and lit; the detail textures off (`NoDetail`) | `DXLASER:` | `laser.py <original run> <dxe run>`; `shots.py --shift`; at `DXCAP_WINDOW=1024x768` with `NoDetail=True` ([shots](#shots)) |
| `ParticleConsole` | both | smoke and puffs on the pier, in marked shots | `DXPART:` | `shots.py --region` over the smoke's column, at `DXCAP_WINDOW=1024x768` |
| `FaceConsole` | both | a character's mesh close up (`MeshOf`, `Ahead`, `Distance`), turned `Turn` degrees from facing the view, seen from `Raise` units over its eyes | `DXFACE:` | `shots.py <original run> <dxe run>` |
| `MeshConsole` | both | meshes lit on the pier (`LODBias`, `NoDetail`, `Enviro`) | `DXMESH:` | `shots.py` |
| `SpriteConsole` | both | an icon whose colour 0 is magenta on the pier: a sprite of each style, and a generator's particles of it drawn masked (`Styles`, `NoParticles`) | `DXSPRITE:` | `shots.py --region`, at `DXCAP_WINDOW=1024x768` |
| `OnceConsole` | both | a sprite animated once (`DT_SpriteAnimOnce`) at Liberty Island's start, a chain of three noise frames, its life held at six values of its class's 8 s, a marked shot of each | `DXONCE:` | `shots.py --region`, at `DXCAP_WINDOW=1024x768` |
| `TileConsole` | both | tiles and text over a grey box, on the canvas and in a window, at the island's start: two textures with colour 0 drawn modulated, a masked one drawn normal in the window, the menu's TrueType font and FontMenuHeaders, the canvas's clipped and wrapped text translucent; the widths FontMenuHeaders gives a space and Latin-1 letters past ASCII, logged | `DXTILE:` | `shots.py --region --block 4` |
| `DetailConsole` | dxe | a detail texture's bands: the pier's planks looked straight down at from each of `Heights` (10 to 400 units, either side of each bound), the detail textures on and off | `DXDETAIL:` | `detail.py <run>`, at `DXCAP_WINDOW=1024x768` |
| `StepConsole` | both | an animated texture with a MaxFrameRate drawn on the canvas every frame, every other and every ninth, 120 frames each: its steps at each cadence | `DXSTEP:` | the steps at each cadence, both at `DXCAP_FPS=60` |
| `CanisterConsole` | both | an augmentation canister and a scorch mark on the pier, from 120 to 1200 units, marked shots (`Distances`, `MarkTime`) | `DXCAN:` | the canister's box cut from each shot, at `DXCAP_WINDOW=1024x768` |
| `LightConsole` | both | a light of one effect on `TargetMap` (`Effect`, `Pick`, `LightName`, `View`, `Aim`, `ShotAt`), or one made over the floor ahead for an effect no stock light shows (`Spawn`, its height; `SpawnAt`, `SpawnType`); the spot's texture as its own macro texture (`Macro`); the detail textures off (`NoDetail`) | `DXLIGHT:` | `shots.py`, at `DXCAP_WINDOW=1024x768`; a made light against `D3DDrv` at the moment its map holds still (NATIVES.md, lighting) |
| `FlashConsole` | both | the screen flash | `DXFLASH:` | DXEngine at `DXCAP_FPS=60`: above some 105 frames a second the game's `ViewFlash`, which drops a fog under 0.019 each frame, never builds the glow's 0.2 |
| `DyingConsole` | both | the player's death: the flash each second, shots of the spiral round the carcass | `DXDYING:` | `shots.py`, both at `DXCAP_FPS=60` |
| `DrugConsole` | both | the drugged view: the HUD's `DrunkFX` background, modulated; the HUD's windows drawn over it as in play (`HudWindows`) | `DXDRUG:` | `shots.py` |
| `ThemeConsole` | both | the colour themes through a new game's travels; the InfoLink's text typed in the HUD theme (`Direct`: the island opened, no travels) | `DXTHEME:` | the original with `Direct=True` |
| `GlassConsole` | both | a glass mover on `TargetMap`, or any mover named `GlassName`, and what is behind it (`Pick`, `GlassName`, `View`, `Behind`; `Flip`: seen from the side with less room), then hidden, then broken (`Destroy`: destroyed); `TraceTexture`'s hits through it | `DXGLASS:` | `shots.py`; the hits' lines diffed |
| `DebrisConsole` | both | on `TargetMap` (Liberty Island), 6 s in: each glass mover's fragment spawned where `BlowItUp` spawns one, `FragmentSpread` from the brush's origin toward each of `Dirs` (128) fixed directions, the glass whole; with `TrashRadius`, `TrashPaper` on a `Steps`-cubed grid (5) over the box `Containers.Destroyed` draws from, round `TrashX`, `TrashY`, `TrashZ` (`TrashHeight`); each spot refused or placed, the piece destroyed at once; the `TrashPaper` already there and whether `ContainerName` is; then each glass broken (`Keep`: not) and its pieces counted, and all again 2 s on | `DXDEBRIS:` | the spots' lines diffed |
| `FlushConsole` | both | a door on `TargetMap` slid half into the flat wall beside it, its face in the wall's plane (`Pick`, `DoorName`, `View`, `Angle`) | `DXFLUSH:` | `shots.py` |
| `MirrorConsole` | both | mirrors from fixed views on `TargetMap`: Liberty Island's UNATCO HQ lobby floor, turned and stepped across, a mannequin on it, and the island's wall mirrors; UNATCO HQ's glass mirrors and floor; the training map's floor and the mirror strip over it; the Paris club's two facing glass mirrors; marked shots (`ShinyOff`: the render device's `ShinySurfaces` off; `Straddle`: the mannequin sunk half its height into the floor, across the mirror's plane) | `DXMIRROR:` | `shots.py --region`, at `DXCAP_WINDOW=1024x768` |
| `FullscreenConsole` | dxe | the window full screen and back by `togglefullscreen`, a shot in each with the root's size: the device's frame at each size | `DXFULL:` | `shots.py` |
| `HeldConsole` | both | what a pawn holds | `DXHELD:` |  |
| `WallConsole` | both | the player's assault rifle against a wall of `TargetMap` (UNATCO HQ) | `DXWALL:` | `shots.py`, at `DXCAP_WINDOW=1024x768` |
| `DeathConsole` | both | an NPC's death and its carcass | `DXDEATH:` |  |
| `DestroyConsole` | both | an actor's destruction, event by event: in a state, owned, standing on one actor, stood on by another, owning one and touched by one, each event logged with its `bDeleteMe` | `DXDESTROY:` | the two logs diffed |
| `TeleportConsole` | both | a SetLocation onto an actor and away again: the touches, the rider's base change and where each stood at that moment (`CapTeleported`) | `DXTELE:` | the two logs' lines diffed |
| `BurnConsole` | both | a breakable crate spawned ahead of the player on the island and set alight: each second the crates and the Fires in the level, and how many stand on the crate, until it burns out and is destroyed | `DXBURN:` | the two logs' lines |
| `AIConsole` | both | every NPC's state, orders, enemy and render time; the camera | `DXAI:`, `DXAISTATE:` |  |
| `SitConsole` | both | Private Lloyd's way into his chair and his sitting, in view: state, animation, `bSitting`, place; two marked shots | `DXSIT:` | the two logs' lines |
| `OpeningConsole` | both | `TargetMap`'s opening view, held, the HUD off: two marked shots | `DXOPEN:` | `shots.py`, at `DXCAP_WINDOW=1024x768` |
| `StaticConsole` | both | `TargetMap`'s static actors at 5 s and 15 s | `DXSTATIC:` |  |
| `MoveConsole` | both | every pawn's moves, every 2 s for 120 s; with `Watch`, one pawn each tick (`WatchFrom`, `WatchTo`) | `DXMOVE:` | `move.py <original run> <dxe run>`; `move.py --pawn <name>`, where two runs' paths part, by place |
| `ReachConsole` | both | the reach tests, each pawn asked from its home spot | `DXREACH:` |  |
| `StrafeConsole` | both | two plain pawns' `StrafeTo` of 200 units, one aloft with no physics, one landed and walking: each one's MoveTimer, DesiredSpeed and physics at its first ticks, and the time its strafe took (`CapStrafer`) | `DXSTRAFE:` | each MoveTimer at the strafe's start, within a tick; the walker's strafe over, the aloft one's never |
| `TweenConsole` | both | a tarp's `PlayAnim`, `LoopAnim` and `TweenAnim` with and without a tween time: from nothing playing, after a tween, a loop going on, after a velocity-scaled rate, a one-frame sequence, a MinRate; the frame, rates, tween rate, old rate, least rate and flags after each | `DXTWEEN:` | every line, exactly |
| `TouchConsole` | both | a trigger's four touch slots filled by crates held still, then the player put in it: each Touch and UnTouch the trigger gets with its slots, and the crates' and the player's slots after (`CapTrigger`) | `DXTOUCH:` | every line, exactly |
| `SwitchConsole` | both | on the pier, a spawned lamp shot on and switched off, then a spawned spotlight shot aimed north and turned east (`CapLamp`, `CapSpot`) | `DXSWITCH:` | shot 2 against 1 (the lamp's pool gone), 4 against 3 (the beam moved) |
| `FogConsole` | both | `TargetMap`'s start (02_NYC_Underground's, in a fog zone): a troop and a large crate spawned 220 units ahead, two shots, the two meshes' places at each | `DXFOG:` | the crate against the floor and walls beside it (the troop wanders where its state takes it) |
| `ToolConsole` | both | the EditFlags cheat screen, a ToolWindow framed by a BorderWindow of nine textures, opened and shot | `DXTOOL:` | the tool window: its frame, title bar, label, list field and buttons (the pointer differs, DXEngine's at the screen's middle) |
| `ResizeConsole` | dxe | a BorderWindow of the tool window's textures in the EditFlags cheat screen, resizing on (`EnableResizing`, which no script of the game turns on): its place and size every half second | `DXRESIZE:` | with `tools/dxcap/timelines/resize-drags.txt`: 200 by 150 at 20, 60, then 250 by 180 by the corner's drag, then at 60, 80 by the right button's; the shots' cursors the corner's diagonal, the arrow inside, the move one |
| `TextBlockConsole` | both | three large text windows of two lines on the menu map's root, aligned to the middle across and to the top, the middle and the bottom, shot | `DXTEXTBLOCK:` | each box's lit rows and columns, at 4:3 (DXEngine's shot through its gamma) |
| `LogoConsole` | both | the ShowQuotesWindow cheat's quotes window, its ten spinning logos 1 s and 3 s after: each one's `winParent`, that parent's size, whether the logo lies within it | `DXLOGO:` | the two logs' lines |
| `FocusConsole` | both | the inventory screen with a lockpick selected, so a button has the focus: the focus window after the button is made insensitive and sensitive again, and after its parent is; then a modal of its own with a clip window scrolled so that its first button lies above the view, then none in view: the first focus, Tab and Shift+Tab from each place, each button's clip rectangle | `DXFOCUS:` | every line |
| `LoginConsole` | both | on Liberty Island, the first personal computer's login screen opened as the player opens it, a name and a password put in its fields as typing puts them, the window tree; two marked shots | `DXLOGIN:` | each field's first letter, its lit pixels, and its frame's outline, at `DXCAP_WINDOW=1280x960` (the original with `DXCAP_RENDERER=D3D`) |
| `ListConsole` | both | the Load Game screen with two saves of the run's own, one named 50 characters long and one with a `|c` colour code, shot and the saves removed | `DXLIST:` | the list: the long name cut at its column, the code shown as typed, the fields' insets, the focus row's outline |
| `MatrixConsole` | both | the player's Sprite set as the Matrix cheat sets it, a Fire sprite spawned ahead, shot; with `TargetMap`, on that map, its surfaces in the Sprite too | `DXMATRIX:` | the sprite drawn as the viewer's Sprite texture; `shots.py` against the original's shot 1 |
| `WalkConsole` | both | RadiusActors, VisibleActors, ZoneActors, ChildActors and BasedActors over the run's own markers, each loop's body spawning one more in reach (`CapLoopMark`) | `DXWALK:` | every line |
| `NudgeConsole` | both | the script's `Move`: a crate 50 units over the floor moved 100 down, no move, 100 down from half a unit over the floor, 60 up: each answer and the crate's height over the floor after it | `DXNUDGE:` | every line |
| `BumpConsole` | both | a colliding actor's `Move` into the floor, partway and again from where it stopped, then up free: each answer and the actor's `BumpWall` events (`CapBumper`) | `DXBUMP:` | every line |
| `HopConsole` | both | an actor set 1 unit over the floor, rising at 30 units a second, falling through a frame held half a second so its fall steps 0.1 s and its first step turns over: its landing's velocity (`CapHopper`) | `DXHOP:` | every line |
| `AimConsole` | both | `PickTarget` and `PickAnyTarget`: two rats and two crates 300 units out along +X, 40 to either side (equal aims); a rat made no projectile target; a rat 2200 out | `DXAIM:` | every line |
| `SmoothConsole` | both | `MoveSmooth`: an actor 50 units over the floor moved 100 along and 100 down, 10 down from its rest, 60 up: each answer, the move made and the actor's `BumpWall` and `HitWall` events (`CapBumper`) | `DXSMOOTH:` | every line |
| `ShotConsole` | both | a bouncing projectile (`CapShot`) set flying at 400 units a second along and 400 down from 50 over the floor, through a frame held half a second: its wall hit and where the 0.4 s tick leaves it | `DXSHOT:` | every line |
| `PathConsole` | both | an actor on a ring of four `InterpolationPoint`s (`CapRider`), through frames held half a second: each 0.4 s tick's place, rotation and alpha, each point's `InterpolateEnd` | `DXPATH:` | every line |
| `IntroDoorConsole` | both | 00_Intro's opening as it plays: from `First` s (2) `Shots` marked shots (20) `Every` s (1), each with the view's place, facing and field of view and the trap door under Bob Page and Walton Simons | `DXINTRODOOR:` | the floor round the two men's feet, the original at `DXCAP_FPS=60`; the original's frame scaled 4/3 about its middle onto DXEngine's, whose letterboxed view is wider (bugs.md D.1) |
| `IntroConsole` | both | the menu map's flyby: its `InterpolationPoint`s, then a marked shot every `Every` s from `First` s, `Shots` of them, each with the view's place, the path's target, rate and alpha and the level's time; every tick from `FrameFrom` s to `FrameTo` s, with the wall's clock | `DXINTRO:` | the path's alpha at the same level time, the original held to 60 fps (`DXCAP_FPS=60`); uncapped it runs ahead (bugs.md D.17) |
| `SpinConsole` | both | four actors turning by their `RotationRate` in `PHYS_Rotating` (`CapSpinner`): a fixed direction, the shortest way to a desired rotation, a fixed direction to one, a rate of a whole turn; each 0.4 s tick's rotations, `EndedRotation` | `DXSPIN:` | every line |
| `TrailConsole` | both | four trailers of a crate (`CapTrail`, `SpriteTrail`): a sprite behind its facing by its `Mass`, one at its `PrePivot`, one turned with it, one against its velocity; the crate turned, moved, given a velocity and none | `DXTRAIL:` | every line |
| `ContactConsole` | both | `TouchingActors` over the player's touches: three colliding actors set onto it (`CapToucher`), listed by class, one destroyed and listed again | `DXCONTACT:` | every line |
| `TitleConsole` | both | the sizes two windows measure through `GetGC` outside a draw: the HUD's mission start text, then the main menu's title bar, each with its preferred size | `DXTITLE:` | every line |
| `TraceConsole` | both | `TraceTexture`, `TraceActors`, `Trace` and `FastTrace`; a rifle's shot, 48000 units long; boxes started inside cylinders | `DXTRACE:` |  |
| `ParabolicConsole` | both | `ParabolicTrace` from the player's eyes on the island, 30 degrees up at 400, 700 and 1000 units a second, bouncing and not, the player turned a quarter turn four times: each throw's landing and time | `DXPARA:` | the two logs' lines |
| `FloatConsole` | both | the crates in the water by the pier | `DXFLOAT:` |  |
| `SwimConsole` | both | falls into water, diving, swimming up; a troop thrown in (`Only`: the scenarios it names) | `DXSWIM:` |  |
| `BubbleConsole` | both | an AirBubble, an Effects and a Rat spawned under water by the pier: each one's zone, physics, velocity and height every 0.25 s for 3 s | `DXBUBBLE:` | the physics, and whether the bubble rises |
| `LadderConsole` | both | Liberty Island's first ladder, every tick | `DXLADDER:` |  |
| `WadeConsole` | both | a troop walking off a ledge into water; `TargetMap`'s shore | `DXWADE:` | `track.py <original run> <dxe run>` |
| `FlightConsole` | both | a gull's flights at the pier; a stock gull's own | `DXFLIGHT:` | `track.py` |
| `SlideConsole` | both | the player on low-friction slopes and in a low-friction zone | `DXSLIDE:` | `track.py`, at `DXCAP_FPS=60` and `20` |
| `RollConsole` | both | four crates on Liberty Island's pier, landed, then rolled away from the player at 400 units/s: each one's place, velocity, physics and base every 0.1 s | `DXROLL:` | the two logs' lines, at `DXCAP_FPS=60` and `20` |
| `VisibleConsole` | both | each mover's line on `TargetMap`: `FastTrace`, `Trace`, its hit and normal, `TraceTexture`'s and `TraceActors`' hits, the line reversed; the visible-actor iterators and `RadiusActors` | `DXVIS:` | the two logs' `mover` lines diffed |
| `MoverConsole` | both | lines across a door and a window on `TargetMap`, moved, hidden, broken, saved | `DXMOVER:` | the two logs diffed |
| `MoverLightConsole` | both | a door on `TargetMap` (Liberty Island) moved `Distance` units off its place, shot from `Ahead` units before it, then with a moving lamp 48 units before it (`DoorName`) | `DXMOVERLIGHT:` | `shots.py --region` of the door, at 4:3 |
| `GlowConsole` | both | a crate on `TargetMap` (Liberty Island), its `AmbientGlow` `Glow` (255, the pulse; 256 for none), `Ahead` units before the eye, the level's `TimeDilation` set to `Dilation` (0.01), then `Shots` shots (6) 1.5 s of the console's clock apart | `DXGLOW:` | `shots.py --region` round the crate: its lights with `Glow=256`; the pulse, on the viewport's clock, through its range over `Shots=12` at any `Dilation` (the original's grabber takes each frame at a phase of its own) |
| `DoorConsole` | both | a hinged door on `TargetMap` (UNATCO HQ) closed on a breakable crate, where its spawn placed it logged: the door's turn and the crate, each 0.1 s | `DXDOOR:` | the two logs' lines |
| `GrateConsole` | both | on 02_NYC_BatteryPark, the five grates under Castle Clinton, movers pitched on their sides: each one's keys and rotation; the player's box swept 16 units along X and Y from a grid round each, every hit; the player put with its box's side resting on a wall's face, and where; the player walked into and along two of them, its place each tenth of a second and its rest | `DXGRATE:` | the put and the rests; the sweeps' hits cell by cell, by place and what they meet |
| `DoorShotConsole` | both | a dart shot at the island's first closed door from 80 units out: every tick for half a second its distance from the door's middle, physics, speed, base and bStuck, and once more at 1.5 s | `DXSHOT:` | the two logs' lines |
| `MoverServer` | both | a line across a door on a dedicated server, the door moved off it: a server actor (`dedicated`) | `DXDEDI:` | the two logs diffed |
| `SightConsole` | both | `LineOfSightTo`, `CanSee`, `PlayerCanSeeMe` | `DXSIGHT:` |  |
| `SeeConsole` | both | the pawns' sight in the tick: two watchers in front of the player, one probing `EnemyNotVisible`, logging each `SeePlayer` and whom it names; their `SightCounter` each tick | `DXSEE:` | the two logs' lines |
| `PainConsole` | both | a pawn's pain and speech timers at half speed, set to be left under a thousandth of a second after a tick: the tick each fires in | `DXPAIN:` | the two logs diffed, both at `DXCAP_FPS=60` |
| `StandConsole` | both | where a walking player rests, and stops at a wall | `DXSTAND:` |  |
| `StairConsole` | both | `Pawn.FindStairRotation` on the pier and a slope | `DXSTAIR:` | the two logs' answers diffed |
| `StompConsole` | both | a pawn landing on another, and on crates | `DXSTOMP:` |  |
| `TurnConsole` | both | a small crate based on a large one, the large turned six times by `SetRotation` (a yaw of 5003 and a pitch of 1000 each): the small one's place and rotation after each | `DXTURN:` | the two logs' lines |
| `BaseConsole` | both | what the island's decorations, items and pawns at rest stand on at its start, and how many stand on each | `DXBASE:` | the two logs' lines, their positions and the original's line ends aside |
| `RestConsole` | both | where a falling crate and the player come to rest | `DXREST:` |  |
| `MissionConsole` | both | whether a mission map's script comes up | `DXMISSION:` |  |
| `EndgameConsole` | both | an ending's way to the credits (`TargetMap`) | `DXEND:` |  |
| `AugVisionConsole` | both | an NPC behind a wall under the vision augmentation | `DXAUGVIS:` |  |
| `PortalConsole` | both | a map opened at a portal | `DXPORTAL:` |  |
| `PasswordConsole` | both | the island opened with `GamePassword=dxcap` on its URL from another mission's map (a dxe run started on `02_NYC_BatteryPark`, the original on its menu map): the local login under the password | `DXPASS:` | the two logs' lines |
| `RevisitConsole` | both | a crate left on the island, UNATCO HQ opened, then the island opened as `01_nyc_unatcoisland`: whether the crate is there, the level as left from Current, or the map loaded fresh | `DXREVISIT:` | the two logs' lines |
| `DeleteSaveConsole` | both | a quick save on the island, then `DeleteGame -1`, as the Save Game screen's Delete sends on the quick save's row | `DXDELSAVE:` | the game's quick save folder after each run: there, as the original leaves it |
| `SlotConsole` | both | the console's `savegame -3`, then `savegame -2`, then the Load Game screen's listing of the save folders | `DXSLOT:` | the listing; the game's Save folder after each run: Save-003 with a SaveInfo and the level, Save-002 with a SaveInfo, the level in Current; remove both after |
| `BrowseConsole` | both | on the island the console's `switchlevel` to UNATCO HQ; a travel to a map that is not there outside any mission and within the island's: the level, the player's progress lines and systems after each; then UNATCO HQ reached with a goal and a note, and `RestartLevel` there: the player's goals, notes and inventory before and after | `DXBROWSE:` | the two logs' lines; the game's Current after (the original's travel within the mission saves the island into it) |
| `TravelConsole` | both | a `CapTravelMarker` (an Info with a `PreTravel` function) spawned on the island, then UNATCO HQ opened: whether the marker's PreTravel runs | `DXTRAVEL:` | the two logs' lines |
| `ReturnConsole` | both | Liberty Island, UNATCO HQ and back: what travels and what stays; the player's goals, notes and keys, two augmentation canisters, the inventory grid, the augmentation slots, a vector and a rotator flag (`Items`: as a map exit travels) | `DXRETURN:` | the two logs' lines diffed |
| `URLConsole` | both | from the menu map: Liberty Island opened, UNATCO HQ by a map exit's travel, the island opened again (from Current), its `RestartLevel`, UNATCO HQ by a map exit's travel again: each level's `GetLocalURL` and the player's item count (`Bare`: the menu map's `open ?loadgame`, a slot named by no number, and the level that comes of it) | `DXURL:` | the two logs' lines; their `InitGame` and `Login` lines |
| `GetConsole` | both | `GET`, `SET`, `GetPropertyText`, `SetPropertyText`; the main menu | `DXGET:` |  |
| `TextConsole` | both | `SetPropertyText` and `GetPropertyText` of each kind of text: a vector whole and by one member, a Plane (a struct extending one), a struct with a quoted string holding a comma, a bare one and an element by index, a dynamic array, a class by path, `Class'..'`, bare name and outside its metaclass, a texture by `Type'Path'`, `Package.Name` and bare name, a bool from `on`, `1` and `False`, an int from junk | `DXTEXT:` |  |
| `JunkConsole` | both | `SetPropertyText` and the console's `SET` with text that is no value of the property, on an int, a float, a bool, a byte, a string and a name of its own actor (`CapJunk`) | `DXJUNK:` | the two logs' lines diffed |
| `MeshTexConsole` | both | `GetMeshTexture` for -1 to 8 on a Lamp1 with no skins, its Skin set, MultiSkins 1 and 7 set, Skin None again, and on an actor with no mesh | `DXMESHTEX:` | the two logs' lines diffed |
| `NameConsole` | both | the names spawned actors get: a Lamp1, a Lamp2, a Lamp3, a second Lamp1, and the two Area 51 sector images | `DXNAME:` | the two logs' lines diffed |
| `ChangeConsole` | both | what a `SET` does to its class's objects after (`PostEditChange`): the client's `Brightness` set to 5 and read back; a `DeusExMover` at rest at its second key, then a `SET` of its class | `DXCHANGE:` | every line |
| `IniConsole` | both | a `User.ini` with keys given twice, or with the player's floats read by hand given empty (`DXCAP_USERINI`) | `DXINI:` |  |
| `IniWriteConsole` | both | `set Engine CacheSizeMegs 5`, then an exit that writes the inis back | `DXINIW:` | by hand, on a copy of an ini with a key above its first header and a section given twice: the file the same after two runs |
| `MenuConsole` | both | every menu screen's window tree (`WindowDump`) and a shot | `DXMENU:` |  |
| `ShownConsole` | both | `IsVisible` and `IsSensitive` of a window of its own and of the HUD's message log, under a parent shown, hidden, disabled, and hidden itself | `DXSHOWN:` | the two logs' lines diffed |
| `SettingsConsole` | both | every menu setting stepped, saved and read back | `DXSET:` |  |
| `DecalConsole` | both | the Display menu's Decals | `DXDECAL:` |  |
| `ScreenConsole` | both | the game's screens on `TargetMap`, each window tree | `DXSCREEN:` |  |
| `CameraConsole` | both | the viewport windows: a security computer's feed with the player in the camera's view, its pan and zoom; the targeting augmentation's zoom on Paul Denton; the spy drone's window; four marked shots | `DXCAM:` | the two logs' lines; `shots.py --region`, at `DXCAP_WINDOW=1024x768`, the zoom and the drone's window against `DXCAP_RENDERER=D3D`'s ([shots](#shots)) |
| `ScrollConsole` | both | a scroll area and a slider under the mouse | `DXSCROLL:` | with `tools/dxcap/timelines/scroll.txt` on DXEngine |
| `ClickConsole` | both | a button, a choice and a checkbox clicked; a menu dragged; Alt accelerators | `DXCLICK:` | with `tools/dxcap/timelines/clicks.txt` on DXEngine |
| `TypeConsole` | dxe | text typed into an edit field on the main menu, past ASCII and several characters at once | `DXTYPE:` | with `tools/dxcap/timelines/type.txt` |
| `PasteConsole` | both | `PasteFromClipboard`'s text, the clipboard holding "dxcap paste" (an X selection owner alive through the run) | `DXPASTE:` | the two logs' lines |
| `PadConsole` | dxe | the player's place, speed, facing and physics every half second, and the root window's size, as a pad or the keys move it | `DXPAD:` | with `tools/dxcap/timelines/pad.txt` or `axis-keys.txt` (`DXCAP_BACKEND=SDL3`), on a `DXCAP_SETTINGS` file whose `Gamepad.Enabled` is true (`vulkan-settings.json`'s is not: the virtual pad attaches and moves nothing) |
| `AxisConsole` | both | an input axis a script sets: the player's `aBaseY` set to `Axis` from the console's tick for `Ticks` ticks on the island, each tick's acceleration and speed | `DXAXIS:` | the accelerations at `DXCAP_FPS=60`, the speeds tick by tick |
| `BindConsole` | both | the island's input bytes every quarter second and exec functions of its own (`Mark`, `Shot`, `ShowPath`), for `bind-keys.txt`'s keys on DXEngine with a `User.ini` binding them (its header); in both engines the console's `TIMEDEMO` by command, started and stopped | `DXBIND:` | the timedemo's result lines, from `DX.dx` in both (a level change restarts it, its first result empty) |
| `KeysConsole` | dxe | the console's state and the root window's marks of Shift, Ctrl and Alt every quarter second, as a timeline holds Left Shift while typing `~`: the state turns Typing | `DXKEYS:` | with `tools/dxcap/timelines/held-keys.txt`, `DXCAP_BACKEND=SDL3` |
| `EditConsole` | both | a script's text calls on an edit field (`CR()`'s text first), a text window and a button | `DXEDIT:` |  |
| `MultiEditConsole` | both | an edit field of several lines on the main menu: each move from fourteen places, chains of moves, a drag, a selection put in, a single line's moves; two marked shots of a selection over three rows | `DXROWS:` | the two logs diffed; `shots.py --region --block 4` on the field, at `DXCAP_WINDOW=1024x768`; with `Keys=True`, `tools/dxcap/timelines/note-keys.txt` on DXEngine |
| `ColumnConsole` | both | a list's columns fitted to fields in fonts that have and lack their characters, and to a field of two lines, on the main menu | `DXCOLUMN:` | the two logs diffed |
| `PointerConsole` | both | the root's pointer in the middle of the island's view: no modal window, one made hidden, shown, hidden, destroyed; five marked shots | `DXPOINTER:` | the shots' middles against the first, at `DXCAP_WINDOW=1024x768` |
| `GrabConsole` | both | the root's counts of the keyboard's and the mouse's grabs on the menu map, a step every half second: the window stack cleared, the main menu, a message box over it hidden and shown again, the menu under it hidden, the stack cleared, a modal made hidden then shown and destroyed, the root's grabs and ungrabs by script; the pointer under a script's grab of the mouse with no modal up, and after it, two marked shots | `DXGRAB:` | every line; the pointer in the first shot alone, at `DXCAP_WINDOW=1024x768` |
| `TimerConsole` | both | a window's timers on the main menu: a single one, one looping every millisecond and one at 0, two added in turn, one with no function name | `DXTIMER:` | the two logs diffed, both at `DXCAP_FPS=60` |
| `SizeConsole` | both | the root's size and scale; the size a new window takes | `DXSIZE:` |  |
| `PerfConsole` | both | what a frame costs on `TargetMap`: sixty one-second windows | `DXPERF:` | `tools/perf/frame-report.py`, with `DXCAP_PERF=1` ([measuring both engines](#measuring-both-engines)) |
| `PerfTurnConsole` | both | `PerfConsole` with the view turning | `DXPERF:` | the same |
| `BeltConsole` | both | the object belt | `DXBELT:` |  |
| `FrobConsole` | both | the frob highlight round two decorations; what `ConvertVectorToCoordinates` gives a window on the root (`ProjectionWindow`, asking from its `DrawWindow` as the highlight does) for each decoration's box, a point off the screen and one behind | `DXFROB:` | the two logs' `projected` lines, at `DXCAP_WINDOW=1024x768`; the shots' boxes |
| `LootConsole` | both | two carcasses searched; the inventory grid; with `Hover`, between the searches, the inventory screen opened by the console, the pointer put on an item's button, then on the screen's corner, a marked shot of each | `DXLOOT:` | with `tools/dxcap/timelines/loot-drags.txt` on DXEngine; with `Hover`, the pointer drawn in both shots, the one `CursorRequested` gives |
| `CarcassConsole` | both | what carcasses hold, their search, the looted weapon | `DXCARC:` |  |
| `StatueConsole` | both | on Liberty Island, `StatueMissionComplete` set 6 s in, as the statue's ending sets it: each terrorist's state, health and items as they change and each `TerroristCarcass` as it appears, with its items, every tick for 5 s; then UNATCO HQ, and the island again from `Current`: every carcass's items | `DXSTATUE:` | the two logs' lines, their ticks and times aside |
| `ChoiceConsole` | both | a conversation's choices and their focus, by the keys and the pointer (`SetCursorPos`) | `DXCHOICE:` |  |
| `KeypadConsole` | both | the root's focus round the keypad; a stray key release | `DXKEYPAD:` | with `tools/dxcap/timelines/stray-release.txt` on DXEngine |
| `BorderConsole` | both | `GC.DrawBorders` on the inventory screen | `DXBORDER:` |  |
| `ColorsConsole` | both | `DrawBorders` on the Colors screen | `DXCOLORS:` |  |
| `RotatorConsole` | both | the rotator and vector conversions; floats past an int's range to ints and bytes, by the VM's generic path and, stored in variables, its typed one | `DXROT:` |  |
| `DepthConsole` | both | an expression 100 operators deep, the sum of a hundred ones, from the menu map | `DXDEPTH:` | the one line |
| `OpsConsole` | both | the natives' edges: an int divided or multiplied by a float in place, shifts by 33 and -1, `Clamp` and `FClamp` with their bounds the wrong way round and `FClamp` of a NaN, floats as text at a tie of their sixth decimal, a large one and both infinities, a string and a float `switch`, `new` with a flag outside the script's, every actor iterator given None, the radius ones round the level's first PlayerStart (with a census of the player's zone by kind and the animals' zones; `ListRadius`: each actor `RadiusActors` gives), `OrthoRotation`, a destroyed pawn's `nextPawn`, `PlayBlendAnim` with slot 256, `StrLen` under `STY_None`, `DrawText` and `TextSize` with no font, `GetNextSkin`, `GetSoundDuration` of a WAV and an MP3 | `DXOPS:` | the engines' own lines too: `new: Flags`, `PlayBlendAnim:`, `No font` |
| `ScoutConsole` | both | the pawn generators and their scouts: each one's location, zone and physics, a marker set at each generator's spot and 16 units under and over it, a scout and a marker spawned at four spots where the original's FlyGenerators have stood, and the level's rolls for flies (its decorations and carcasses with `bGenerateFlies`) | `DXSCOUT:` | the FlyGenerators are chance, one per roll in ten: Liberty Island's six rolls give 0 to 2 a run in either engine |
| `OverlayConsole` | both | `Canvas.DrawActor` and `Canvas.DrawClippedActor` of the player's pawn from the console's `PostRender`, drawn visible and drawn hidden: its `bHidden` after each; with `Wire`, a crate and a `JCDentonMale` holding a pistol on Liberty Island drawn each frame with `WireFrame` (`Solid`: without), shot | `DXOVERLAY:` | with `Wire`, the shots compared over the crate and the figure, at `DXCAP_WINDOW=1024x768` |
| `ConstrainConsole` | both | on Liberty Island a hidden crate 96 units ahead, drawn by a window over the screen's top right quarter (`CapActorWindow`) with `GC.DrawActor`, constrained to the window and then not, a shot of each | `DXCONSTRAIN:` | `shots.py --region` over the window, at 1024x768 |
| `MapWalkConsole` | dxe | every map of the Maps folder opened in turn, 1.5 s each, Entry left out: each one's name and count of actors once it is up, then a done line | `DXMAPWALK:` | a map the loader refuses ends the run before the done line, the refusal in the log |
| `NoneConsole` | both | what a None context yields: an array element written and read through None, a member of each type, a switch, a struct, calls, and a native's optional argument given through None (`CapNoneProbe`'s probes) | `DXNONE:` | the two logs' lines diffed |
| `StateConsole` | both | state code's limits in a tick: an actor whose state code destroys it and logs on, and one bouncing between two states by `GotoState` alone, its count each second | `DXSTATE:` | the two logs' lines |
| `ExtendConsole` | both | a state extending another (`CapExtender`): the extended state's `Begin` and a named label run on `GotoState` into the extending one, its function called from it, the state's name kept | `DXEXTEND:` |  |
| `PreemptConsole` | both | a `GotoState` from within `EndState` and from within `BeginState` (`CapPreempter`): the state the actor ends in, the labels run | `DXPREEMPT:` |  |
| `MidConsole` | both | `Object.Mid` at its edges | `DXMID:` |  |
| `FrameConsole` | both | a frame held 2 s: the level's time over it, an actor's next tick | `DXFRAME:` | the two logs diffed |
| `PassesConsole` | both | a tarp's Blowing looped at 20 a second through one frame held 2 s: its ticks and AnimEnds, the AnimFrame after the long tick (`CapAnimated`) | `DXPASS:` | the AnimEnds in the long tick |
| `FinishConsole` | both | a tarp's state code calling `FinishAnim` after a `PlayAnim`, a `TweenAnim` with a time, with nothing animating and after a `LoopAnim`: the level's time as each returns (`CapFinisher`) | `DXFINISH:` | the time each wait takes |
| `AnimNameConsole` | both | `HasAnim`, `GetAnimGroup`, `PlayAnim`, `LoopAnim`, `TweenAnim` and `PlayBlendAnim` of a sequence name a tarp's mesh lacks | `DXANIMNAME:` | the two logs' lines diffed |
| `ModConsole` | both | the natives only a mod calls: the timing natives, a flag from a string, `ExtString`'s parts, `ResetConfig` | `DXMOD:` | the two logs diffed |
| `ScriptedConsole` | both | `ScriptedTexture.ReplaceTexture` on the harness's `CapScripted`, drawn at four times its size: as loaded, after a texture of its size, after one of another size and None, after an animated one; beside it `CapSourced`, of a source texture, after its source is set and after a `ReplaceTexture`; each step shot | `DXSCRIPTED:` | `shots.py --rmse --region 64,64,256,256`, and `384,64,256,256` for `CapSourced`, at `DXCAP_WINDOW=1024x768`; the two logs diffed |
| `LocalizeConsole` | both | a package's text and the objects its `.int` files register: the game's, two mod folders' (`build/dxcap/mods`, the harness's own, first in the run's paths, one by its absolute path and one by a relative one), none | `DXLOC:` | the two logs diffed |
| `ConfigConsole` | both | a class's own config file (`ConfigProbe`'s `DXCapConfig.ini`) read, one added and a vector, a class and two strings written back as the original writes them, then an object's own `StaticSaveConfig`, which writes the class's | `DXCONFIG:` | the file after each run |
| `LoopConsole` | both | one loop of `Jumps` jumps in a tick, 9,990,000 by default: past 10,000,000 either engine stops, but with `-norunaway` (`DXCAP_FLAGS`); with `LoadAfter`, slot 9's load in the same tick (`SaveConsole`'s `Fresh` makes it), the load counting its own jumps | `DXLOOP:` | the two logs; the original's error is its dialog's ([crashes](#crashes)) |
| `CallConsole` | both | singular functions held off; a call chain `Depth` deep, 248 by default: one more and either engine stops | `DXCALL:` | the two logs diffed |
| `DynLoadConsole` | both | `Object.DynamicLoadObject` by each form of name | `DXDYNLOAD:` |  |
| `GarbageConsole` | both | 128 actors destroyed, one a tick | `DXGARBAGE:` |  |
| `ChurnConsole` | dxe | 20 actors spawned and 20 destroyed every tick for 120 s (with `DXCAP_MEMLOG=1`) | `DXCHURN:` |  |
| `DeleteConsole` | both | `CriticalDelete` as the game uses it | `DXDELETE:` |  |
| `SoundConsole` | both | sounds behind a wall, a reverb zone, beeps from three sides and the root window | `DXCAP:` | `sound.py <run>`, with `DXCAP_RECORD=1` |
| `SkipConsole` | both | a conversation's lines skipped | `DXSKIP:` | `skip.py <run> [<run> ...]`, with `DXCAP_RECORD=1` |
| `LipConsole` | both | the player speaking three of mission 1's lines as ConPlay plays one, 8 s each: every mouth shape written to its `nextPhoneme` (`Beep`: a beep on the same tick as each line; `Speech`: a sound's full name, played first, such as one of the package's test sounds, `DXCapture.CapLoop`) | `DXLIP:` | each line's shapes one after another; with `Beep=True` and `DXCAP_RECORD=1`, `sound.py --onsets <run>`: each line's first onset after its beep's |
| `SleepConsole` | both | on Liberty Island, a `CapSleeper` sleeping 0.105 s over and over and one 10 s, the game saved and loaded during them, the load asked for by the console's tick (`LoadByActor`: a `LoadHelper`'s): each waking, and their `LatentFloat` before and after the load; the level's time saved, and the level's time and the wall clock at each tick waiting for the load and the first after it | `DXSLEEP:` | the two logs, at `DXCAP_FPS=60`: the short one's wakings by the save, the long one's after the load; the ticks around the load, the level's time after it past the save's by the clock's step |
| `InventoryPathConsole` | both | on Liberty Island, `FindBestInventoryPath` of each pawn with a `MinWeight` of 0 and 0.0005, predicting respawns and not | `DXINVPATH:` | the two logs, pawn by pawn where a pawn stood at one place in both |
| `AmbientConsole` | both | on Liberty Island, the level's ambient sounds hushed, a fan 64 units ahead at a `SoundVolume` of `Volume` (5): 4 s at the ini's `AmbientFactor`, 4 s after a `SET` of it to `Factor` (20, past Galaxy's hold at 10) and 4 s at 0.7 again, beeps before each | `DXAMBIENT:` | with `DXCAP_RECORD=1`, `sound.py --levels <run>` between the beeps (`--onsets`): the fan's step from 0.7 to `Factor` |
| `SaveConsole`, `LoadConsole` | either | a save to slot 9 with a goal, a note, a key and two HUD messages (a log object each), and its load (`Via`: the save after a map exit's travel; `Open=<url>`: the load by the console's `open <url>`, as `?loadgame=9`; `Fresh`: a new game's start, sent by the game's `StartNewGame` from the menu map and saved as it is) | `DXSAVE:` |  |
| `QuickLoadConsole` | dxe | UNATCO HQ opened from the island (the island into Current), the quick save there by a `SaveHelper`, then `LoadGame -1`: whether the level loads again | `DXQL:` | a Windows build under Wine (`DXCAP_ENGINE`) against the Linux one |
| `StartConsole` | either | a run started on slot 9's save (`?loadgame=9`: DXEngine's `<map>`, the original's `DXCAP_START`), logged as `LoadConsole` logs it | `DXSAVE:` |  |
| `ForwardConsole` | original | from the menu map, the level's URL each second for 60 s, then an exit, while a second `DeusEx.exe` started by hand with a map and the run's `INI=` and `USERINI=` hands the running game its command line | `DXFWD:` | the map after the second launch; the game's `WM_COPYDATA:` line; the splash in the game's windows over its start (`xdotool search --pid`) |
| `CensusConsole` | both | slot 9's save loaded in the running game, from the menu map or `From`'s, `Wait` s in, or none (`Load=False`: a run started on it); the loaded level's actors by class each second for `Seconds`: live, the most live, those spawned (a `CensusNotify` hears them; `Notify=False`, none) and the highest number in their names | `DXCENSUS:` | `census.py <original run> <dxe run>`, with `DXCAP_MEMLOG=1` |
| `LoadingConsole` | both | the screen while a map loads or a level is saved: a load, a travel within a mission, a save and its load; a beep a second before each; each tick under a level action, the paint's | `DXLOADING:` | the two logs' lines diffed; `shots.py`, at 4:3; `sound.py --levels` and `--onsets <run>`, with `DXCAP_RECORD=1 DXCAP_MUSIC=1`: the music through each load |
| `NetConsole` | both | the script's sockets, a master server's list | `DXCAP:` |  |
| `ServeConsole` | either | a listen server, its players and beeps; it exits after 290 s (`Kick`: the other player's pawn destroyed 5 s after the host is placed) | `DXCAP:` | `netquery.py`, `fakemaster.py`, `netrelay.py` |
| `JoinConsole` | either | a client of `127.0.0.1:7790`; the moves its velocity does not account for (a server's corrections) | `DXNET:` | `sound.py --onsets <run>`, the client alone with `DXCAP_RECORD=1`: the server's beeps |
| `LinkConsole` | both | 20 UdpLinks bound to ports from 2100, destroyed, then 20 bound to the same ports again: how many bind each time | `DXLINK:` | the two logs' lines |
| `EntryConsole` | both | on Liberty Island, markers spawned into the Entry level by its LevelInfo, as the game's server lists spawn their links there, and moved: each one's level, zone, zone number and leaf, with the player's and both levels' LevelInfos' | `DXENTRY:` | the two logs diffed |
| `RstConsole` | dxe | a TcpLink to `tools/dxcap/rstserver.py` (started beside the run), which resets the connection a second after it opens: a line sent as it opens, two after the reset, each send's answer and the link's error | `DXRST:` | the run ends with its own exit, no signal |
| `RejoinConsole` | either | a client that joins twice | `DXREJOIN:` |  |
| `TravelServeConsole` | either | a server that travels; it exits after 150 s | `DXCAP:` |  |
| `TravelJoinConsole` | either | a client that follows it | `DXNET:` |  |
| `PickServer` | either | a weapon held before the joined player's eyes on a dedicated server (`dedicated <engine> PickServer DXMP_Cathedral?game=DXCapture.CapDeathMatch`): the player's frob target as the server has it, also in the player's name; it exits 20 s after placing it | `DXPICK:` | the client's log |
| `PickJoinConsole` | either | a client of it that uses what it looks at 12 s in: its frob target, the server's (its name), whether it holds the weapon | `DXPICK:` |  |

- `SaveConsole`, `LoadConsole`, `StartConsole`, `LoadingConsole`, `MoverConsole`,
  `SleepConsole` and `CensusConsole` use the game's own `Save\Save0009` and `Save\Current`,
  and `QuickLoadConsole` and `DeleteSaveConsole` its `Save\QuickSave`: delete them after, as
  the launchers' UI tests continue the newest save they find there. A save the original made,
  copied into `Save0009`, loads in both engines when it is a hub save (`bCheatsEnabled`); with
  a quick save there the original never finishes its run.
  `CensusConsole`'s save is Liberty Island's start, saved in the original without cheats
  (`SaveConsole` with `Fresh`, run on the original): the player is under fire there, so a
  load's census 5 s in differs from run to run.
- `ConfigConsole` writes `DXCapConfig.ini` into the game's `System` folder, the original's into
  its view (through the view's link when the file is there), DXEngine's with `DXCAP_USERDIR`
  into that folder: delete it after. Its object's `StaticSaveConfig` is the console's own, into
  the game ini's `[DXCapture.ConfigConsole]`: an Object of a config class made by `new` hangs the
  original at exit, the run ending at its timeout.
- In `VisibleConsole`'s logs the original numbers the player and its shadow one higher: its menu
  map made the first.
- **A dxe run started on its map has no long first frame there.** The original's always travels
  from its menu map, its first frame on the map taking in the load
  ([a long frame](NATIVES.md#small)): a dxe run compared over a map's first seconds starts at
  `DX.dx` and travels as the original's does (`StandConsole`, `MoveConsole`).
- A sweep runs `MissionConsole` over every mission map; no driver for it is committed.

### Timelines

A dxe run does what `DXCAP_TIMELINE`'s file says, in [the live mode's format](#live-servers),
beside its console's own script; with `-` for its console it keeps the stock one. Keys reach the
game as the window's do, so a timeline drives the UI, which no console can; `keydown` and `keyup`
go through the window itself (`DXCAP_BACKEND=SDL3`'s or `SDL2`'s), its naming of keys with them,
and `pad` and `padaxis` through its own virtual pad, the one pad such a run sees (the harness sets
`SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT` to its ids). The original takes no timeline, and the
desktop's Xwayland passes over the pointer moves XTEST sends, so what a timeline's mouse and keys
do is checked on DXEngine alone.
`tools/dxcap/timelines` has the map loads that show what a session keeps of the levels it
left, both from Liberty Island, ten loads of each map 25 s apart, then an exit:

| Timeline | Between | So each load |
|---|---|---|
| `reload-fresh.txt` | the island and Battery Park | crosses a mission: `Current` emptied, the map from `Maps` |
| `reload-current.txt` | the island and UNATCO HQ | stays in mission 1: the map left saved into `Current`, the next one from there |

and fourteen more: `collections.txt`, the collections a player meets, each timed -- the console's
`obj garbage` on the island, then Battery Park and the island loaded in turn, `obj garbage` after
each load -- `keys.txt`, a key of each kind the SDL windows name, the keypad with Num Lock on and
off, B with Alt held and its character, and typing keys under each layout family, on the stock
console with `DXCAP_BACKEND=SDL3` ([a key's name](NATIVES.md#small)), `bad-keys.txt`, a `Button` and an
`Axis` the pawn lacks pressed on the stock console (`DXCAP_USERINI` binding them), and, each one
console's input, `loot-drags.txt` (`LootConsole`), `stray-release.txt` (`KeypadConsole`),
`scroll.txt` (`ScrollConsole`), `clicks.txt` (`ClickConsole`), `pad.txt` (`PadConsole`),
`type.txt` (`TypeConsole`), `note-keys.txt` (`MultiEditConsole`), `bind-keys.txt` (`BindConsole`), `held-keys.txt`
(`KeysConsole`), `resize-drags.txt` (`ResizeConsole`) and `axis-keys.txt` (`PadConsole`: two keys
on one axis, a key bound in lower case, Enter with Alt held).

```sh
DXCAP_MEMLOG=1 DXCAP_TIMELINE=tools/dxcap/timelines/reload-fresh.txt \
    tools/dxcap.sh dxe - 01_NYC_UNATCOIsland 580
```

### Net tests

One engine on each side, on this machine. Start the server (`original ServeConsole 300` in the
background, or `dxe ServeConsole DX.dx 300`: the default 120 s would end it before its own 290),
wait until its game port is bound (`ss -uln | grep :7790`; an original server answers some 13 s
after it starts), then run the client (`dxe JoinConsole DX.dx`, or `original JoinConsole`).
Never start a second server before the first has exited: it cannot bind the port ("Net: cannot
listen" in its log), and the client joins the old one. To read what passes between them, start
the server with `DXCAP_PORT=7792`, wait for that port, start `tools/dxcap/netrelay.py` (it
listens on 7790 and forwards to `127.0.0.1:7792`), then the client: each second a row each way
-- packets, bytes, acks, bunches by channel and reliability -- and with `--log <file>` a line
for every datagram, each bunch's header decoded. The server's game is the package's
`CapDeathMatch`, without the game's check that a joining player's console is the stock one.
`tools/dxcap/netquery.py` asks a server's LAN beacon and GameSpy query answerer, and
`tools/dxcap/fakemaster.py` acts as a master on UDP 27900 (`DXCAP_UPLINK=127.0.0.1:27900`):
the same questions for either engine.

### Live servers

Every server's game disconnects a player whose console is not the stock one
(`Invalid Console class, disconnecting`;
[the check](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#deus-exs-additions)).
So `live <address> [<secs>]` keeps the stock console, and the engine drives the run from a
timeline (`--timeline=<file>`, `DXEngine/Timeline.h`). Each line is
`<clock> <seconds> <action>`: the clock `start` (from the engine's start) or `game` (seconds of
the net game); the action `press <key>` or `release <key>` as the key in the window (a mouse
button pressed where the pointer is, a press of `MouseWheelUp` or `MouseWheelDown` a notch of the
wheel), `keydown <name>` or `keyup <name>` a key of the keyboard by the window system's name for
it (SDL's scancode names, under SDL2 and SDL3: `Left Shift`, `Keypad *`, `Menu`; the X11 window
takes none), put among the window's own events with the modifiers the timeline holds and its
Num Lock and Caps Lock, which its presses of them toggle (Num Lock on and Caps Lock off at the
start), and each key and character the window gives the game logged (`DXKEY:`), `keylayout
<family>` the layout's family the keys stand in for as the SDL windows name them (`QWERTY`,
`QWERTZ`, `AZERTY`, `Dvorak`, `Swedish`; alone, the system's again), `pad <button> down` or `up`
and `padaxis <axis> <value>` the SDL window's virtual pad
(`A` to `DpadRight`, `LeftX` to `RightTrigger`: a stick -1 to 1, a trigger 0 to 1), its keys
logged so too, `pointer <x> <y>` the pointer moved there in the root window's coordinates,
`chars <text>` text typed as the window sends it (UTF-8, every character in one go), or a
console command (`shot`, `exit`, ...). The timeline is `DXCAP_TIMELINE`'s file, or `JoinConsole`'s
walk: stand 5 s, walk forward 5 s (`W`), stand, shots at the stops, out at 25 s. Each second of
the game the player's place and every other pawn's are logged (`DXLIVE:`), and each correction the
server sends the player (`ClientAdjustPosition`), how far it moved it; a run dropped to the menu
exits. A join downloads what DXEngine lacks into the game's `Cache`. Which servers are up, and how
full: `https://master.333networks.com/json/deusex`. Pick an empty server. Most run a mod (MTL,
ANNA and others) whose player class replaces the stock moves and corrections, its names hidden:
for the stock game's, serve the original here as for the [net tests](#net-tests)
(`original ServeConsole 300`, its port bound) and run `live 127.0.0.1:7790`.

### Shots

- The original's own `shot` gives noise under `D3DDrv` and black under the others in Proton. Its
  window, through either renderer, is read five times a second by `tools/dxcap/grab.py`
  (the window found by its name), keeping each frame whose corner carries the console's mark (a
  magenta block, then the shot's number in eight black or white blocks).
- **A frame up through a load or a save** (`LoadingConsole`'s) reaches the original's window for
  all of it or not at all, whatever the load's length: each is grabbed in about half the runs,
  so a comparison takes the original's shots from two or three runs.
- **Through `D3DDrv`** the window's contents reach the grabber only every 0.2 to 1.4 s, though
  the game draws some 63 frames a second: a console's mark, up for 0.6 s, can pass ungrabbed,
  and a run keeps 0 to 2 of `MeshConsole`'s 2 shots. `OpenGLDrv`'s window keeps them all.
- **After a viewport window**, through `OpenGLDrv`, the original's frame shows none of what its
  parent draws over it in `PostDrawWindow` (the targeting zoom's box and the target's text, the
  drone window's label); through `D3DDrv` it shows all of it, as DXEngine's does
  (`CameraConsole`).
- **The desktop awake.** With the monitors blanked (Plasma's energy saving: their DPMS off) no
  window is viewable and every grab fails, a run keeping no shot.
  `distrobox-host-exec kscreen-doctor --dpms on` wakes them, and
  `distrobox-host-exec kde-inhibit --power --screenSaver sleep <seconds>` keeps them on that long.
- **Gamma.** The original's frames carry its gamma ramp, 1.5 at the runs' Brightness of 0.6, the
  same through `D3DDrv` (`DXCAP_RENDERER=D3D`). DXEngine's `shot` reads the frame before the
  present pass applies gamma (`GammaCorrectScreenshots` off), so a DXEngine shot takes that gamma
  before its brightness is compared ([brightness](NATIVES.md#brightness)):
  `tools/dxcap/shots.py <original run> <dxe run>` puts it through the gamma and gives,
  for each pair of shots, DXEngine's brightness over the original's, block of 16 pixels by
  block (`--blocks` prints the grid) and over the frame or a region (`--region x,y,w,h`).
- **The presented frame.** A DXEngine shot is the scene before the present pass, so the
  render-scale upscale does not show in it, nor the gamma with `GammaCorrectScreenshots` off.
  `grab.py` reads DXEngine's window too, by its name, `Deus Ex (v1112fm) - DXEngine`; at a render
  scale under 1 the mark's blocks are scaled with the frame, and the shot numbers it reads from
  them are wrong.
- A renderer that fails to start falls back to `SoftDrv` without a word, its frames passing for
  the renderer's: the script then fails on the log's `Bound to SoftDrv`; and so for a dxe run
  whose GPU device does not start, on its log's `the software device draws in its place`
  ([which device draws](ENGINE.md#rendering)).
- **The same view.** Two runs' shots compare only from one view: one window size (`DXCAP_WINDOW`)
  and, for `LightConsole`, one `View`, whose distance its `chosen` line logs; a dxe run against
  an original run of another `View` is off by however much the frame differs.
- **Two dxe runs**: `shots.py` takes a dxe run in the original's place, another device's or
  build's, and puts the gamma on neither. `--levels` gives, pixel by pixel, the largest
  difference of a channel and how many pixels are more than a level apart. Two runs' shots differ
  where something moves with the level's time, and at another frame rate by a level at a few
  pixels, where a light map lies elsewhere in its page (`LightmapAtlas` places each in the order
  the surfaces are first drawn).
- **Registration against `D3DDrv`.** DXEngine draws the scene's pixels as `D3DDrv` does, not as
  `OpenGLDrv` ([the scene's pixels](NATIVES.md#the-scenes-pixels)): where a polygon's edges or
  rows are compared, or a correlation or RMSE taken, the original runs with
  `DXCAP_RENDERER=D3D`. `shots.py --rmse` gives the grey levels' RMSE and correlation over the
  region, pixel by pixel, and `--shift` the quarter-pixel move of DXEngine's shot that fits the
  original's best: none when the two register.

### Running the original

- **A long script log line stops it**: one of some 1,400 characters ends the run within a
  second, no dialog shown, its log cut at 4 KB. A console logs a long list a few names a line.
- **It boots its menu map** whatever map its command line or ini names, but with `-hax0r`
  (`DXCAP_START`) or as a dedicated server (`dedicated`: `<map> -server`); a console class
  travels with `open <map>` itself.
- **`UCC.exe` crashes on a long base directory** while it reads its ini, and wants both
  `UCC.ini` and `DeusEx.ini`: hence `build/dxcap`. 92 characters as Wine names it
  (`Z:\...\build\dxcap\System\`) work; after a move to a longer path, `setup` and `compile`
  show whether it still holds.
- **It runs in the development container, never on the host outside it**, with the container's
  32-bit libraries
  ([dependencies](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)),
  under the Proton build's own `wine`, in a Wine prefix of its own (`build/dxcap/prefix`). A
  prefix's Wine desktop is on the display of whatever started its wineserver: a prefix shared with
  a Windows program started from the desktop, as
  [`tools/ida/idalib-mcp.sh`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/tools/ida/idalib-mcp.sh)
  starts IDA, puts the game's first window there, where it fails with `BadWindow`.
- **It runs in a view of the game**, `build/dxcap/game`, made afresh for each run: the game's
  folders linked, and its `System` folder's files except what the game writes there (its log,
  `Running.ini`, shots), the IDA databases (`*.i64`) and any file with no extension. The original
  takes a package's bare name in its working directory before any of its paths
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)),
  so the recreated launcher's `DeusEx`, installed beside `DeusEx.exe`, cannot stand in for the
  `DeusEx` package there. The run's log, `Running.ini` (a stale one opens the recovery wizard,
  which waits for a click) and shots stay in the view, never in the game's folder. Its
  `DeusEx.exe` is GOG's where `goggamefiles/` holds it: Steam's is packed and starts only under
  Steam's client ("Failed to find Steam"), and GOG's is the same build over the same DLLs.
- **It stops at its first focus change while another X client holds the pointer
  grabbed**: Wine gives a window the focus only once it can grab the pointer itself, and
  tries until it can, in the game's message pump, the window black and the log cut at
  4 KB. ImageMagick's `import`, given a window that has gone, grabs the pointer and waits
  for a click on another, which a Wayland desktop never sends it: `grab.py` gives each
  import 2 s and kills it with itself, and `original` refuses to start while the pointer
  is grabbed, naming any `import` left. `wine taskkill /im DeusEx.exe`, without `/f`, in
  the run's prefix closes a running original cleanly, its log written.
- `dxcap.sh`'s comments document the rest of its set-up: vkd3d's libraries in the prefix for
  `D3DDrv`, no Wine desktop, and at a run's time limit the game closed by that `taskkill`,
  its log written, killed only when it has not closed within 10 s; never the wineserver.

### A Windows build

`DXCAP_ENGINE` naming a Windows build's `DXEngine.exe` (Port Ex Machina's
`scripts/dx.sh build windows-x86_64 engine`) runs it as the original runs, under the Proton
build's own `wine` in `build/dxcap/prefix`, its files by Wine's names for them (`Z:\...`), the
run's ini's `Paths=` too. Its window draws through Wine's X11 driver, its silence is OpenAL
Soft's null driver (`ALSOFT_DRIVERS`), the run collects the shots in its `--userdir`
(`DXCAP_SETTINGS`'s), and DXVK's `d3d11` and `dxgi` (`DXCAP_DXVK`) are copied into the prefix
and taken by `WINEDLLOVERRIDES`. `wine` starts it through `start.exe`, which passes on its exit
code but, killed at the run's time, leaves the engine running: the run finds the engine's own
process, which `DXCAP_MEMLOG` logs and the run's end kills.
`DXCAP_RECORD` and `DXCAP_PERF` are for Linux builds.

### The display

Both engines draw on the desktop's X display, Xwayland (`$DISPLAY`; `DXCAP_DISPLAY` names
another), each in a 1280x720 window (`DXCAP_WINDOW` another size): the original through Wine's X11
driver, DXEngine through its window's X11 backend (`DXE_DISPLAY_BACKEND=X11`,
`WAYLAND_DISPLAY` unset, `SDL_VIDEODRIVER=x11`). One X server and the GPU serve both, so their
runs compare; no run uses a hidden display, as runs on different displays do not compare. The
development machine is given over to it: windows come and go on its desktop.

### Measuring both engines

`PerfConsole` times either engine's frames the same way, and `DXCAP_PERF=1` samples its main
thread meanwhile. Both draw through OpenGL with no vsync, at 1280x720, DXEngine without MSAA:

```sh
DXCAP_NOVSYNC=1 DXCAP_PERF=1 tools/dxcap.sh original PerfConsole 150
DXCAP_NOVSYNC=1 DXCAP_PERF=1 DXCAP_SETTINGS=tools/dxcap/perf-settings.json \
    tools/dxcap.sh dxe PerfConsole DX.dx 150
tools/perf/frame-report.py <original run> <dxe run>
```

- **The windows** are the wall clock's, the level's `Hour` to `Millisecond`, which both engines
  set from the local time each tick: each `DXPERF:` line gives a window's span in milliseconds of
  the day and the frames in it. The total line gives frames a second, the mean, p50, p99 and the
  worst frame. Started at `DX.dx`, DXEngine travels to the map as the original does.
- **The original's own split**: its `Engine` object's `GameCycles` (the game's tick) and
  `ClientCycles` (the frame drawn), read each frame where they are not 0 (DXEngine keeps neither),
  are each line's `game` and `client`.
- **`frame-report.py`** keeps the samples inside the windows and gives the main thread's time a
  frame by area and by function, and its cycles and instructions a frame. The original's
  functions are each DLL's nearest export below the sample (its base from `maps.txt`):
  `Core.dll` and `Engine.dll` export most of theirs, `Render.dll` 97 of them, so names in it
  are only near. DXEngine's are perf's own symbols. `--mean` averages each engine's runs.
- **One run at a time**, nothing else running; three of each, alternating, before a number is
  taken. The machine's speed drifts from run to run by 10% and more, its CPU at full clock
  (its power and memory clocks, the scene's own changes): a change to DXEngine is measured by
  alternating runs of the two builds (`DXCAP_ENGINE`), and by the time of the functions it
  changes, which the report gives.

### Crashes

An original run that crashes shows a "Critical Error" dialog on the desktop and hangs
until the harness kills it, its log cut short: the game writes the log only as it exits.
Dismissed, the dialog lets the game log the error and the calls under it, and exit. A dxe run
that crashes leaves a core, which systemd keeps.

```sh
# The original's dialog, dismissed
xdotool search --name 'Critical Error'    # its <window>
xdotool windowfocus --sync <window>
xdotool key Return
# DXEngine's core: the functions on the stack (the Release build has no lines)
coredumpctl dump <pid> --output=<scratchpad>/core
gdb -batch -ex bt <parent>/build/linux-x86_64/engine/DXEngine <scratchpad>/core
# Lines: a copy with symbols (some two and a half minutes), run with the last dxe run's ini
cmake -S DXEngine -B <scratchpad>/dbg -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build <scratchpad>/dbg --target DXEngine
cd gamefiles && gdb -batch -ex run -ex bt --args <scratchpad>/dbg/DXEngine --no-launcher <parent>/gamefiles \
    --ini=<parent>/build/dxcap/System/DXE.ini --userini=<parent>/build/dxcap/System/DXEUser.ini --url=<map>
```

An object freed while something still points at it shows only in an AddressSanitizer build,
which stops at the first bad access with the stacks of the access, the free and the allocation.
Its flags go in by the environment, which CMake puts beside the port's toolchain's own (a
`-DCMAKE_CXX_FLAGS` would drop them, the sysroot's with them). A harness run takes it through
`DXCAP_ENGINE`:

```sh
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer" LDFLAGS=-fsanitize=address \
    cmake -S DXEngine -B <scratchpad>/asan -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build <scratchpad>/asan --target DXEngine
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 DXCAP_ENGINE=<scratchpad>/asan/DXEngine \
    tools/dxcap.sh dxe <console> <map>
```

### The software device

Its pipelines are proven against its float reference in the running game: a dxe run with
`DXE_SW_REFERENCE=check` ([the variables](ENGINE.md#settings-and-environment)) logs how far apart
the two came out (`the pipelines against the reference`). The general pipeline alone
(`DXE_SW_GENERAL=1`), a scalar build, the SIMD wrapper's loops on x86 (`DXE_SW_SCALAR`), and the
Windows build under Wine give an SSE2 build's bits, their shots its own but where two runs differ
([shots](#shots)): `LightConsole`'s `Spotlight7` the same to the bit all four ways. The scalar
build:

```sh
CXXFLAGS=-DDXE_SW_SCALAR cmake -S DXEngine -B <scratchpad>/scalar \
    -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake
cmake --build <scratchpad>/scalar --target DXEngine
DXCAP_ENGINE=<scratchpad>/scalar/DXEngine DXCAP_SETTINGS=tools/dxcap/software-settings.json \
    tools/dxcap.sh dxe <console> <map>
```

Its look against the Vulkan device's: a console run on each (`software-settings.json`,
`vulkan-settings.json`), `shots.py --levels` pairing the two ([shots](#shots)); against
`D3DDrv`'s, the original with `DXCAP_RENDERER=D3D`.

Its threads under ThreadSanitizer, built as the AddressSanitizer build is ([crashes](#crashes)),
in harness runs (`ProveConsole`, `PerfTurnConsole`, `CameraConsole`'s two scenes a frame,
`MirrorConsole`, `LaserConsole`, `FlushConsole`): the one report is OpenAL's own mixing thread's.

```sh
CXXFLAGS="-fsanitize=thread -fno-omit-frame-pointer" LDFLAGS=-fsanitize=thread \
    cmake -S DXEngine -B <scratchpad>/tsan -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build <scratchpad>/tsan --target DXEngine
TSAN_OPTIONS=log_path=<scratchpad>/tsan DXCAP_ENGINE=<scratchpad>/tsan/DXEngine \
    DXCAP_SETTINGS=tools/dxcap/software-settings.json tools/dxcap.sh dxe <console> <map>
```

Its speed against Mesa's CPU drivers, each through `PerfConsole`
([measuring both engines](#measuring-both-engines)): the Vulkan device on lavapipe
([the packages](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies);
`lavapipe-settings.json`) and the GL device on llvmpipe
(`llvmpipe-settings.json`), each with `CPUDriver`, without which a CPU driver alone gives way to
the software device ([which device draws](ENGINE.md#rendering)).

## Gotchas

The workspace's:
[gotchas](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#gotchas-that-cost-time).
Starting the engine (an option's value, the signals, `Running.ini`):
[running it](ENGINE.md#running-it). Profiling on a device, and the hooks off before changing
the engine: [the profiling hooks](ENGINE.md#the-profiling-hooks); on the desktop:
[profiling and validating](ENGINE.md#profiling-and-validating-on-the-desktop).

- **`compile` takes `DXCapture.u` away before it builds it again**: a run started meanwhile, or
  one of a sweep, finds no console class. Compile between runs, never under one.
- **A dxe run refuses to start over stray shots** in the game's `System` folder, or in a
  `DXCAP_USERDIR`'s `Screenshots`: the engine writes the first free `ShotNNNN.bmp` while the
  consoles number their marks from 0, so a shot left there would shift the run's numbers. Move
  them away.
- **A dxe run straight into a mission map leaves `Save/Current` as earlier runs left it**: only
  travel to another mission, a new game or a `?restart` URL empties it, and a save's load fills
  it with that save's maps. A run that returns to a map within its
  mission takes that map from there; a timeline that first crosses a mission starts it empty.
- **An unattended run sees no NPC react to the player.** Liberty Island starts the player 7,000
  to 20,000 units from every NSF, and no NPC reacts to a player it cannot see. `AIConsole`,
  `MoveConsole` and `ReachConsole` watch the AI's own work (states, routes, the reachability
  tests) without a player. To see NPCs react, a console class stands the player in front of one,
  at a spot written into the class, rather than a hook in the engine.
