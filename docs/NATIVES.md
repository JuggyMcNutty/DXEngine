# Deus Ex against the original

What DXEngine does for Deus Ex, feature by feature, each against the
original: its write-up in dx-reverse-info, the console that compares the two
engines, what matches, and what differs. What else it changes:
[what it changes from Surreal Engine](ENGINE.md#what-it-changes-from-surreal-engine).

## How it is known

- **The audit**, [`tools/natives_audit.py`](../tools/natives_audit.py):
  every native the game's packages declare against DXEngine's -- a stub, a
  partial or empty one, an iterator that makes none, or none registered --
  with its
  call sites in the game's scripts; `--runs` adds which stubs a run's log
  fired. A stub logs once a session, naming the script function that called
  it, so a run shows which stubs a map reaches, not how often. Run it for
  the current list of stubs; this doc does not copy it.
- **Reading the DLLs**
  ([dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info)): C++
  that is not a native leaves no stub, and only the original shows whether
  an implemented native does what it does.
- **Scripted runs of both engines**
  ([scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)), by the
  consoles named below.

## Stops the game

Differs. An error DXEngine does not catch -- a script error, an unknown
native, a failed save -- exits the engine with 1, which the launcher takes
for a crash. No map or action is known to reach one; the audit lists the
natives whose call would. A `foreach` on a None context, and state code's
`goto` to a label its state has not, stop both engines alike (the
original's Critical Error); a None class's `default` is 0 in both, and
`AllActors(None)` every actor. A dynamic array's element, on which the
original asserts (no Deus Ex script indexes one), grows the array to the
index in DXEngine, a negative index a script error.

A runaway script stops both engines alike, with an error no native's call
catches
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-script-interpreter)):
a loop past 10,000,000 jumps run, taken or not, in one level tick or one
map's load, "Runaway loop detected (over 10000000 iterations)"
(`LoopConsole`); a 251st nested script call, "Infinite script recursion
(250 calls) detected" (`CallConsole`). Differs: the original's
`-norunaway`, which lets the loop run on, is not carried; a save's load
counts on from its frame's level tick, where the original's, a map's load,
starts the count afresh.

## Saving, loading and travel

Matches the original
([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving),
[the save directory](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-save-directory)):
each engine loads the other's saves (`SaveConsole`, `LoadConsole`), and a
level returned to keeps its game (`ReturnConsole`). A level's start
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick))
runs each pass over its actors as the list grows, so a map's carcass holds
its items hidden in `Idle2` and its search hands them over whole
(`CarcassConsole`), and after `BeginPlay` finds every actor's zone where it
is, whatever region the map saved: DXMP_Silo's items, crates and robots
start in their zones, where the map's regions put them in none and a first
fall would take them out of the world (`VisibleConsole` there); every level
loaded, a save's or one returned to, then calls `PostPostBeginPlay`
(`LoadConsole`, `ReturnConsole`). A save loaded in the running game, from
the menu map or the level it was saved in, and a run started on it, spawn
what the original's do, class by class, second by second (`CensusConsole`,
`census.py`), and keep it, the game's decals going as
[out of sight](#out-of-sight) has them. A save's load logs the saved
player in as the original's does, with `?loadgame`: the game's `Login` keeps
it as saved, and its `TravelPostAccept` spawns the level's mission script,
which no save holds -- an object of a transient class is not written
(`LoadConsole` over either engine's saves and the original's hub save,
`ReturnConsole`). The
console's `open ?loadgame=N` loads the slot as the game's own `LoadGame`
does (`LoadConsole` with `Open`); a slot that is no number loads nothing in
either, DXEngine reading it as slot 0. The console's `SAVEGAME n` takes any
number as the original's `SaveGame` does: -1 the quick save, 0 a new slot,
and any other `Save%04d`, a negative one too -- `savegame -3` writes
Save-003, and `savegame -2` its SaveInfo to Save-002 with the level saved
into Current (`SlotConsole`). Differs: a bare `?loadgame`, which
the original's browse takes for no load, its menu map opened again, loads
nothing in DXEngine's. Before
a level is saved into `Current`, the augmentations, skills and flag base
that travel with the player go from it, a decoration in its hands too
(`PruneTravelActors`; `ReturnConsole`). The
player's goals, notes and key infos, which the game makes with `new(Self)`,
are in the level's package, as the original's `new` puts an object in its
outer
([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)):
a save holds them and either engine's load gives them back, on the island
and after a map exit's travel (`SaveConsole` and `LoadConsole`, also with
`Via`). A map exit's travel makes each object a travel property holds anew
in the new level, held by the new player and named by its class's count,
its text whole (`ReturnConsole` with `Items`: a goal, a note and a key,
their texts with `?`, `;` and `%` in them); a console's `open` carries none
of them, in either engine. Every element of a static array travels, and
each field of a struct (`ReturnConsole` with `Items`: two canisters'
`AddAugs`, the inventory grid's `invSlots`, the augmentation slots'
`AugLocs`, a vector and a rotator flag). A map
opened at a portal (`open <map>#<portal>`) starts the player at the
teleporter of that tag (`PortalConsole`). Differs: a level returned to from
`Current` keeps the map's name in its URL, where the original's names the
saved file (`..\Save\Current\<map>.dxs`), as `Level.GetLocalURL` shows
(`ReturnConsole`). The saved level is found whatever case the URL spells
the map in, with or without its `.dx`, as the original's file system finds
it: the stock exits spell `12_vandenberg_cmd` and `00_training`
(`RevisitConsole`: the island opened again as `01_nyc_unatcoisland` has the
crate left there). A travel's `PreTravel` goes to every actor of the level that
has the function, with no argument, as the original's `SetClientTravel` sends
it: the mission scripts' among them (`TravelConsole`).

While a map loads or a level is saved the screen shows one frame, painted
before the work, as the original's does
([the loading screen](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-loading-screen)):
the view and the windows at a tenth of their brightness, under the screen
flash (with the client's `ScreenFlashes` on, as the game's ini has it, or in
a net game), and the stock console's `LOADING`, `SAVING` or `CONNECTING` (a
join's map) centred over them in blue. A map's load, a save's load too,
first empties the pauser and clears the player's `bShowMenu`, set while an
in-game menu is up, which would hide the word; every save paints `SAVING`,
the quick save's too, and a travel within a mission shows `SAVING`, then
`LOADING` (`LoadingConsole`: the same frames in the same order, their shots
within 1% of the original's). Differs:

- The word sits a pixel lower: the console centres it at a half pixel for an
  odd size, which the original's canvas truncates and DXEngine's draws as
  it is.
- The original's draw ticks the console (`UConsole::PreRender`), so its
  painted frame ticks it once more; DXEngine ticks it as each frame's draw
  begins, after the level's tick, as the original's does (`HopConsole`,
  `SpinConsole`: an actor's ticks after the console's exit as the
  original's), and its paint does not.
- No frame shows `PRECACHING`: the original's shows it only with its
  driver's precache on, which the game's ini has off.

## Flags

Matches the original
([flags](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#flags)):
each engine finds the other's saved flags. Mission scripts come up as the
original's: `MissionConsole` over all 83 maps finds every mission map's
script running. `DX`, `DXOnly` and `Entry` are not mission maps, and
`12_Vandenberg_Tunnels` (no `MissionScript` actor) and `99_Endgame4` (no
`DeusExLevelInfo`) run none in the original either.

## Conversations

Matches the original
([`ConSys.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md)):
`SkipConsole` compares a skipped line's speech and each line's length, which
the script waits for, read from the sound's header as the original reads it
(an MP3's bits over its first frame's bit rate: 2.834 s for a line the
decoded samples make 2.847 s), `CaptureConsole` a jump to a comment's label,
`EndgameConsole` an ending skipped to its credits: 99_Endgame2's
conversation stopped, its Played flag, the mission script's timer and quote,
and the credits up at the level's fifteenth second, alike.

## What the player reads

Matches the original's parser
([`DeusExText.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusextext-dll.md)),
but for one quirk left out on purpose: at a comment (`NOTE`, `GOAL`,
`COMMENT`) with no end tag the original reads on past the text's end, so
`09_EmailMenu_ShipOps`'s listing depends on the memory after it. DXEngine
stops at the end, hiding the rest.

## Every NPC

What `DeusEx.dll` and `Engine.dll` do for every NPC, piece by piece.

### The native tick: `AScriptedPawn::Tick`

Matches the original
([the native tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-native-tick)),
run before the actor tick, in the original's order. A wound drips a drop
each 0.1 s at the most, however hard it bleeds, its `BleedRate` held
between 0 and 1 for the drop's period as the original holds it
(`CensusConsole`).

### Starting up

Matches the original
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
every actor starts a level 10 s undrawn and every dynamic one ticks in its
first tick, so a pawn out of sight starts from its state code (`AIConsole`).

### Changing state

Matches the original
([events and probes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#events-and-probes)):
a script's `GotoState` into the state the object is in only goes to the
label, with no `EndState` or `BeginState` and its probes as they are, so a
`Disable` in `BeginState` holds. Private Lloyd's walk to his chair on
Liberty Island meets it, and the chair's `Bump` sends him to his own
state's `CircleToFront`: he sits in `SitBreathe` from then on in both
engines (`SitConsole`).

### Hearing: the AI event system

Matches the original
([the AI event system](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-ai-event-system)),
saved in its layout
([saved](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#saved)):
each engine loads the other's saves with every listener. DXEngine also reads
the manager in a layout of its own, which some of its saves carry.

### Moving: wandering and tactical movement

Matches the original
([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving),
[reaching](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#reaching),
[the search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)):
`MoveConsole` (`move.py`) has all 52 of Liberty Island's pawns move within
15% (or 200 units) of the original's distance. Each that walks to a spot and
stops there ends where the original's does, within a unit and a half, though
25 to 110 units short of its distance: the console counts from its first
line, 2 s in by its own clock, which takes in the island's first frame, the
load's, longer in the original's ([a long frame](#small)): by then
DXEngine's pawns are further along their first walk. `ReachConsole`'s
144 answers, twelve pawns each asked from its home spot, are the original's
to the unit. A strafe's move timer counts at the speed the strafe sets, all
of `MaxDesiredSpeed` held to the speed asked, as the original's `StrafeTo`
sets it before the timer; the timer takes a pawn with no physics at 200
units a second and counts only while the pawn's physics run, as the
original's `setMoveTimer` and `performPhysics` have it, so a strafe or move
of a pawn with no physics never runs out (`StrafeConsole`). A jumper's
landing is judged by the zone its feet land in, each test move of the
jump's fall setting its feet's and head's regions as the original's test
moves set them, so a jump into a pain zone the pawn does not resist is
refused. A dead robot frozen in `Dying` for good is the original's own
behaviour, and the rest of a death is alike too (`DeathConsole`).
`Terrorist34`, walking west to `PatrolPoint86`, comes to a stop at x 1510
some 8 s in, still walking at it, and stands there until his move's time
runs out (`MoveTimer`) some 12 s later, then steps back and goes on: in
every original run, and in every one of DXEngine's held to the original's
120 frames a second, or to 60 (`MoveConsole`'s `Watch`, `move.py --pawn`). A
walker's heading drifts with the frame rate, in both engines alike: after
his turn toward the spot he drifts across his way, 0.06 units a second
toward the spot's side at 120 frames and 0.1 away from it at the original's
227 with vsync off, where he meets the wall half a unit further along.
DXEngine uncapped, at some 300, meets it further along again, and there in
some runs he slips past within two seconds.

## Out of sight

Matches the original
([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time),
[which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn),
[stasis and render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#stasis-and-render-time)):
whether an actor counts as drawn is tested piece by piece in the BSP walk,
and the grid's pixels are the original's: a surface's taken by its centre,
as the original's span buffer takes it by its lower right corner half a
pixel past the frame's middle; a proxy's rectangle, a sprite's by its upper
left corner and a mesh's, as a node's box, by its lower right, as the
original rounds them
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame);
`BspClipper::SetPixelSample`): at Liberty Island's start the same three NPCs
count as drawn in both engines, every other one hidden from the camera by
the level, as the traces have it (`AIConsole`). An actor
drawn on its own (`GC.DrawActor`, `Canvas.DrawActor`) counts as drawn
whenever its rectangle is on the frame, whatever is in front of it: an NPC
the vision augmentation draws through a wall stays drawn while it is in view
(`AugVisionConsole`). A decal counts as drawn when some of it shows, tested
against the spans before its surface fills them, as the original clips each
decal to what is drawn of its surface
([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time)):
the game's go when not drawn for a second, some 20 s after they land
(`DeusExDecal`), so `CensusConsole`'s blood splats, 90 s under fire on
Liberty Island, are 140 to 170 at the end against the original's 90 to 186,
of some 400 to 700 made in either. Differs:

- **One span buffer** for the frame, where the original keeps one per zone.
  A buffer per zone would change no test in 15,500 at the starts of Liberty
  Island, UNATCO HQ and Battery Park: the walls round a portal are drawn
  before what lies beyond it.
- **Drawing** is DXEngine's own box test, the depth buffer hiding the rest;
  only whether an actor counts as drawn is the original's.

## On screen

What is drawn and how it looks, against `Render.dll` and `D3DDrv.dll`.

### The view's width

Differs by choice: on a screen wider than 4:3 the view keeps the height a
4:3 screen shows and sees more at the sides (Hor+), where the original keeps
its field of view across the width and loses height. The game's scenes are
framed for 4:3: at 16:9, with the original's angle, the intro's "Deus Ex"
under the logo falls off the bottom; with DXEngine's it shows. At 4:3 and
narrower the view is the original's. The scene, the coronas, what the HUD
draws in 3D, the frob highlight's box and the meshes' level of detail
([mesh detail](#mesh-detail)) all take the one angle (`HorPlusFovAngle`), so
on a wide screen a mesh keeps less detail than the original's at the same
distance.

### Tiles

Matches the original
([tiles](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#tiles)):
every tile, a canvas's tile or text, a window's or a sprite, takes the
flags `D3DDrv` draws it with (`RenderDevice::TileFlags`): one of a texture
whose palette has alpha (the menu's TrueType font) blends by it unless
translucent, and a modulated one draws modulated and masked alone, filtered
whatever its smoothing, its colour 0 cut out. A window's tile, as a
canvas's, adds its texture's own masked bit
([the GC's styles](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#styles)),
so a masked texture draws masked in any style; the canvas's text, clipped
or not, takes its style's flags. `TileConsole`, on the canvas and in a
window over a grey box: `StaticLine` modulated (the line across a camera's
and the InfoLink's static, a quarter of it colour 0, black) at 1.02 of
`D3DDrv`'s brightness; `HackInfoBackground` modulated (the hacking screen's
message, all colour 0, dark grey) drawing nothing, at 1.00; in the window,
`HUDScopeCrosshair`, a masked texture, drawn `DSTY_Normal` at 1.00, its
lines alone; "NEW GAME" in the TrueType font, drawn opaque, at a
correlation of 1.00 to `D3DDrv`'s, its edges blended into the box (the
stock menus draw it translucent); on the canvas, clipped text drawn
`STY_Translucent` translucent.

### The scene's pixels

Matches the original
([its coordinates](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame),
[the screen's pixels](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-screens-pixels)):
a polygon -- a surface, a mesh's face, a decal -- is drawn half a pixel
right of and below where the frame's middle puts it, and a tile -- the
canvas's, a sprite, a corona -- at the middle, as `D3DDrv` draws them, on
every device; `OpenGLDrv` draws the polygons at the middle. Against
`D3DDrv`'s frames, `LaserConsole`'s wall and floor fit best unmoved on
Vulkan, with and without 4x multisampling, and within a quarter pixel on GL
(`shots.py --shift`): their grey levels 0.47 and 0.30 apart on Vulkan (RMSE,
of 255). `FaceConsole`'s troop, `MeshConsole`'s Paul Denton, the mirrors and
the light maps register so too ([mesh detail](#mesh-detail),
[mirrors](#mirrors), [lighting](#lighting)), as a sprite and the canvas's
tiles do ([sprites](#sprites), [tiles](#tiles)).

### The software device

Matches `D3DDrv`'s picture as the GPU devices draw it without their
multisampling, bloom and HDR ([rendering](ENGINE.md#rendering)), into an
8-bit frame as `D3DDrv`'s, its textures sampled as `D3DDrv` samples them
with the game's ini: bilinear, from the one mip whose texels are no larger
than the screen's pixels, where the GPU devices filter trilinearly with 8x
anisotropy, by their own choice. Against `D3DDrv`'s frames: `LightConsole`'s
`Spotlight0`, with its macro texture and without, and `Spotlight7` at 1.00,
as the GPU devices' ([lighting](#lighting)), and `Light33`'s floor within
1.02; `LaserConsole`'s wall and near floor 0.40 and 0.40 apart (RMSE, at
1024x768 with `NoDetail`), the Vulkan device's 0.39 and 0.39, and best
unmoved, its receding floor nearer, 0.66 apart where the Vulkan device's is
1.10; `LoadingConsole`'s text on the rows `D3DDrv` draws it, where the
Vulkan device draws some of its glyphs a row lower (point sampled on a
texel's edge). Against the Vulkan device's: `TileConsole`'s tiles and text
within a level, as are `DetailConsole`'s bands (`detail.py` as the Vulkan
device's). It draws where the original's software renderer does
([choosing the render device](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#choosing-the-render-device)):
with `-nohard`; where the game's ini names it
(`GameRenderDevice=SoftDrv.SoftwareRenderDevice`, the recreated launcher's
Software Rendering) and `Settings.json` names no device; and where the GPU
device does not start, as the original's `WindowedRenderDevice` takes
`GameRenderDevice`'s place, the error logged
([which device draws](ENGINE.md#rendering)). Differs:

- **Vsync** only through SDL's renderer: X11's and Win32's presents do not
  wait for it.

### Sprites

Matches the original
([sprites](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#sprites)):
a sprite draws upright, the texture's first row at the top, with its
actor's style -- `STY_Masked` masked, `STY_Translucent` translucent,
`STY_Modulated` modulated -- its texture's own flags and its `bNoSmooth`; a
render iterator's items with their proxy's, so a `ParticleGenerator` with
neither `bTranslucent` nor `bModulated` draws its particles masked. A
modulated sprite draws as every modulated tile ([tiles](#tiles)).
`SpriteConsole`'s icon, its colour 0 magenta, drawn `STY_Masked` and as
such particles, shows none of that colour in either engine: 0.0% of the
sprite's box and 0.7% of the particles' magenta in DXEngine's frame, none in
`D3DDrv`'s, the sprite's box at a correlation of 0.998 to `D3DDrv`'s; drawn
`STY_Modulated`, its box at 0.96 and within 1% of `D3DDrv`'s brightness.
A sprite animated once (`DT_SpriteAnimOnce`) shows the frame its life has
reached and holds the last, as the original's does (no run: the reading
settles it).
Differs:

- **A texture drawn both masked and not** shows its colour 0 in the
  original as the draw that first set it made its copy
  ([one copy a texture](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)),
  in DXEngine as each draw is: beside the masked ones, `SpriteConsole`'s
  `STY_Normal` sprite shows the icon's magenta in DXEngine, black in the
  original; drawn alone, magenta in both.

### Particles and lasers: render iterators

Matches the original
([render iterators](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-iterators),
[particles and lasers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#particles-and-lasers)):
`LaserConsole`'s laser fence, a still texture in its beams' slots, unlit and
lit, has each beam's brightest row within 2.5% of `D3DDrv`'s red, its two
lines a segment in `D3DDrv`'s rows (each row within 20 levels of 256 of
`D3DDrv`'s red, but the middle beam's top one, which `D3DDrv` draws and
DXEngine's beam, a fraction of a pixel thinner there, misses), and one
segment of each beam drawn twice, picked at random each frame as the
original picks it; and `ParticleConsole`'s particles rise, fall, grow and
fade as the original's: its smoke's column at 1.06 to 1.10 of `D3DDrv`'s
brightness, its puffs under gravity in the same rows. Differs in the
occlusion: the original occludes each item's sprite through its span
buffer; DXEngine clips items to the view (and a portal's spans) before its
BSP walk and leaves the rest to the depth buffer. The look is the same; a
hidden item's cost is paid. Whether the proxy counts as drawn is the
original's.

### Coronas

Matches the original
([coronas](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#coronas);
`CoronaConsole`): the pass runs only with the render device's `Coronas` on,
which the game's ini turns off for SoftDrv (`CoronaConsole` with
`Coronas=False`: no glow in either engine); the static lights reaching the
player's leaf get coronas,
and no dynamic light does, the original's pass reading the leaf's dynamic
lights only after its occlusion pass has emptied them. `CoronaConsole`'s own
movable lamp shows none in either engine, in the player's leaf or out of it,
and logs the same BSP leaves in both. A light behind a glass mover shows
none in either: the line to it is checked against the level and the movers
with every surface, the invisible collision hull round the glass's pane
among them, which a sight test passes (`GlassConsole` at 03_NYC_Airfield's
`BreakableGlass2`, `Light56` beyond it).
An animated or fractal corona skin shows its current frame (no run: the code
settles it).

### What a pawn holds

Matches the original
([a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments));
`HeldConsole`.

### Viewport windows

Matches the original
([a viewport window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#a-viewport-window)):
a new window is enabled, at a field of view of 90, with a player's view
rotation and both eye heights used, and looks from the player's eyes; each
setter takes the original's defaults and clears what the original's
clears. The frame is drawn from the origin actor, up its `EyeHeight`, or
from the window's own place, at the window's field of view, with the player
in behind view for it, so the player and what it owns are drawn as in third
person, and the origin actor hidden unless `bShowActor`. So a security
computer's feed shows the player in the camera's view, its arrow keys pan
the camera by `fov / 90` of a step and its plus and minus keys zoom, and
the spy drone's window and the targeting augmentation's zoom, at its top
level aimed at the target's eyes, are drawn (`CameraConsole`: every value
it logs the original's; the feed within 0.5% of the original's frame, the
zoom and the drone's window within 1% of `D3DDrv`'s). Differs, none of it
reached by the game:

- no `CalcView` event is sent, and an origin or watch actor gone leaves the
  view where the window's fields put it, not at its last place and turn;
- a window turned off draws nothing, where the original tiles its default
  texture; `SetRotation` keeps the watch actor, and `SetDefaultTexture`
  takes no defaults;
- `bShowWeapons` draws no overlays; `SetViewportActor` adds no network view
  (`AdditionalViews`);
- `ConvertVectorToCoordinates` in a viewport window, or a window inside
  one, projects through the main view, not the window's own;
- the frame takes the window's whole rectangle, not its clip rectangle, and
  a window wider than 4:3 sees more at the sides
  ([the view's width](#the-views-width)).

### Mirrors

Matches the original
([a mirror's frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)):
a mirror's frame is seen from the eye reflected in the mirror's plane, its
BSP walked front to back and its faces, a mesh's too, culled from there; it
starts in the zone on the viewer's side, and the mirrors of one plane seen
from one zone share one. A mirror seen from behind opens no frame, and one
opens a frame three frames deep at most, a sky's and a warp zone's counted;
a translucent mirror reflects only while the render device's
`ShinySurfaces` is on, and is drawn solid without it. The player shows in a
mirror: `bOnlyOwnerSee` and `bOwnerNoSee` go by the player's `bBehindView`
in every frame, and the player is left out of the main frame alone, in
first person. `MirrorConsole`, at 4:3, on GL and on Vulkan:

- **Liberty Island's UNATCO HQ lobby**, its marble floor a mirror, turned
  and stepped across: the furniture, a mannequin and the player show in its
  reflection from every view, the right way out, as in the original's
  frame.
- **UNATCO HQ's glass mirrors**, facing -x, -y and +x, and the Paris club's:
  the room and the player reflected, each mirror within 2 to 5% of the
  original's brightness, through `OpenGLDrv` and `D3DDrv` alike.
- **A mirror in a mirror**: the training map's exit sign, a strip of mirror
  under it, in the marble floor's reflection, as the original's.
- **An actor across the plane** (`Straddle`): the mannequin sunk half its
  height into the lobby's floor, the part under the floor kept out of the
  reflection, as in the original's frames.
- **`ShinySurfaces` off**: the lobby's floor solid marble in both.
- **A mover's mirror**, as a BSP mirror, the original filing a mover's
  faces into the BSP
  ([which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn)):
  `GlassConsole` with `Flip` on 06_HongKong_WanChai_Underworld's
  `BreakableGlass0`, an opaque one, its reflection, the frame's middle, at a
  correlation of 0.985 to `D3DDrv`'s frame, and on 02_NYC_Smug's
  `DeusExMover7`, a translucent one, 0.997; `OpeningConsole`'s view of the
  club, a doorway's glass in it, 0.987 over the frame.

Differs:

- **The mirror's plane clips** what lies behind it: in the BSP walk, and on
  every device by a clip distance, or on OpenGL ES without one (the Smart
  Pro's) by a discard in the mirror's frame alone. The original keeps what
  lies behind a mirror out with a span buffer per zone.
- **A mover's mirror** is no part of the BSP walk: before it, each mirrored
  face of a shown mover opens a frame of its own, over all of the face the
  frame shows, and the depth test leaves out what lies in front of it,
  where the original's walk clips the face by what is drawn in front of it
  and shares a frame between the mirrors of one plane.

### Mesh detail

Matches the original's vertex budget and its morph
([mesh detail](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-detail)):
`MeshConsole` with `LODBias` 0.004 and 0.0015, Paul Denton cut down to a few
vertices, has his shape at a correlation of 0.95 and 0.97 to `D3DDrv`'s. On
a screen wider than 4:3 the budget takes the wider angle
([the view's width](#the-views-width)). An environment-mapped face reflects
the eye's line in its normals in the world's axes, as the original's
([mesh textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-textures)):
`MeshConsole` with `Enviro`, the four drawn so, has their streaks where
`D3DDrv` has them. A face's texture lies as the original's, its corners'
bytes ÷ 256 of the texture's size: `FaceConsole`'s UNATCO troop at a
correlation of 0.997 to `D3DDrv`'s frame, its chest 0.999.
A budget past an int's range (a script's `LODBias` or `DrawScale`) is held
to the mesh's vertices.

### Lighting

Matches the original's light maps, mesh lighting and the faces a mesh draws
([lighting](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#lighting)):
`ViewConsole`, `LaserConsole`, `MeshConsole` and `CoronaConsole` come out
within a level or two of 256 of `D3DDrv`'s frames. A light that leaves a
surface's moving lights -- switched off, destroyed, moved or shrunk out of
reach -- and a spotlight that turns, or whose cone, period or phase a script
changes, build the surface's map again, as the original's dynamic map, built
every frame from the lights then over it, has them (`SwitchConsole`: a
lamp's pool gone as it is switched off, a spotlight's beam moved as it
turns). A spotlight's cone is the
original's, its cosine `1 − LightCone ÷ 256`, its value rounded down, and a
light or fog map lights texel i at its centre, `Pan.X + i × UScale` along U,
where both engines draw it
([a texel's place](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)):
`LightConsole` has 03_NYC_Airfield's `Spotlight7` and Liberty Island's
`Spotlight0` at 1.00 of `D3DDrv`'s frames, with DXEngine's detail textures
off as the original draws none (`NoDetail`), in blocks of 16 pixels within
0.98 to 1.03 across `Spotlight7`'s wall and `Spotlight0`'s world map, inside
its border ([brightness](#brightness)). A surface's light map holds
the ambient of the zone it is seen from, and pans at that zone's speeds, as
the original's draw list has it: 06_HongKong_WanChai_Underworld's floor
under its disco lights `Light2` and `Light21`, lit by its zone's ambient and
one steady light, comes out at 0.99 of `D3DDrv`'s frames. A surface on a
plane's other coplanar nodes, which may face the other way, takes the zone
on its own side, as the original's: the wall left of that map's start takes
its own zone's ambient, not the LevelInfo's of the first node's side
(`OpeningConsole`). Its fog map is
drawn only with the render device's `VolumetricLighting` on and the
viewport's actor in a fog zone, for a surface not translucent whose zone is
a fog zone too, as the original's renderer has it: 02_NYC_Underground's
floor under the water below `Light33`'s grating, seen from the fog zone
above, is unfogged in both, the frame within 1.02 of `D3DDrv`'s
(`LightConsole`). The other effects' shapes are Render.dll's own, on the
level's time. `LightConsole`, shooting both engines at one moment of it
(`ShotAt`), has 06_HongKong_WanChai_Canal's disco `Light343`,
02_NYC_BatteryPark's cylinder `Light81` and 09_NYC_ShipBelow's slow wave
`Light16` within 0.99 to 1.01 of `D3DDrv`'s frames, correlated at up to
0.997, 0.998 and 0.98 (`NoDetail`). The rest are the reading's alone: the
stock views of the searchlight, non-incidence and the rotor come out the
same under other shapes too, and no stock light shows a fast wave
(99_Endgame2's one is dark), shock, shell or interference.
DXEngine's own:

- **Light maps**
  - floats, converted on the CPU for a GPU that cannot filter them (the
    Smart Pro's; [rendering](ENGINE.md#rendering));
  - a map whose lights changed built and sent again only in part
    ([rendering](ENGINE.md#rendering)), where the original builds and
    uploads it whole every frame;
  - the lookup each surface's own last map, else a `std::map`, where the
    original's cache hashes and first checks the item it found last;
  - a still-shaped animated light re-run, where the original keeps it as its
    shadowed light and rescales it;
  - blink and strobe on DXEngine's timing; the original's blink goes by the
    lowest bit of its turn count and its strobe turns over each frame, both
    at the frame rate;
  - a flicker drawn at most 25 times a second;
  - `LE_CloudCast`, the plain shape, built once, where the original re-runs
    it every frame (its table counts the shape as changing): the same light.
- **Meshes**
  - the moving lights come from DXEngine's light tree near the actor, not
    the leaf's own list; its fog from the level's first four volumetric
    lights in memory order, where the original takes the leaf's own list
    of them by a vertex model of its own, unread: a mesh in a fog zone is
    faintly fogged in `D3DDrv` and unfogged here (`FogConsole`:
    02_NYC_Underground's crate 220 units into the start's fog is at 0.09 of
    the wall beside it in `D3DDrv`'s frame, 0.25 in DXEngine's; the fog
    balls' model with the lights that reach it fogs it to the wall's
    brightness, so the model is what differs, not the lights);
  - the `AmbientGlow` pulse runs on the level's time, the original's on the
    viewport's.

### Brightness

Matches the original's picture
([gamma](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#gamma),
[the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness),
[textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)):
the Brightness slider raises the picture to 1 / (2.5 × Brightness), as
`D3DDrv` sets its ramp, and `Settings.json`'s other gamma mode (XOpenGL's
curve) takes the same product. The shaders take nothing off a texture's
colours (`NO_DARKCLAMP`), as `D3DDrv` draws 32-bit textures on any
display of 24 bits or more, on every device: `MeshConsole`'s shots under
Vulkan, with descriptor indexing and without (the Smart Pro's), are within
0.2% of the GL device's. How a DXEngine shot meets the original's frames:
[scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines). Differs:

- `MeshConsole`'s Liberty Island pier, where nothing is in hand, is 1.7%
  under `D3DDrv`'s frames with the detail textures on, and 0.7% over with
  them off in both (`NoDetail`): DXEngine draws the pier's detail texture,
  the original none ([the menus' settings](#the-menus-settings)). Its crate,
  box and barrel are within 0.8%, Paul Denton a level over, and
  `LaserConsole`'s corridor 2% under, within 0.5% with them off.

### The screen flash

Matches the original
([the rest of a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-rest-of-a-frame)):
`FlashConsole`, and over the windows the loading screen's
(`LoadingConsole`). The player's `bNoFlash` stays `DeusExPlayer`'s default,
True: Deus Ex declares it no config property, so neither engine reads the
`bNoFlash=False` the game's `User.ini` lists. Its `ViewFlash` then drops
every instant flash and fog, and the death's spiral round the carcass shows
none of the fade to white and to black its state's `PlayerCalcView` asks
for (`DyingConsole`, at 60 frames a second).

### Draw order and movers

Matches the original's order
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)):
after the portals, the opaque surfaces with their decals and the solid
actors; then, each back to front, the parts of the actors not drawn
translucent (a mesh's translucent faces, a sprite of another style), the
translucent and modulated surfaces with their decals, the movers' among
them, and the actors drawn translucent (`STY_Translucent`), a render
iterator's items by its proxy's style. So glass blends over a mesh or a
sprite behind it in both engines. `GlassConsole`'s glass movers come out as
the original's frames: the room seen through 05_NYC_UNATCOMJ12lab's
`BreakableGlass16` and 03_NYC_Airfield's `BreakableGlass2` within 2%, and
behind the first an unlit crate within 0.3% and an unlit sprite drawn
`STY_Normal` within 0.5%, the glass adding 7.8 of 255 over the sprite to the
original's 8.5. After the world the depth buffer is cleared and the
player's overlays drawn
([the overlays](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#the-overlays)),
so the first-person weapon meets no depth of the world: an assault rifle
held flush against a wall of UNATCO HQ draws whole, within 1.3% of the
original's frame (`WallConsole`). Differs (`VisibleFrame::Draw`,
`VisibleMesh`, `VisibleBrush`):

- **Back to front** by the distance of one point of each, where the
  original's lists run in the order its BSP walk met them.
- **The opaque surfaces** go by zone, texture and light map, as the
  original sorts them by zone and texture, its order among one texture its
  sort's own: where two of them overlap at one depth, which shows can
  differ (the gaps between the Liberty Island pier's planks, a few hundred
  pixels of `MeshConsole`'s frame).
- **A mesh's translucent faces** are drawn after all its opaque ones. The
  original draws them in face order among them, so one drawn before an
  opaque face behind it is lost where that face writes over it. Of the 61
  meshes with translucent faces on 13 maps counted, only `GM_Jumpsuit`,
  `GM_Suit`, `GFM_TShirtPants` (and their carcasses') and `ComputerPublic`
  have opaque faces after them; `FaceConsole`'s close-ups of a UNATCO troop,
  an MIB and Anna Navarre, face on, come out the same in either order, the
  MIB's lenses within 0.5% of `D3DDrv`'s.
- **Movers** are drawn as their brushes, through their own matrix, a step
  toward the viewer ([rendering](ENGINE.md#rendering)): their opaque
  polygons with the solid actors, their translucent ones among the
  surfaces. The original files their polygons into the level's BSP and
  drops what lands in solid, a polygon in a wall's plane filed in front of
  the wall's. So one flush with a wall shows over it in both, by the step
  in DXEngine: `FlushConsole`'s pocket door (Liberty Island's
  `DeusExMover12` slid half into the flat wall beside it) shows the door,
  head on and from 1,200 units at 75 degrees. No device here fights there
  without the step (GL and Vulkan on the desktop, Vulkan on the Smart Pro:
  the shots within 15 pixels with it and without); the step keeps the
  order whatever a GPU's rounding. A mover's one-sided polygon seen from
  behind is passed over, as the original's walk passes over a surface. A
  mover hidden after it is filed stays drawn there in the original, where
  DXEngine stops drawing it (`GlassConsole`); no stock script hides a
  mover.

### Fire, water and ice textures

Matches `Fire.dll` step for step
([`fire-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md)),
checked against its own routines run in an emulator. Its random bytes come
from the C library's `rand`, seeded from the clock at start as the
original's
([the platform's start](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-platforms-start)),
so a fire differs from run to run in either engine (`LaserConsole`'s
texture shots). A texture moves on whenever something draws it, a window
too: the drugged view's `DrunkFX`, a water texture only the HUD draws as
its background, modulated over the view (`DrugConsole`). How a mesh shows
its textures is the original's too
([mesh textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-textures)).
Differs:

- **The pace**
  ([stepping](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md#stepping)):
  a texture with no `MaxFrameRate`, as most of Deus Ex's have, steps every
  frame in the original, faster at a higher frame rate; DXEngine steps it at
  most 60 times a second, the original's pace at 60 frames. A texture steps
  by the time since its own last update, however often it is drawn, as the
  original's: one with a `MaxFrameRate` drawn every other frame steps as
  often as one drawn every frame, and one drawn every ninth frame at 60
  steps at every draw (`StepConsole`).
- **The wave lighting table** can be one step off at a single entry, in 3 of
  200 settings: DXEngine takes the C library's cosine, the original the
  x87's, and they differ in the last bit.

### Head turns and lip sync: blend animations

Matches the original
([blend animations](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)),
as the main animation's tick does
([the main animation's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)):
four passes a tick, each ending at a notify the step crosses (one the actor
has no function for too), a loop's wrap past 1.0 (its `AnimEnd` sent when
the pass crossed `AnimLast`) or a sequence's end, so a long tick through a
fast loop plays four loops of it and sends four `AnimEnd`s, the rest of its
time unplayed (`PassesConsole`: a tarp's Blowing looped at 20 a second
through a 0.4 s tick, four `AnimEnd`s and frame 0 after in both).
`FinishAnim` waits as the original's does: not at all with nothing playing
or tweening, or with the frame at `AnimLast` or past, else until the
animation finishes -- a tween's whole time after a `TweenAnim`
(`FinishConsole`). A sequence name the mesh lacks is none: `HasAnim` false,
`GetAnimGroup` None, and `PlayAnim`, `LoopAnim`, `TweenAnim` and
`PlayBlendAnim` of it play nothing, as the original's `GetAnimSeq` answers
(`AnimNameConsole`); an actor whose `AnimSequence` the mesh lacks -- None,
most decorations' -- is drawn at the mesh's first frame, as the original's
`GetFrame` draws it. `PlayAnim`, `LoopAnim` and `TweenAnim` start as the
original's do: with no tween time given, a frame's tween at the last
animation's rate (in 0.025 s a frame after a tween or nothing playing, at
the velocity's rate after a velocity-scaled one), none from nothing playing;
a frame over the time given; none with 0, the frame set a shade past 0; a
one-frame sequence tweened whole over the time, or in 0.1 s; and the same
sequence looping on takes its new rate alone (`TweenConsole`).

### Lists

Matches the original
([lists](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#lists)):
the rows, fields, sort order and selections that `MenuConsole` and
`ScreenConsole` log are alike in both engines. A list shown with rows
selects and focuses its first, so the Images screen opens on its first
image; a float field reads as its shown number. The hot keys and a drag's
selection are the original's, read from its code; a list in a clip window
fills it, and scrolls a row at a time (`ScrollConsole`). A column fitted to
its fields measures each in the column's font as the GC draws it, its
widest line, a character the font lacks taking no room; a column given a
new font, `None` the list's own, widens auto-expanding columns to their
fields (`ColumnConsole`'s widths alike). A list draws as the original's
does, in three passes with word wrap and special text off: each selected
row's highlight across the row; each field on one line in its column's
font, inset by the column and row margins and cut at the cell's edge, a
code such as `|c` shown as typed; and the focus row outlined in the focus
texture, `focusThickness` thick, only while the list has the keyboard
focus (`ListConsole`: the Load Game screen with a 50-character save name
and one named `a|cff0000b`).

### The UI

Matches the original
([the UI in front of the game](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-ui-in-front-of-the-game),
[layout](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#layout),
[text](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#text),
[buttons](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#buttons),
[the computer window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-computer-window),
[scales and scrolling](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#scales-and-scrolling),
[tiles and tab groups](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#tiles-and-tab-groups),
[drawing](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing),
[small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)).
The consoles: `GetConsole`, `BeltConsole`, `BorderConsole`, `ColorsConsole`;
`ChoiceConsole` for a conversation's choices, by the keys and the pointer;
`ThemeConsole` for the InfoLink's text in the HUD theme; `SizeConsole` for
a new window's size; `ScrollConsole` for a scroll area and a slider;
`ClickConsole` for a button, a choice and a checkbox clicked, a menu dragged
by its title bar and Alt with a letter; `EditConsole` for a script's text in
an edit field, a text window and a button; `MultiEditConsole` for an edit
field of several lines. `MenuConsole` and `ScreenConsole`
log every menu and game screen's windows (their places, sizes, text, focus,
a slider's value text, a scroll area's bars, a tile's children and a list's
rows), alike in both engines but for what is listed below. The same, read
from the original's code:

- **A new window** starts hidden, 10 by 10; its parent and ancestors hear of
  it before its `InitWindow`, and it is shown after, so `VisibilityChanged`
  reaches everything its `InitWindow` made.
- **`IsVisible` and `IsSensitive`** recurse unless told not to, up through
  the parents: true when the window and every ancestor is shown, or
  sensitive, so a HUD window under the hidden HUD is not visible;
  `IsVisible(False)` is the window's own flag (`ShownConsole`). A window
  made insensitive, or whose parent is, has the focus and a grab checked
  as hiding checks them, and `SensitivityChanged` reaches it and its
  sensitive descendants with what is in effect, as the original's
  `SetSensitivity` has it; a focused button disabled keeps the focus in
  both engines (`FocusConsole`).
- **Places**: `SetPos` makes a window left- and top-aligned with its x and
  y as margins; `SetWindowAlignments` sets both margins of each side, the
  second the first's unless given. A parent's `ConfigureChild` sets just
  what it gives; a window no parent places, or that the base script's
  answer to its asking hands back (`ResizeChild`), sits by its alignment at
  its preferred size -- centred with the half dropped, plus its margin --
  and its `x` and `y` are that place. So a menu dragged by its title bar
  follows the pointer and the credits start below the screen
  (`ClickConsole`, `MenuConsole`).
- **Sizes**: a side a window was given (`SetSize`) counts as asked for; a
  side it leaves unset is its background's size, else its own -- an empty
  text window is 0 wide and keeps its height. A size set is held at 0 or
  more and lays the window out again, its parent asked, only when it
  changes, as the original's `Resize` has it; `ResetSize` lays it out again
  only when a size was set. A large text window and an
  edit field measure rows of the font's height with their vertical spacing
  between (`ScreenConsole`'s skill text and note), an edit field a space
  wider when no width is given. A window asking to be laid out again is
  laid out itself too.
- **Text from a script** (`EditConsole`): a text window takes no text that
  differs only in case. An edit field's `SetText` replaces its text as
  typing would -- its undo cleared, `TextChanged` announced up its
  parents -- and leaves the insertion point at the start, as `AppendText`
  does after adding at the end; so the save screen's Save Game button
  wakes for the name it fills in. An edit leaves the insertion point after
  what it put in, and what it put in selected when asked to.
- **Edit fields** (`MultiEditConsole`): rows as a large text window breaks
  them, within the margins. Up and down a row or a page keep the column
  the insertion point was last put at; a word either way within a row, a
  row's start and end, the text's; a drag leaves the hook where it began
  (each move from fourteen places, chains of them, a selection, a single
  line's: the logs alike). A single line does not word wrap. With the
  focus, a selection is drawn row by row behind its text, the text in the
  inverse colour, then the insertion point, blinking each half period
  (the field's shots alike, every pixel within 24 levels); else the rows
  as a large text window draws them. A press puts the insertion point at
  the pointer and drags it, whole words on a second click.
- **Buttons** activate on the release, a repeating one on the press and
  again while held; a right click where the button takes them; each with
  its sounds, and a press from the keyboard shown pressed for its delay. A
  toggle's state is its pressed look; a radio box keeps one of its toggles
  on.
- **Scales** keep a tick among their ticks: a slider's value is its tick's,
  its text the tick's own or the value printed; the thumb drags, a click
  beside a scrollbar's thumb pages, held, again and again. A scroll area
  shows a bar only while what it holds does not fit, its clip window moving
  it a unit at a time (a list's row, a text's line) and bringing the focus
  into view; the wheel steps it.
- **Keys** go up from the focus window to the root, so Esc and F1 close a
  screen; **mouse buttons**, only while a modal shows, to the window the
  mouse acts on (the grab, else the one under the pointer in the topmost
  modal, else that modal), then up its parents. A press grabs the mouse for
  that window, so a drag's moves and its drop reach the item it started on,
  and counts as a multiple click within 0.5 s and 10 units of the first.
  The root marks every press and release: a press of a key already down is
  a repeat, and a release of a key or button not down is taken while a
  modal shows and goes to no window (`KeypadConsole` with
  `stray-release.txt`, DXEngine's alone; with no modal up the game gets it).
  `IsKeyDown` reads those marks. Typed text reaches the console and the
  windows a character at a time, as the original's comes (a `WM_CHAR`
  each), one past ASCII by its Latin-1 code: `TypeConsole`'s edit field
  takes "Café", "ñü" and "ab" typed whole (`type.txt`, DXEngine's alone).
  `LootConsole` searches two carcasses alike in both engines; the keys and
  the drags between the searches (`loot-drags.txt`) are a timeline's, so
  DXEngine's alone, as are `ScrollConsole`'s clicks, drags and wheel
  (`scroll.txt`) and `ClickConsole`'s clicks, drag and keys (`clicks.txt`).
- **The pointer**, while a modal shows: a move that puts another window
  under it leaves the last one and enters that one, each told alone, never
  its parents, and `MouseMoved` goes out only for a new place. The topmost
  modal's mouse focus mode moves the focus with it. Every modal, the root
  too, starts at `MFOCUS_Enter`: the window entered takes the focus, its
  nearest selectable one if it can take it now. `MFOCUS_EnterLeave`, the
  keypad's, also clears it on leaving; `MFOCUS_Click` gives it on a press
  alone. A window that appears under a still pointer is not entered. So a
  conversation's choice and a menu's button take the focus the pointer
  moves onto (`ChoiceConsole`: the pointer put on a choice by
  `SetCursorPos`, which either engine takes as a move). It is drawn only
  then, while `ShowCursor` leaves it: a modal window made hidden, or hidden
  again, draws none in either engine (`PointerConsole`). The pointer drawn
  is the one `CursorRequested` gives, asked up the windows from the one
  under it, else the nearest default, with its shadow, hot spot and
  colour, as the original's `QueryCursor` chooses it: a dragged item's
  icon, a menu's tinted arrow, the shadow under it (`PointerConsole`'s
  shot with the modal window shown, the arrow's pixels within 31 levels
  of the original's).
- **The InfoLink's text** (a computer window): each character printed is
  due a time step after the last, `SetTextTiming`'s 0.03 s, on the
  windows' real time; each tick types those due, in the colour
  `SetTextFont` queued before them, faded in from black over the fade time.
  A row leaves its last column empty, and a word that would reach it starts
  the next row whole; past the last row the text scrolls. The cursor, a bar
  under the next cell in the header text's colour, is lit by each character
  and blinks every 0.5 s. A clear waits its turn, so a speech stays up
  until the next starts typing (`ThemeConsole`).
- **Accelerators**: a text's `|&` marks its window's key while the text is
  its accelerator -- every text window's from its start, but a large text
  window's. Alt and a key the scripts leave press, in the modal on top, the
  first window with that key -- the modal, then its children bottom to
  top, each in turn -- that shows, is sensitive and takes the focus, a
  letter in either case (`ClickConsole`: Alt+B opens Brightness from the
  Display screen, Alt+C cancels it).
- **Keyboard focus**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#keyboard-focus)):
  a tab group's windows in order of where they lie on screen; `MoveFocus`,
  `MoveTabGroup`, `SetFocusWindow` (only a window that can take the focus
  takes it, its modal keeping it to return to), the focus moving off a
  window hidden, made unselectable or destroyed, and the root seeding it --
  none while the keypad (`MFOCUS_EnterLeave`) is up, none with no modal up
  (`KeypadConsole`).
- **The windows** tick by real time since their last tick, not the level's,
  and a held button's first repeat counts what of the frame had gone.
- **Timers**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#timers)):
  each is due its time out after it was added, the frame already gone
  counted in, and calls its function once a tick, the newest first, with
  its id, how many of its time outs have passed and its client data
  (`TimerConsole`'s logs alike, both engines at 60 frames a second).
- **A GC's style** sets only how tiles draw: text stays masked, translucent
  only by `EnableTranslucentText`, so the frob label over its modulated
  backing reads as the original's (`FrobConsole`'s shots). A box
  (`DrawBox`) is four strips of its pattern that do not overlap, each tiled
  from the origin given, and a box no wider or taller than two thicknesses
  is filled once, as the original's `XGC::DrawBox` draws it, so the frob
  label's translucent border has corners as bright as its sides
  (`FrobConsole`'s shots).
- **The scale** is the original's, its root's one multiplier: the largest
  whole multiple of 640x480 the screen holds, at least 1
  ([the root window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-root-window)).
  `SizeConsole` at 1600x900 and 1276x960 (a scale of 1 at both),
  `MenuConsole` and `ScreenConsole` at 1600x900: the windows' places and
  sizes alike.
- **A point on the screen** (`ConvertVectorToCoordinates`), as the frob
  highlight's box asks it
  ([a point on the screen](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#a-point-on-the-screen)):
  projected by the main scene's view into the root window's coordinates,
  truncated to whole units, and in front of the eye or not wherever it
  lands, so the box takes in a corner off the screen; a point behind the
  eye at the view's middle, in whole pixels. `FrobConsole`, its points
  asked from a window on the root (`ProjectionWindow`) at 1024x768: every
  answer the original's, a point far off the screen and one behind among
  them, the places within 2 units of the original's (its view turned by its
  sine table:
  [the engine's own rotations](#implemented-not-as-the-original));
  its boxes round two decorations, the crosshair on their middle and turned
  5 degrees four ways, within 7 pixels of the original's at 1280x720 with
  the original's [view width](#the-views-width).

`Canvas.DrawActor` and `Canvas.DrawClippedActor` leave their actor hidden,
whatever it was, as the original's natives do, where `GC.DrawActor` puts the
flag back (`OverlayConsole`: the pawn drawn visible and drawn hidden, hidden
after each, in both engines); `DrawClippedActor` draws from an eye at its
own frame's origin.

Differs:

- **`GC.DrawActor` ignores `bConstrain`**: the vision augmentation's calls
  cover the whole view. Its render time: [out of sight](#out-of-sight).
- **`Canvas.DrawActor`'s and `DrawClippedActor`'s `WireFrame`** draws the
  actor solid: DXEngine draws no wire frames, where the original draws the
  call with the player's `RendMap` at wire (the game passes false).
- **A typed character past Latin-1** (U+0100 on) goes nowhere, DXEngine's
  text being 8-bit; the original's windows take it, and the game's fonts
  draw it as nothing (read from `WinDrv.dll`'s code). A string the original
  stored as wide characters (such a note in its save; every conversation
  comment of `DeusExConText.u`) loads, each character past Latin-1 a `?`.
- **Keys**, read from both codes: the keyboard counts as grabbed while a
  modal shows, where the original counts its grabs (every modal grabs while
  shown). With Alt down DXEngine takes a letter or digit from the key's
  press, not from the characters the platform sends, and it forgets the
  keys held as its window loses the keyboard.
- **Keyboard focus**: DXEngine places windows in its layout pass, after the
  level's tick, and seeds the first focus there; the original places them as
  they are made and seeds it at its windows' next tick, so a conversation's
  first choice has the focus a frame sooner (`ChoiceConsole`). Read from
  both codes, the tables pass no window over as clipped away, DXEngine
  keeping no clip rectangles.
- **Layout** (`MenuConsole`, `ScreenConsole`, `SizeConsole`): DXEngine lays
  windows out once a frame, the original as each asks, so a window made this
  frame is 10 by 10 until the next; a window that shows sits at the margins
  `SetPos` gives it at once, where a parent that places it again does so the
  next frame. A window a parent placed keeps that place until it moves or
  asks again, where the original's goes back to its alignment when the
  parent's next layout leaves it out; read from both codes, a hidden window
  takes a parent's `ConfigureChild`, where the original's only counts itself
  placed. The root window is 4:3 (960 wide at 1280x720) by choice, where the
  original's is the whole viewport: the stock
  `ConWindowActive.CalculateWindowSizes` gives a cutscene's subtitles no
  height on a root 16:9 or wider, the Smart Pro's among them, so a cutscene
  letterboxes at 16:9. The root's children lie across the screen up to 16:9,
  their `x` the screen's. On a screen narrower than 4:3 (1280x1024) the root
  is the screen's width, as the original's, so the HUD's left and right
  parts lie at its edges.
- **A large text window** draws its lines through the GC, read from both
  codes, whose block of lines ends in one more spacing than the original's
  rows: in a window taller than its text, lines centred or at the bottom
  sit up to a pixel higher. An edit field draws its own rows, as the
  original's.

## The menus' settings

Matches the original: every setting the Controls, Game Options, Display,
Sound, Colors and multiplayer Player Setup screens change, stepped through
its menu and saved with OK, reads back alike in both engines -- the menu's
value and its setting's `GET` -- and steps back (`SettingsConsole`). A
volume slider moved but not yet saved plays its new level while `GET` still
reads the saved one, as the original's subsystem and class default differ.
With Decals off no new decal attaches
([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small))
and the ones on the walls are not drawn
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame);
`DecalConsole`). World and Object Texture Detail draw the world's textures
and the skins from one, two or three mips below the top at Medium, Low and
UltraLow
([a texture's mips](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-textures-mips)),
World Texture Detail below High dropping the detail textures too. Low Sound
Quality makes each WAV 8-bit, and one of 22,050 Hz or more half its rate, as
it loads
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)).
Full screen, Screen Resolution offers the display's modes from 640x480,
and a choice, or `FullscreenViewportX` and `Y` as the game starts, takes the
display's nearest mode; 0 takes the desktop's
([fullscreen under SDL](ENGINE.md#running-on-our-devices)). A size chosen is
saved, the window's or the full screen's, as the original's `SETRES` saves
it. Differs:

- **Screen Resolution** in a window offers DXEngine's sizes, not the
  display's modes: the common sizes from 640x480 that fit the screen, with
  the window's own and the screen's, a choice resizing the window.
- **Detail Textures** draws the world's detail textures as `D3DDrv`'s passes
  draw them, all three in the scene shader's one pass
  ([detail textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#detail-textures)):
  a band each nearer than 380, 90 and 21.3 units, each band's grain 4.223
  times finer than the last, faded to grey toward its bound and modulated in
  at twice its colour, each detail placed by its own pan, not panned with
  its surface (`DetailConsole`, `detail.py`). A surface takes the detail
  texture of its texture's current frame, but for a portal or a surface with
  a fog map, as `Render.dll` hands them over. The original draws none here
  either way, `D3DDrv` and `OpenGLDrv` alike: `D3DDrv` turns them off when
  Direct3D does not offer `BLENDDIFFUSEALPHA`, which Wine's Direct3D 7 does
  not, and set on in the running game, its pass changes nothing
  (`MeshConsole` with `NoDetail`); what that does to the pier's picture is
  under [brightness](#brightness). Against `D3DDrv`'s passes, the fade is
  worked out for each pixel, where theirs goes by a polygon's corners and
  passes over one with no corner within the band, and the bands are not
  rounded to 8 bits between passes.
- 16-bit **Texture Color Depth** is kept, DXEngine drawing in 32 bits.
  **Use 3D Hardware** finds no hardware, as the original finds none without
  an A3D or EAX card.
- **Rendering Device**, and a dedicated server from the Host screen, send
  `RELAUNCH`, which starts the launcher again with the rest of the line once
  the engine has exited, as the original's starts `DeusEx.exe`
  ([`RELAUNCH`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#relaunch)):
  the recreated launcher's renderer page or DXLauncher's Graphics page, or a
  dedicated server. DXEngine passes the line to the launcher on its line
  ([running it](ENGINE.md#running-it)), which starts again once the game has
  ended, where the original's new process starts while the old one is still
  ending. With no launcher on the line -- the engine started by hand -- the
  dedicated server's starts the engine again itself, as that server, and
  Rendering Device does nothing.

## Sound

Matches `Galaxy.dll`
([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)):
Deus Ex's sounds are mixed as its mixer mixes them (pan, volumes, resampling
and reverb, at `OutputRate`, which keeps its default when the ini's value
is no number ending in `Hz`) into one OpenAL source, one behind the level's
BSP muffled -- a closed door's too, the movers in it where it filed them
([the movers in a line's way](#implemented-not-as-the-original)); the
wall test is the solid-space walk the original's `FastLineCheck` makes
(`FastWalk`, the hearing test's): a solid brush muffles through its
invisible faces too, open space on both sides of a polygon does not
(`SoundConsole`'s wall, recorded on both engines). Who hears
a sound an actor plays is the original's too
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)):
the player within the radius ÷ √1.3, and with the level's BSP between, at
0.35 of the volume within 0.77 of that, but for the actor's instigator.
`SoundConsole` (its hearing test) and `SkipConsole` record both engines. A
window's sound is the original's too
([window sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#window-sounds)):
in the world, one unit from the player, turned by its place on the screen,
at the window's own volume, which is 0, unheard, unless the window sets it.
`SoundConsole`'s root beeps from the screen's left edge, centre and right
edge come out within 0.2 dB of the original's, panned alike, the one at the
root's own volume unheard in both. A map's load stops the sounds and leaves
the music playing, as Galaxy does when the load hands it the viewport it has
([playing a sound](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#playing-a-sound)):
the old song fades out and the new level's starts (its player's
`MTRAN_Fade`), a level of the same song jumping to its section
(`LoadingConsole` with `DXCAP_RECORD=1 DXCAP_MUSIC=1`: no silence across a
load in either engine, the fade after it, the music on after a save of the
map it is on is loaded). Differs:

- The music plays on an OpenAL source of its own.
- Not carried: Galaxy's quirk with the Sound and Speech sliders equal, both
  kinds of sound scaled by the slider once more.
- Not carried: the original's 1/256 volume floor, an integer artifact.
- A change to `EffectsChannels` (the Sound menu's Effects Channels) applies
  at once, held to 0-32, as the original's. The sounds on channels past a
  lowered count stop, where the original's play on out of its reach, and a
  raised count's channels are voices of their own, where the original's are
  its music's
  ([settings](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#settings)).
- Carried: Galaxy's holds on `OutputRate` (8,000 to 48,000 Hz) and `Latency`
  (10 to 250 ms). Not carried: the holds a `SET` puts on `DopplerSpeed` and
  `AmbientFactor`, which no menu sets.
- An ambient sound on a light follows its blink and strobe on DXEngine's
  timing ([lighting](#lighting)).
- A sound's ID packs a number DXEngine gives each object that plays one,
  never given out twice, where the original packs the object's index
  ([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds));
  the slot and `bNoOverride` are packed alike.

## Implemented, not as the original

Every other native the scripted runs compare matches the original
([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives),
[`Engine.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md)):

- **The traces** (`TraceConsole`, `VisibleConsole`; `TraceActors`, the
  player's frob target, through decorations), and `RadiusActors`, which
  reaches each actor's own collision radius further (`VisibleConsole` on
  Liberty Island and DXMP_Silo). The actor iterators (`RadiusActors`,
  `VisibleActors`, `ZoneActors`, `ChildActors`, `BasedActors`) walk the
  level's list at each step, so an actor spawned in reach during the loop's
  body is reached and one destroyed passed over, in the level's order, and
  `ChildActors` gives every actor whose owner chain reaches the caller, the
  caller among them (`WalkConsole`). A line or a box started inside an actor's
  cylinder, heading in, stops at its start, its normal level, out from the
  axis
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces);
  `TraceConsole`'s boxes started near a decoration's top and at its middle).
- **A line** meets the level as the original's walk down its BSP does
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
  where it enters solid space, a start in solid stopping it there, at time
  0, and a polygon with empty space on both sides stopping nothing; the
  hit's normal is the plane of the node the walk crossed, turned to face the
  start, and its surface that node's. `FastTrace` is blocked by any solid
  leaf its line touches, its start's included. A mover's brush is walked the
  same way, its hit's normal the node's plane as the mover's rotation turns
  it, by the original's sine table, not turned to face the line. The hits
  come out as the original's multi-hit check hands them on: the level's one,
  nearest first, the actors along the line cut to 5 units past it; with the
  level met within a hundredth of the line and 30 units, every actor's hit
  but a mover's dropped; a mover's hit with the level hit's normal to the
  bit, within 2 units of it, set 2 units before the level's, so a door flush
  with its wall is met before the wall (`VisibleConsole` on DXMP_Silo: 55
  movers' lines each way, their hits, normals, `TraceTexture`'s and
  `TraceActors`' alike; `TraceConsole`, `MoverConsole`).
- **A thrown thing's flight** (`ParabolicTrace`) moves as the original's
  `MoveActor` moves
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
  each step's line checked 2 units past its end, the move ended 2 units
  short of what it meets, so the thing lands 2 units and the trace's tenth
  short of the floor and bounces on from there; its failures' times the
  original's. `ParabolicConsole`: 30 throws from the island, bouncing and
  not, their landings and times alike, one coordinate a thousandth apart.
- **A mover's move** is `ULevel::MoveActor`'s
  ([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)):
  what stands on it carried first, then its encroachment checked at its new
  place and rotation (a mover mirrored on an axis tests the actor's box by
  its scale's size, as the traces take the scale), what it meets pushed by
  its move first, and moved and turned at once. A hinged door crushes a
  crate in its swing (`DoorConsole`
  on UNATCO HQ: 0.2 s into the close in DXEngine, 0.3 s in the original's,
  the crate coming to rest 10 units from where the original's does).
- **The movers in a line's way**: a line meets each mover where the level's
  BSP last filed it
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)),
  whatever it collides with, hidden or not. A map's load and a save file
  every mover where it is; each frame drawn files again where it is every
  mover not hidden that has moved or turned since; a destroyed one stays
  filed until the next load or save. A dedicated server, which has no
  client, files none at its load, as the original's makes no tracker
  without one: `FastTrace` and the checks on its line pass its movers, a
  `Trace` meeting a colliding one only as an actor (`MoverServer` on
  DXMP_Silo, through `dxcap.sh dedicated`: a closed door's line clear to
  `FastTrace` in both engines, `Trace` meeting the door; a door moved off it
  clear at both its places). `FastTrace` and the checks on
  its line (`LineOfSightTo`, hearing, the reach tests and path searches, the
  visible-actor iterators, network relevance, mesh lights' shadows, the AI's
  sight and the mixer's walls, [sound](#sound)) meet the filed polygons, a
  line's `Trace` and the multi-hit check meet them as the level's -- a
  polygon in the plane of a node of the level's own as that node, the
  texture and flags given those of the first polygon filed in the plane,
  the flags of the surface the original files it under: the polygon's
  without the editor's own (`PF_NoAddToBSP`), with `PF_SpecialLit` for a
  `bSpecialLit` mover, as it draws -- and a box meets none (`MoverConsole`
  on Liberty Island and DXMP_Silo: a door made not to collide, then hidden
  and moved, then saved; a window broken, at once and a frame on;
  `VisibleConsole` there; `GlassConsole` on WanChai's club glass: 33 in
  both, its polygon's own flags carrying the editor's processed bit too).
  Not carried: the refiling of the movers filed under one filed again,
  which the original files again wherever they are; and the polygons are
  filed whole, where the original's tree drops what lands in solid, which
  no line in the open meets.
- **Auto-aim's targets** (`AimConsole`): `PickTarget` takes a live
  projectile-target pawn ahead, nearer than 2000 units, whose aim is
  strictly the best so far and in sight, `PickAnyTarget` such an actor that
  is no pawn, and neither any without a `Game` (a client's level).
- **Sight** (`SightConsole`), and the pawns' in the tick (`SeeConsole`:
  `SeePlayer` sent only a player, each 0.2 s,
  [the senses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-senses)),
  **a pawn's pain and speech timers**, each due once under a thousandth of a
  second (`PainConsole`, both at half speed: the tick each fires in alike),
  **the reach tests** (`ReachConsole`), and
  **the look up and down stairs** (`StairConsole`: every answer of
  `Pawn.FindStairRotation` alike on Liberty Island's pier and
  `SlideConsole`'s slope,
  [small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
- **Falls**, a whole frame at once in the original's steps, each by its mean
  velocity (a pawn's air control, a bounce left to the script, a wall slid
  along, a landing ending the frame's move), coming to rest where the
  original's do at 60 frames a second and at 20, and their landing on a
  ledge -- a crate nudged off it, a pawn fitted clear of it and pushed on at
  random (`RestConsole`,
  [moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)).
- **Rolling**, in the original's steps, the whole frame at once
  (`RollConsole`: crates rolled across Liberty Island's pier slow, turn off
  its walls and creep down its slope as the original's do, within 0.15
  units, at 60 frames a second and at 20).
- **A pawn landing on another** or on a decoration that cannot be its base,
  which the scripts' `SupportActor` bounces off 50 up and 25 to 50 sideways
  at random, again until it slips off, and the one under it stomped but for
  a human NPC not hostile to the player (`StompConsole`: the player and Paul
  Denton on each other, crates).
- **A projectile's frame** is the original's `physProjectile`: one held-off
  move by the velocity x the whole frame, `HitWall` from what stops it, and
  with `bBounce` the step's time not moved given back, twice at most, the
  next move again by the whole frame (`ShotConsole`: a shot bouncing off
  the floor within one 0.4 s tick ends it 129 units along and 63 up in both
  engines).
- **Every physics runs once a frame**, as the original's `performPhysics`
  gives it the whole tick to step as it will: a pawn's walk, fall, swim,
  flight, path or climb; another actor's fall, projectile flight, roll,
  path, brush move or trailing; nothing in any other physics (a walking
  decoration stands still, as the original's). Then the turn, once a frame.
- **A path** (`PathConsole`): an actor following a ring of four
  `InterpolationPoint`s in 0.4 s ticks, the spline through the four, each
  point's `InterpolateEnd` and the time past a point carried on to the
  next, is where the original's is, turned as it turns, and stops at the
  point that ends the path.
- **An actor's turn** by its `RotationRate` (`SpinConsole`): a fixed
  direction spins on; to a desired rotation it stops there; the shortest
  way goes back through 0 when that is shorter; a rate of a whole turn a
  second does nothing (its low 16 bits are 0); `EndedRotation` at the
  desired rotation -- in both engines alike.
- **A trailer** (`TrailConsole`): a sprite rides behind its owner's facing by
  its `Mass`, or at the owner's `PrePivot`; another actor sits on the owner,
  turned with it or against its velocity, straight up when it has none.
- **Water**: falls in it (`FloatConsole`: a buoyant crate rises and bobs at
  the surface at the original's speeds) and swimming (`SwimConsole`: the
  player and a troop falling or thrown into water are put back at the water
  line and swim on, the player stopped as its head goes under and held to
  0.3 of `WaterSpeed`, as it walks whenever it swims; up out of the water it
  hops and falls back in, bobbing). A tick's chain of falls and swims shares
  the fall's eight steps, each hand-over carrying the count on, as the
  original's `startSwimming` and `physSwimming` carry it. A fall's step
  whose Z velocity turns over is split at the turn, as the original splits
  it, so a hit within the step meets the part's own mean (`HopConsole`: an
  actor rising at 30 units a second from 1 unit over the floor, through a
  0.4 s tick, lands in the split's next step at -77.5 in both engines).
- **Walking**, a whole frame at once in the original's steps, a slope slid
  down where the zone's ground friction is low. Its ledges: a pawn that
  cannot jump stops there, its move given up; any other is given `Falling`
  (`WadeConsole`: a troop walking off the pier given the jump
  `ScriptedPawn`'s `FallingState` aims at its destination, `EAdjustJump`,
  into the water or across it, with `bCanSwim` off, and stopped at the edge
  with `bCanJump` off; one walking down 14_OceanLab_Lab's steps into the
  sea, the stock maps' one shore). The speed a pawn walks and swims at
  (`calcVelocity`, once a frame; one that stops brakes to a standstill);
  ladders (`LadderConsole`: a player at a ladder's foot hangs there, climbs
  as it pushes into it, goes down looking down and steps off onto the top);
  and the same check's slide of a walking player on a texture with a
  `Friction` under 1 (`SlideConsole`: on a slope at a `Friction` of 0.5 and
  0, the glide to a stop and the slide down a slope in a zone of ground
  friction 0.2, at 60 and 20 frames a second; the game's one such texture,
  `Slippery_Metal`, covers two slopes in 14_Oceanlab_Silo).
- **Flying**, by which a player climbs (`FlightConsole`: a gull flying into
  the pier's wall, level and rising, toward the player, taking off as it
  walks off the pier, and its `LongFall` 2.5 s into a drop into the water;
  the stock gull's own flight plausible), and the pawns' turning (the pitch
  at half a turn a second toward level when walking or falling, held within
  `RotationRate`'s).
- **Small ones**: `SetLocation`, its touches made in its encroachment check
  before what stands on the actor is unbased and before it moves, as the
  original's FarMoveActor makes them, so a Touch handler sees the actor
  where it was, and what it no longer overlaps there untouched, not what it
  leaves behind (`TeleportConsole`); a touch linking each side on its own,
  its Touch sent as it is linked and the other side linked unless that
  undid it, the last free of the four slots taken, and a full four freed of
  their touchers with no physics, or for a pawn of a non-pawn, or for a
  player of a non-player, as the original's BeginTouch links
  (`TouchConsole`); the rotators' conversions, `<<` and `>>`
  by the sine table, a rotator's `*=`, float `==` exact, a rotator's `==`
  and `!=` every bit of each angle, `GetEnum` from index 0, `Normal` zero
  under a length of 0.0001, `Chr(0)` the empty string, `IsInState` true for
  a state the state extends, whose labels and functions the extending state
  has too (`ExtendConsole`), and a float made an int past an int's range,
  wrapping, by `int`, `byte` and a rotator's scaling (`RotatorConsole`;
  differs: `Chr` keeps the low 8 bits, DXEngine's strings being of bytes,
  where the original's makes a 16-bit character); `Object.Mid` 127 at its
  edges, its end's clamp known from `MidConsole`'s ten probes of both;
  `Object.DynamicLoadObject` with a group in the name, its own, another or
  none, and a class that is not the object's own (`DynLoadConsole`); and
  `Object.GetPropertyText` and `SetPropertyText`, through which the server's
  query answerer reads what a remote asks for: a public property's text as
  `GET` writes it, a const one never set, a private one or a name the class
  has not read empty and set nothing (`GetConsole`); text that is no value
  of the property, by `SetPropertyText` or the console's `SET`, leaves an
  int, a byte or a bool as it was and makes a float 0, as the original's
  import does, an int read as far as it is a number and a name its first
  token -- quoted, or a run of letters, digits, `_` and `-`, or else one
  character (`JunkConsole`). `LevelInfo`'s clock is
  the original's
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
  `Actor.GetBoundingBox` with no test place is the box where the actor is
  now, a dedicated server's too, as the original takes its primitive's
  (`MoverServer`). `Actor.GetMeshTexture` answers as the original's: the
  skin of that number, else the mesh's texture of a number above 0, else
  `Skin`, else the mesh's texture of that number, None past the mesh's
  textures and for an actor with no mesh, never `DefaultTexture`
  (`MeshTexConsole`).
- **The natives' edges** (`OpsConsole`): an int divided by a float in place
  is 0 for a divisor of 0.0, and multiplied or divided in place past an
  int's range wraps, as `int` does; a shift takes its count's low 5 bits; a
  string `switch` matches its cases without regard to case and a float one
  by its bits (-0.0 is not 0.0); `new` with a flag outside the script's own
  (Transactional, Public, Transient and the three NotFor) logs `new: Flags
  %08X not allowed` and leaves it out; every actor iterator given None takes
  Actor, and walks the caller's own level; `OrthoRotation` is the
  original's; a destroyed pawn keeps its `nextPawn`, so a walk through it
  goes on; `PlayBlendAnim` with a slot outside 0..3 logs and does nothing;
  `StrLen` measures under `STY_None` as under any style, and `DrawText`,
  `TextSize` and `DrawTextClipped` with no font log and do nothing (`StrLen`
  with none, which asserts in the original, answers 0 by 0); `GetNextSkin`
  walks the `.int` files' Texture objects as the original does, none in the
  game's. Differs: an int divided by -1 at INT_MIN is INT_MIN on every port,
  where the original's divide faults (a Critical Error).
- **A property's text** (`TextConsole`; `SetPropertyText`,
  `GetPropertyText`, `SET`, `GET`, an ini's or a `.int` file's value, a
  travel's): as the original's import and export have it. A struct's text is
  `(X=1.000000,Y=2.500000,Z=-3.000000)`, its members by name and a static
  array's by `Name[i]`, its own before those of the struct it extends (a
  Plane's W, then X, Y and Z), a quoted string holding a comma or a
  parenthesis whole, a nested struct whole; only the members named are set.
  Differs: an element by its index, `N[1]=7`, is taken, where the original's
  import stops at one (`ImportText: Illegal array element`: its index reader
  starts on the bracket) though its own export writes them. A dynamic array
  is `(1,2,3)`. An object is `Type'Package.Group.Name'`, set from that, from
  `Package.Name` or from a bare name among the loaded objects of its class;
  a class is held to its metaclass (an Actor class from `Engine.Texture` is
  None). A bool takes `1`, `True`, `0` and `False` alone, an int the number
  its text starts with.
- **A `GotoState` from within `EndState` or `BeginState`**
  (`PreemptConsole`): preempts the one under way, as the original's
  `RF_InEndState` and `RF_StateChanged` have it: the actor stays where the
  inner one took it, the outer one's state and label forgotten, and
  `EndState` is not sent again from within `EndState`.
- **A None context** (`A.X` with `A` None; `NoneConsole`): the zero of
  `X`'s type, as the original's `execContext` zero-fills the result -- a
  `switch (A.Physics)` takes `case PHYS_None`, a struct copied through None
  is zeroed, and a native's optional argument given through None is its
  type's zero, not left out (`Trace(,,, A.Location)` starts at the origin).
  A string, of which the bytecode has it zero-fill no byte, and an array's
  element, of which the original copies nothing, leave a variable assigned
  from them as it was; a Let through None stores nothing.
- **An actor spawned into water** keeps its physics, the spawn setting its
  region alone, as the original's does: an AirBubble, an Effects with
  PHYS_Projectile, rises as a swimming player's breath does, where Surreal
  Engine's rule gave anything but a Projectile PHYS_Swimming, which moves
  only a pawn (`BubbleConsole`: an AirBubble, an Effects and a Rat spawned
  under water, alike).

Differs:

- **A tie between a mover's own hit and the level's** at one plane falls to
  the mover, the actors first in DXEngine's order, where the original's two
  times, rounded apart by a half thousandth of a unit, fall either way: of
  `VisibleConsole`'s 110 lines on DXMP_Silo, `DeusExMover19`'s and `25`'s
  are the level in the original (a door turned a quarter turn in the plane
  of the wall round its doorway, its normal a bit off the wall's through the
  sine table, so not set before it) and `DeusExMover8`'s reversed line the
  level too; DXEngine answers the mover for all three.
- **The filed polygons** are an overlay on the level's BSP, not nodes in
  it: a start inside a filed mover's own solid is not in solid to DXEngine,
  and `TraceTexture` on `DeusExMover26`'s line gives the flags of the
  polygon DXEngine crossed, `PF_Invisible | PF_Semisolid`, where the
  original's filed node has 0.
- **`DeusExPlayer.GetDeusExVersion`**: DXEngine's own string, by choice, so
  a player can tell which engine they run
  ([the version](ENGINE.md#running-on-our-devices)). The original's is "Mon
  Mar 19 12:06:14 2001 v1.112fm".
- **The engine's own rotations** (the view, movement) take exact angles; the
  scripts' conversions take the original's sine table
  ([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)).
  At 20 frames a second `FlightConsole`'s gull, flown level into the pier's
  wall, turns and slides some 18 units along it in the original, where
  DXEngine's stays put.
- **Swimming out from under the water** (`SwimConsole`'s d: the player 100
  under the water line off the pier, looking 45 degrees down, the forward
  axis held): at 60 and 120 frames a second the two engines' players swim
  out alike, 4 units out at 0.2 s and 38 at 0.75 s. At some 1,000, either
  engine uncapped, a frame's move outward is under the 0.001 units between
  two floats of the player's place there (y 9,920), and each departs its own
  way: the original's gains one such step a frame and is six times as far
  out by 0.75 s; DXEngine's, whose velocity is the move it made, keeps to a
  step a frame until its sinking adds to the move, 3 units out at 0.2 s.

## Housekeeping, not seen directly

**The level's tick** is the original's
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
at every load the actor list is put in the original's order, static actors
first, and every pass of the tick starts after them, so a static actor never
ticks -- on Liberty Island 1,833 of some 2,600, none of which animates or
keeps a timer (`StaticConsole`). Each dynamic actor's distance to the player
comes first, in single player; an actor whose owner has not ticked that
frame waits for it; one spawned during the pass is ticked in it; a class
whose defaults are static or not to be deleted is not spawned; and stasis is
for single player only. An actor's own tick runs its state code, its timer,
its `LifeSpan` and then its physics; the timer calls `Timer` once a tick
however many of its periods passed, and a life span run out ends the tick
(a 0.05 s timer over `FrameConsole`'s held frame: one call in each engine).
Before each level tick the level's `bDropDetail` and `bAggressiveLOD` are
cleared, as the original's game engine clears them with a client: nothing
drops detail for the frame rate.

**Garbage** is collected as the original's map load collects it
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-map-loads-collection);
DXEngine's: [objects and memory](ENGINE.md#objects-and-memory)), the new
level's actors that are not in its `Actors` (an editor brush, a camera) with
it. `GarbageConsole` and `DeleteConsole` run both engines. Differs:

- **When**: at the end of the frame of the load, once the new level has
  begun and the player is in, where the original's runs before the new
  level begins.
- **A level left behind** is eliminated: every reference to one of its
  objects is made None, where the original's keeps the objects a live
  reference reaches. Without it a conversation event's `speaker`, the game
  replication info's `PRIArray` or a HUD's `clientObject` keeps a whole
  level. No stock script reads such a reference before setting it again.
- **Code is never freed**, where the original's frees a script class nothing
  uses and loads it again when asked for.

Measured on linux-x86_64, under software GL (llvmpipe) unless the row says
the GPU, and on the Smart Pro:

| Run | At engine | Result |
|---|---|---|
| Liberty Island and Battery Park loaded in turn (`reload-fresh.txt`), twenty loads | `d491a5c`, the GPU | 379 MB resident at the end, 393 MB at most; each island load's collection leaves the same 43,054 objects and frees some 130 textures and 20 sounds |
| Liberty Island and UNATCO HQ through `Current` (`reload-current.txt`) | `2bdaa7d` | 522 MB at most throughout |
| Liberty Island and UNATCO HQ through `Current` (`reload-current.txt`), twenty travels | `d88898f`, the GPU | 380 MB resident at the end, 386 MB at most |
| A collection | `d491a5c`, the GPU | a map load's 14 to 24 ms, the destroyed actors' backlog's 7 to 8 ms mid-game, the console's `obj garbage` 6 ms |
| A collection on the Smart Pro, Overclock: `ChurnConsole`, and `collections.txt` | `cd07879` | a map load's 147 to 230 ms; the destroyed actors' backlog's, 2,100 actors, 85 to 92 ms; the console's `obj garbage` 73 to 78 ms on the island, 102 to 103 ms in Battery Park |
| `ChurnConsole`, 2,200 actors a second for two minutes | `d491a5c`, the GPU | 360 MB resident throughout; a collection 7 to 8 ms |
| A save loaded in the running game (`CensusConsole`): six loads on Liberty Island, three from the menu, three runs started on it | `35243de`, the GPU | 351 to 361 MB resident throughout, as at the level's own start; 38 actors spawned a second at most |

**Names**
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#names-hashed-and-compared)):
a spawned actor, an AI event's object, an object a script makes with `new`
or travel makes anew, and a new window are named as the original names them
(`MakeUniqueObjectName`: the class's name without trailing digits and the
class's count, past any number an object of its package already has, a
spawned one's or an export's: `NameConsole`, Lamp1, Lamp2 and Lamp3 spawned
on Liberty Island named Lamp2, Lamp5 and Lamp6 in both), and
the collection deletes such a name once nothing holds it (an object's own
name, its properties', a class's defaults, a state frame's locals), its slot
taken by the next name made. Differs: every other name -- from a package, an
ini, the script or C++, or a spawned actor's looked up by its spelling -- is
kept for the session, where the original's deletes any name nothing reaches
but the hard-coded ones (`ChurnConsole`'s memory: the table above).

**Destroyed actors** are the original's
([destroyed actors](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#destroyed-actors)):
step by step as the original's, `bDeleteMe` set only at the end, so an actor
takes its own `EndState`, `BaseChange`, `Detach` and `LostChild` as it goes
and its `Destroyed` still finds it in `AllActors` (`DestroyConsole`); each
joins the level's chain (`FirstDeleted`, through `Deleted`), and at the
end of a level's tick, once 128 wait, or before a save, every live actor's
references to them are made None and the event manager is told. The original
then deletes them; DXEngine frees them at the next collection, which makes
every other reference to them None (a window's, a conversation's), where the
original's delete leaves those pointing at freed memory, and DXEngine lets
them gather between collections ([when](ENGINE.md#objects-and-memory)): a
collection is a hitch on the Smart Pro (above). A map load does not clean
the chain of the level it leaves, as the original's does only for `?push`:
that level is freed whole. A decal attached again after its `Destroyed`
(which detaches it) stays on its surfaces until that cleanup, as the
original's; there DXEngine takes it off them, where the original's surfaces
keep pointing at the deleted decal.

**`Object.CriticalDelete` 751** (20 call sites). The original frees the
object at once, whatever still refers to it
([`CriticalDelete`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#criticaldelete)).
DXEngine's flags it (`EliminateObject`): it goes at the next collection,
which makes every reference to it None, and until then it stays as it was,
which `NanoKeyRing.RemoveAllKeys` relies on as it reads the next key from
the one it deleted. Gone to the script at once as the original's:
`AllObjects`, the console's `SET` and a find by name pass it over, and a
save writes a reference to it as None. An actor is destroyed instead and
goes with the destroyed actors; one whose `Destroy` refuses (`bStatic`,
`bNoDelete`) stays, the log saying so, where the original frees it with the
level still pointing at it. Code, a package and the engine's own objects --
the levels, their infos and games, the save info and the root window -- are
not deleted (the log says so).

## Small

- **A long frame** is the original's
  ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
  the level's clock takes all of it, the actors and the player's input at
  most 0.4 s, so a load or a hitch moves nothing further in one step
  (`FrameConsole`: a frame held 2 s, the level's time 2 s on and an actor's
  next tick 0.4 s in both). The first frame on a level travelled to takes in
  its load: some 1.5 s on Liberty Island in the original's runs, of which
  its actors get 0.4 (`AIConsole`), and less in DXEngine's, whose load is
  shorter. The player then stands where the original's does, 4.75 over the
  floor (`StandConsole`; a line straight down meets the floor half a unit
  higher). Not carried: the original's floor of 5 ms, the step of every tick
  past 200 frames a second, which would run the game fast there. The level's
  `TimeDilation` is held to 0.0025 to 25, where the original's is not held
  (the game sets it only through `SetGameSpeed`, to 0.1 or more).
- **A travel's items**: the travel is taken whether or not the game's Login
  spawned a new pawn, as the original's SpawnPlayActor takes it; its info is
  looked up by the URL's name, then by its first 20 characters (the stock
  Login's cut), and standalone with none found the first entry is taken; a
  travelling actor is spawned at the player's place whatever stands there,
  as the original's SpawnPlayActor spawns them.
- **The first frame** is a microsecond at the shortest, never 0 s
  (`ScoutConsole`): a physics that takes the move's velocity from its tick
  -- flying, a projectile, rolling, a mover, a path -- divided 0 by 0 for an
  actor at rest on a frame of nothing, a generator's scout then lying
  nowhere; the original's first frame is its loop's own microseconds.
- **The script's `Move`** is a move as every other, the actor stopping 2
  units short of what blocks it, and answers whether the actor moved at
  all, not only the whole way (`NudgeConsole`: a crate 50 units over the
  floor moved 100 down answers true in both engines, resting at one height;
  a move from half a unit over the floor answers false, the crate unmoved).
  A move the level stops sends `BumpWall` with where the trace met it, 2
  units past where the actor stops, and its normal (`BumpConsole`).
- **The script's `MoveSmooth`**, a mover's push of what blocks it and a
  client's smoothing are the original's `moveSmooth`: a held-off move, a
  slide along what it meets by the rest, `HitWall` at a second wall, and
  true when the first move went at all (`SmoothConsole`: an actor 50 units
  over the floor moved 100 along and 100 down slides on along the floor,
  resting at one height in both engines; a move straight down from its rest
  slides nowhere and still gets `HitWall`, the first move's hit kept).
- **Input axes**: every `input` float is 0 again after the player's
  `PlayerTick`, and after its `PlayerInput` while paused, as the original's
  ([a player's input](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)).
  Differs: an axis a console command sets is used as it is; the original's
  input scales it (the harness's `StandConsole` and `JoinConsole` allow for
  it).
- **A key's binding**, as the original's
  ([a key's binding](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)):
  on a press, each command goes to `Button` and `Axis`, the exec functions
  of the player and the console, then the engine's commands, so
  `DefUser.ini`'s `GreyStar=Shot` takes a shot (a timeline's press; the
  original's input takes no scripted key on this desktop, and its dispatch
  is read). An `Axis` binding's `speed=` word is taken in any case, and 1
  without one, as the original's parse has it (`DefUser.ini`'s
  `JoyU=Axis aturn speed=5.9`; `PadConsole` with `pad.txt`). A `Button`
  naming a property the pawn lacks, or an `Axis` naming no float of it, is
  the original's "Bad Button command" or "Bad Axis command", logged and
  left alone (a User.ini's `F=Button bFirre` and `G=Axis aNope Speed=1`
  pressed by `tools/dxcap/timelines/bad-keys.txt`); a `Button`'s `input byte` is
  written as a byte, 1 pressed and 0 released. Differs: the
  original takes its viewport's own words (`SHOT` among them) before the
  exec functions, the same but for a word an exec function shares; `Toggle`
  and `Pulse` do nothing, and a command after `OnRelease` never runs.
- **A key's name** from the SDL2 and SDL3 windows, the launchers', is the
  original's
  ([the keys](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-keys)):
  a shift, control or alt its name for both sides, then its side's own
  (`LShift`; `UnknownA4` for the left alt), Caps Lock, the punctuation, the
  keypad's operators and Enter, Print Screen (`PrintScrn`), the Windows and
  menu keys (`Unknown5B` to `Unknown5D`):
  `tools/dxcap/timelines/keys.txt` with `DXCAP_BACKEND=SDL3` or `SDL2`
  presses each through the window's own events and logs the names the game
  gets (the original's input takes no scripted key on this desktop, and its
  code is read). Differs: a key is named by its place on a US keyboard,
  where the original's name follows the layout; the keypad's digits are
  always `NumPad0` to `NumPad9`, where with Num Lock off the original's are
  the arrows, Home and the rest. The other windows name fewer, and a side's
  own never: the X11 one, the harness's, has no Alt but the Meta key, no
  `` ` ``, no keypad Enter, and Print Screen as `Print`; the Wayland one,
  the engine's own on a Wayland desktop, no `/`, no `[` or `]` but as their
  shifted braces, no keypad operator but `-` (as `Minus`), no keypad period,
  Print Screen, Pause, Home, End, Insert, Page Up or Page Down.
- **The field of view** is the original's setting: the player's `DefaultFOV`
  is Deus Ex's own config, 75 (`[Engine.PlayerPawn]` in `User.ini`), the
  angle across a 4:3 view ([the view's width](#the-views-width)). Surreal
  Engine's own key, `MainFOV`, which the game's ini lacks (it would make the
  view 90), is neither read nor written.
- **A singular function** does not run while any singular function of its
  object does, as the original's
  ([variables and calls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#variables-and-calls)):
  the game has 32, a trigger's `Touch`, a pawn's or a decoration's
  `BaseChange` and `SupportActor` among them (`CallConsole`: one calling
  itself, its object's others directly and through a function that is not
  singular, and its object touched while it runs, all held off; another
  object's run). Differs: a call held off gives its type's zero, where the
  original's leaves a variable assigned from it as it was; none of the
  game's returns a value.
- **The windows' icon** is the original's, `DeusEx.exe`'s icon group 128
  ([the frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#the-frame)):
  the original's window under Proton and
  [DXEngine's](ENGINE.md#running-on-our-devices) game and error windows,
  through X11 and SDL3, give the window manager its 32x32 of 256 colours
  (their `_NET_WM_ICON`).

## The command line

The original's command line reaches DXEngine as one string, `--cmdline=`,
which main's launcher and DXLauncher pass on as given
([running it](ENGINE.md#running-it)).
Its flags are found as the original finds them
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)),
with one exception, by choice: a flag's name (`-X` or `/X`) must end at a
space, a `?` (a URL's next option, where the Host screen's
[`RELAUNCH`](#the-menus-settings) puts `-server`) or the line's end, where
the original's `ParseParam` checks nothing after it. Both take `/` as a
switch, and a Linux path is full of them: the original's would find
`-server` in `INI=/srv/server/x.ini`. The launchers parse alike
([where it differs](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#where-it-differs-from-the-original)).
What DXEngine does with each:

- **The start URL**: with `-hax0r` or `-server`, the first word (the second
  after `SERVER`) unless it starts with `-`; else the game's own start,
  `DX.dx`
  ([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)).
  `?loadgame=N` there, or as `--url`, starts a client on slot N's save,
  loaded before any map but Entry, as the original's first browse loads it
  (`StartConsole`, the original's with `DXCAP_START`); a slot with none
  starts at `DX.dx`.
  Differs by choice: the player is the one saved, as the game's own load
  gives it (`LoadConsole`), where the original's start logs the player in
  with the travel info the level was saved with -- on a map the player
  travelled to, the inventory, skills, themes and key ring it arrived
  with, made anew, the saved ones left in the level, held by no one.
- **`-server`**: a dedicated server, as `--server`.
- **`INI=` and `USERINI=`**: the inis read and written back, as `--ini` and
  `--userini`, each found by its own name, never `INI=` within `USERINI=`
  (`DXCAP_USERINI_FIRST`: the two given `USERINI=` first, `MidConsole`'s
  run alike in both engines). One not there yet is empty, as the original's
  config cache takes it, and written whole at its first save; a missing
  `Default.ini` or `DefUser.ini` the same.
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in;
  `exec` runs a file's lines as commands, as the original's does.
- **Safe mode's flags**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music. The original makes no audio
    subsystem; DXEngine keeps its device, on OpenAL Soft's null driver.
  - `-defaultres`: the window and fullscreen sizes 640x480 for the run, not
    saved.
  - `-nohard`: the software device for the run, as the original's software
    renderer ([the software device](#the-software-device)).
  - `-noddraw`: with the software device, chosen or drawing where a GPU
    device does not start, a window for the run, as the original's software
    renderer has no fullscreen mode without DirectDraw: so
    `-nohard -noddraw`, "Run the game in a window".
  - `-nojoy`: no pad at all, neither DXEngine's pad handling nor its buttons
    as keys.
  - **Nothing to do:** `-no3dsound` (the original's turns off A3D or EAX
    hardware; DXEngine's mixer is software, [sound](#sound)), `-nommx`,
    `-nokni` and `-nok6` (the original's mixer picks its routines by them),
    and `-safe`'s no DirectInput (DXEngine's input is its window's, SDL's
    under the launchers).

## Mods

A mod gets the original's loader and natives:

- **Its textures**: Deus Ex's formats (`TEXF_P8` to `TEXF_RGBA8`), drawn
  from the compressed mips when they are DXT1, as the original's
  `UTexture::Lock` picks them; a format past those, which no Deus Ex
  package has (a UT 469 one), is drawn white, with a line in the log.
- **Its folders.** Its packages and maps are found by the `Paths` of its own
  ini (`--ini`), relative to `System` and written with backslashes as the
  game's are, their folders and extensions in any case, as the original's
  on Windows
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
- **Its text.** Differs by choice: the `.int` files are read from each
  folder of the paths, in their order, and from `System` after them when
  they do not name it, where the original reads them from `System` alone
  ([the .int files](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-int-files)).
  So a mod's folders, named before the game's in its ini, give its text and
  the objects its files register with no file of it in `System`. The first
  folder to give a key, or to register an object, gives it: a mod's own
  `DeusEx.int` changes only the keys it has. A key no file gives is the
  original's: the script's `Localize` logs `No localization` and gives
  `<?int?Package.Section.Key?>`, and a localized property keeps its default
  (`LocalizeConsole`: the game's text, its render devices and a missing key
  alike in both engines; two mod folders' text and objects in DXEngine's
  run alone).
- **Protected packages.** An export whose flags give it no context (client,
  server or editor) is never made, and a reference to it is None, as the
  original's linker has it: the live servers' anti-cheat packages (ANNA,
  DXNMS) point their classes' `ScriptText` at such exports. A save's exports
  are all made whatever their flags: DXEngine's own saves may lack them.
- **A mod's packages and saves.** a package's or a save's property tag of an
  element past its static array, of another type or of another struct is
  logged and skipped, as the original's loader skips it, the object loading
  on; a struct's static-array members are read every element, its native and
  transient members passed over on disk, as the original's binary has them;
  a script's Unicode string constant of more than one character loads on the
  Linux ports; an expression 1,024 deep loads (64 was the limit); a mesh
  whose faces run past its vertices or wedges loads, those faces passed
  over; a mesh DXEngine saves loads back; a palette short of 256 colours is
  padded with black; a texture whose mip data is shorter than its size is
  padded with black; a map whose surfaces or nodes point outside its arrays
  -- a node's links, zones, surface, render bound, vertices and their
  points, and its collision hull's list -- is refused with their numbers,
  the vertex pool's entries no node names left alone, every map of the game
  having thousands past its points (`MapWalkConsole`: every map of the game
  loads); a fire texture's sparks and drops outside it are masked into it; a
  scripted texture drawn from outside its source wraps, and one drawn with a
  size of 0 or a float that is no number draws nothing; an animation chain
  that never comes back ends at 2,048 textures; an import whose outers never
  end is refused; an export whose class, outer or base rings on itself is
  refused; a dynamic array property of a dimension over 1 reads its element
  where it is; a name's length that is no name is refused; a string's, an
  array's, a sound's or a song's size that is none is refused before
  anything is allocated; a map folder the paths name but the disk lacks is
  passed over with a line in the log; a path's map pattern is taken in any
  case; a path's folder in another case than the disk's gives its text and
  registered objects too; a class DXEngine saves has its super class for its
  base, so it loads back laid out from it.
- **A mod's actors drawn.** a mesh sequence of no frames draws its first
  frame, and an `AnimFrame` of 1 or more wraps into its sequence with the
  weight between frames taken before the wrap, as the original's GetFrame
  takes it; a blend's frames past the mesh's vertices are left out, as a LOD
  mesh's are; a LOD mesh's material naming a texture slot past the mesh's
  draws with the environment map, and a face naming a wedge past them, or a
  collapse list leading past them, is passed over, as a vertex past the
  vertices is; a mover's polygon without a texture draws with the level's
  default texture, its macro texture too; a mover whose brush has no
  polygons is passed over; a warp zone with no other side opens no frame,
  its `Generate` called first, as the original's OccludeBsp has it; a font
  lacking a character and the space draws nothing for it, and a hot key's
  underline is drawn from its own page of the font, under its letter when
  centred; a viewport window of no size draws nothing, and a frame is a
  pixel at the least and 16,384 at the most; a `bHighDetail` actor is passed
  over while the render device's `HighDetailActors` is off, as the
  original's SetupDynamics passes it (of the game's classes only `Decal`
  sets the flag, and a decal is drawn with its surface); an actor at no
  place (a NaN location) sorts nearest among the translucent.
- **A mod's deletions and sizes.** a `CriticalDelete` of an actor destroys
  it, with the destroyed actors (its flag once cut their chain), one whose
  `Destroy` refuses staying, and of the levels, their infos and games, the
  save info or the root window nothing; a light in the level's light lists
  is kept, destroyed or not, so the lists keep their places; a destroyed
  mover's polygons keep their texture for a `TraceTexture` through them; a
  collision size or location past the cells an int names, or no number, is
  held to them.
- **A net game's edges.** a client's reference to a map actor destroyed and
  freed is refused, not loaded afresh in no level; a channel destroyed
  leaves nothing for the next bunch to merge into; an enum of more than 256
  names goes as a byte; a coordinate that is no number or past an int is
  held; a download past its size, or past 2 GiB, ends at once; a first actor
  bunch that opens nothing closes the connection, and a spawn that fails
  closes its channel (the original's are a fatal error and an assertion).
- **`ScriptedTexture.ReplaceTexture`** copies the texture's current frame
  over the scripted texture when their sizes match, else logs, as the
  original's (no run: no stock script calls it).
- **Config files.** A class's config file that is not there is empty, its
  properties keeping their defaults, until the class writes it
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has. A key
  with an empty value is a value, what the original's `ImportText` makes of
  no text: a string empty; a name, object or class None; a float 0; an int,
  byte or bool its default. Of a key given twice in a section the last is
  read and the last written, wherever the engine reads the ini: a class's
  config, the default URL, the settings it reads again by hand -- the
  player's as it spawns one, the client's, the audio and render devices'
  (`IniConsole`). Those read by hand, though, take an empty value for none:
  a float or a string keeps DXEngine's default where the original's is 0 or
  empty. An ini written back, at a clean exit, has a changed value changed
  in its own line, a key new to a section added after the section's last
  key, a section new to the file at its end, and the rest of the file as it
  was, a CR LF file's line endings with it; a file not there yet is written
  whole. The
  original rewrites each file whole, CR LF, comments gone (`ConfigConsole`:
  a class's own `DXCapConfig.ini`, CR LF or not there, read and written back
  alike). `StaticSaveConfig` from an object writes the class's defaults, as
  from the class (`ConfigConsole`). A config value of any type is read and
  written as the original's import and export have it (a property's text,
  above): a struct's members, a class or an object as `Type'Path'`, a
  dynamic array one line of its name per element, a bool's four words alone,
  an int as far as it is a number (`ConfigConsole`: a vector, a class and
  two strings written and read back).
- **The natives no stock script calls** (`ModConsole`): `ResetConfig`, a
  System or User class's section copied from `Default.ini` or `DefUser.ini`
  and its config read again into the class, its subclasses and their
  objects
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)),
  which the original then tells of it (`PostEditChange`) and DXEngine does
  not; `DeusExPlayer.SetBoolFlagFromString`
  ([the player](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-player));
  `ExtString`'s text in parts
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small));
  and `clock`, `unclock` and `CyclesToSeconds`
  ([the timing natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#clock-unclock-and-cyclestoseconds)),
  which count the steady clock's nanoseconds where the original's count the
  CPU's cycles.
- **Spawn notifications.** A `SpawnNotify` on the level's list is asked
  about each actor spawned of its `ActorClass`, its answer standing for the
  actor from then on: the next notification is asked about it, and the
  spawn returns it, as the original's
  ([spawning](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#teleporting-an-actor);
  `CensusConsole`'s `CensusNotify`). Differs: a None answer ends the walk,
  where the original's next notification with a class faults on it.
- **Two path natives no stock script calls**: `Pawn.FindRandomDest` 525
  answers None, as the original's does (its `findRandomDest` returns nothing
  in any level with navigation points, and finds nothing without them);
  `FindBestInventoryPath` 540, the way to the inventory the pawn wants most,
  is Surreal Engine's, not the original's `findPathTowardBestInventory`,
  which is unread.

A macro texture, which no stock texture has, is drawn as `D3DDrv` draws it
([the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness)):
modulated at twice its colour, the light map after it twice as bright as on
one pass, and not panned with the surface, at its own scale
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)).
`LightConsole` with `Macro`, Liberty Island's `Spotlight0` on a world map
given itself as its macro texture, has the GL and Vulkan devices' frames at
1.00 of `D3DDrv`'s (`NoDetail`): in blocks of 16 pixels inside the map's
border, within 0.94 to 1.04 (without the macro, [lighting](#lighting)). The
D3D11 device keeps Surreal Engine's
([where it stands](../README.md#for-advanced-users)).

## Multiplayer

Matches the original's client and server
([the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md)):
joining and playing, listen and dedicated servers, replication with the
original's native lists, the server's calls, downloads both ways, server
travel, the Entry level for a lost server, the login, the player's net
speed, which sets how soon the server corrects a client (`JoinConsole`'s
jumps), merged bunches, the uplink and query answerer, the scripts' sockets
([the script's links](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-scripts-links)),
the addresses they ask for, a player's and the server's own, and a server's
URL with the address it bound (`ServeConsole`'s `serving` line;
[the scripts' addresses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#addresses)).
The sounds the server plays reach a remote player's client, which plays them
through `Pawn.ClientHearSound`
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds);
`ServeConsole`'s beeps, the client recorded). Each runs against the original
as client and as server: `ServeConsole`, `JoinConsole`, `RejoinConsole`,
`TravelServeConsole`, `TravelJoinConsole`, `NetConsole`, `PickServer` with
`PickJoinConsole`. DXEngine joins live
public servers, the mods they run included (MTL, ANNA, CDX, DXAG, SG), and
stays in. Until its pawn arrives, a joining DXEngine client draws no world.
The server-only stubs are under
[not needed for single player](#not-needed-for-single-player). Differs:

- **The net driver** is DXEngine's own, over UDP with
  `[IpDrv.TcpNetDriver]`'s timeouts; the connection, its channels and the
  handshake are the original's, and so is its socket: broadcast and address
  reuse on, 32 KB buffers for a client's and 128 KB for a server's, a
  server's port the URL's or the next free of the 20 from it (`PORT=` and
  `MULTIHOME=` are not taken); a server's host name is looked up on a
  thread, the game going on.
- **The download cache.** A package is cached as its GUID with `CacheExt`,
  where the original writes `.uxx` whatever `CacheExt` says (the game's ini
  names `.uxx`). A package a join loaded from the cache is let go by name at
  the next map load that does not use it, and that load's collection frees
  what nothing uses of it but its code, which stays loaded, where the
  original's collects it all
  ([housekeeping](#housekeeping-not-seen-directly)).
- **A root window per pawn**, made at its first Possess and kept in it, as
  the original's `InitRootWindow` makes it; the engine draws, ticks and
  feeds the viewport's own player's: a client joining a listen server
  gets a root of its own and leaves the host's in place (`ServeConsole`
  with `JoinConsole`: the server's log names each root's pawn).
- **A listen server runs at most 144 frames a second**, where the original's
  runs as fast as it draws: a remote client's queue drains its rate over a
  tick, so the faster the ticks, the less room an unreliable call finds
  ([packets](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)).
  `ServeConsole`'s 8 beeps reach the original's client 7 of 8 at 100 and at
  144 frames a second, 4 at 200 and none at some 1,300 (uncapped on the
  desktop with no vsync), as the original server's reach DXEngine's, 6 to 8:
  the first goes as the host stops, its last step's packet just sent.
- **A link's binary data**: `ReceivedBinary` gives the script the bytes that
  came, each element of its array. The original's gives it others, the
  count right: 20 bytes sent to a `UdpLink` in binary mode reach its script
  as 20 bytes of what else is there, whatever was sent, and its
  `SendBinary` answers False. No mod can have counted on them. DXEngine's
  `SendText` and `SendBinary` answer False when nothing went.
- **A dedicated server's natives**: `GetPlayerPawn` answers None, as the
  original's with no client or viewport, and `AttachDecal` attaches none, as
  the original's with no client.
- **A URL**: a '/' is a character of a map's name or an option's value, '#'
  alone marking the portal, as the original's URL has it (another game's '/'
  once cut `?Name=AC/DC` at the slash); a relative travel that names a
  server's address goes there; a URL cleared is empty whole, its port the
  game's default again.
- **Entry's actors** find their regions in Entry's own model, with its zone
  actors and its LevelInfo, while a map is current (no run: the code settles
  it).
- **The player's name in a net game**: a name the player never chose --
  none, or the game's stock `Player` -- logs in as `DXEnginePlayer`, a
  client's to its server and a listen server's host's, by choice, so
  DXEngine's players show as such (`Engine::SetMultiplayerName`). A name the
  player chose is kept; single player, the menus and the inis keep the
  game's.

As in the original:

- A client replicates its own player's pawn to the server each tick, before
  its packets go (`ULevel::TickNetClient`): the values the script classes'
  statements send from a client, none of the engine classes' native lists,
  which send only from the server. Deus Ex's `FrobTarget` goes so: the
  client finds what the player looks at, and the server's `ParseRightClick`
  takes it, so a weapon or an item is picked up on a net game only so
  (`PickServer` and `PickJoinConsole`, each engine as server and as client).
  Each side keeps what the other last got of each value.
- Until a server's first correction gives it its base, a client's player
  rests 2.46 units below the server's, which the correction lifts it by: a
  client runs no level's `SupportActor`, a simulated proxy running only
  simulated functions, so its floor is never its base, and the walk lifts a
  pawn only over its base -- DXEngine's client on the original's server as
  the original's on DXEngine's (`JoinConsole`).
- For a player's first seconds in, until the server's augmentation manager
  reaches its client, the client walks at 100 where the server runs it at
  230 (`GetCurrentGroundSpeed` gives 0 without one), and the server corrects
  the walk (`JoinConsole`'s jumps).
- A game hosted from the menus is never listed on a master server: an uplink
  announces a server only with its `DoUplink` set, which the game's
  `DeusEx.ini` does not set
  ([the master server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-master-server)).
- The Join Internet screen lists servers only once `MasterServerAddress`
  (`[DeusEx.MenuScreenJoinGame]` in `DeusEx.ini`) names a live master; the
  game's names GameSpy's, which is closed.

## Not needed for single player

Stubs a single-player game does without; the audit lists each:

- `DumpLocation`'s 21: Ion Storm's bug-location tool, though
  `DeusExGameInfo.Login` calls `HasLocationBeenSaved` on every map but a
  save's.
- `DebugInfo` (compiled out in the original too:
  [`DebugInfo`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#debuginfo)),
  `SaveTimeDemo` and `Commandlet.Main`.
- `ComputerWindow`'s 21, which no script calls: the InfoLink's text window,
  its only user, calls only implemented ones.
- `GC`'s `PushGC`, `PopGC`, `CopyGC` and `Intersect`, and 3 more of the
  windows' stubs, which no script calls.
- One more that no script of the game calls, so only a mod reaches it:
  `Canvas.DrawPortal` 480, which draws nothing; the original's is unread.

`StatLog`'s and `StatLogFile`'s natives do nothing too, though the audit
reads them implemented (they log no `Unimplemented`), so a DXEngine server
logging world stats writes no log of them.
