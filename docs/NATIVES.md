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
- **What differs** is the owner's choice, said "by choice" with its reason,
  or open, named by its ID in Port Ex Machina's
  [bugs list](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/bugs.md):
  a `D` one made on purpose, the owner's to rule on or ruled to change, any
  other to be made the original's.

## Stops the game

Differs by choice. An error DXEngine does not catch -- a script error, an
unknown native, a call to a function import that did not load, a failed save
-- exits the engine with 1, which the launcher takes for a crash. No map or
action is known to reach one; the audit lists the natives whose call would.
A `foreach` on a None context, and state code's `goto` to a label its state
has not, stop both engines alike (the original's Critical Error); a None
class's `default` is 0 in both, and `AllActors(None)` every actor. A dynamic
array's element, on which the original asserts (no Deus Ex script indexes
one), grows the array to the index in DXEngine, by choice, a negative index
a script error. A modal window made inside another window stops the original
as it is destroyed: its cleanup takes it out of the table of tab groups of
the modal it lies in, which never held it, and the table's index asserts (as
it was made, only "Modal window must be a child of Root!" was logged; no
Deus Ex script makes one). DXEngine destroys it, by choice. `StringToName`
of text with a character other than A to Z, a to z, 0 to 9, `_` or `-` stops
the original (Extension.dll `0x10037940`); DXEngine makes the name, by
choice. The game's 26 calls pass identifiers alone. The text parser's
`GetName` at a token other than a NOTE, G, F or L stops the original, a name
made of a null string failing FName's assertion (DeusExText.dll
`0x10001980`); DXEngine gives None, by choice. An `EMAIL` tag with no `=`
crashes it (`0x10002560`); DXEngine empties the fields, by choice.

A runaway script stops both engines alike, with an error no native's call
catches
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-script-interpreter)):
a loop past 10,000,000 jumps run, taken or not, in one level tick or one
map's load, "Runaway loop detected (over 10000000 iterations)"
(`LoopConsole`); a 251st nested script call, "Infinite script recursion
(250 calls) detected" (`CallConsole`). With `-norunaway` the loop runs on,
its count started again, as the original's (`LoopConsole` with
`DXCAP_FLAGS=-norunaway`: 20,000,001 jumps run through in both); a save's
load counts its own jumps, as the original's map load does (`LoopConsole`
with `LoadAfter`: 9,999,001 jumps, then slot 9's load in the same tick, in
both).

## Saving, loading and travel

Matches the original ([travel and
saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving),
[the save
directory](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-save-directory)):
each engine loads the other's saves (`SaveConsole`, `LoadConsole`), a string
written as the original writes it, 16-bit when a character in it is past
Latin-1 and else 8-bit, and read either way whole ([a string's
bytes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers):
a note of U+0100, U+03A9 and U+2122 saved by each engine and loaded by each,
its characters' codes alike), as a package's are; and a level returned to
keeps its game (`ReturnConsole`). A level's start ([the
original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick))
runs each pass over its actors as the list grows, so a map's carcass holds
its items hidden in `Idle2` and its search hands them over whole
(`CarcassConsole`), and after `BeginPlay` finds every actor's zone where it
is, whatever region the map saved: DXMP_Silo's items, crates and robots
start in their zones, where the map's regions put them in none and a first
fall would take them out of the world (`VisibleConsole` there); every level
loaded, a save's or one returned to, then calls `PostPostBeginPlay`
(`LoadConsole`, `ReturnConsole`). A state's `Sleep` keeps its time in the
actor's `LatentFloat`, as the original's, so a save keeps it and a script
reads it, and ends once under half a tick is left ([latent
functions](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#latent-functions)):
`SleepConsole` at 60 frames, a 0.105 s sleep over and over waking 31 times
in 3 s in both engines, and a 10 s one saved and loaded 3 s in sleeping out
its time after the load. A save loaded in the running game, from the menu
map or the level it was saved in, and a run started on it, spawn what the
original's do, class by class, second by second (`CensusConsole`,
`census.py`), and keep it, the game's decals going as [out of
sight](#out-of-sight) has them. A save's load logs the saved player in as
the original's does, with `?loadgame`: the game's `Login` keeps it as saved,
and its `TravelPostAccept` spawns the level's mission script, which no save
holds -- an object of a transient class is not written (`LoadConsole` over
either engine's saves and the original's hub save, `ReturnConsole`), nor one
flagged transient, as a script's `new` can make one, a reference to it None
(`SaveEdgeConsole`: a probe made by `new` with `RF_Transient` and held by a
saved actor loads as None in both engines, from either's save, one made
without it whole); one of another package is still written as an import
when it is also public, as the original's is. The
console's `open ?loadgame=N` loads the slot as the game's own `LoadGame`
does (`LoadConsole` with `Open`); a slot that is no number loads nothing in
either, DXEngine reading it as slot 0. The game's `SaveGame` takes any
number as the original's does: -1 the quick save, 0 a new slot, and any
other `Save%04d`, a negative one too -- `SaveGame(-3)` writes Save-003, and
`SaveGame(-2)` its SaveInfo to Save-002 with the level saved into Current
-- and the Load Game screen lists every `Save*` folder, both of
those among them, by name, as the original's `FindFiles` on NTFS gives them,
by upper case, `[`, `\`, `]`, `^`, `_` and `` ` `` after the letters
(`SlotConsole`). A new slot takes the number past the highest of the
`Save` folders' whole numbers, by choice, where the original's reads each
folder's number past its first digit (DeusEx.dll `0x10017cc0`), so that
`Save1000` counts as 0 and its 1001st save can overwrite a slot. A bare
`?loadgame`, a slot named by no number, is no load,
as the original's browse asks for `loadgame=`: the console's `open` of it
goes to the default map, `Index.dx`, which is not there, so the level's own
map is loaded again, as for any map not there (below): from the menu map the
menu again, its URL a load's, `Index.dx` (`URLConsole` with `Bare`: the same
lines in both engines). A `?load=` loads no save either: a URL with one is a
map's browse, as the original's, the option's value, whatever it is, the map
the browse tests for its mission and looks for in `Current`, in place of the
URL's, and the URL's map the one loaded from `Maps`. From UNATCO HQ
`open 01_NYC_UNATCOHQ?load=01_NYC_UNATCOIsland` saves HQ into `Current` and
brings the island back from there as it was left, HQ passed over;
`open 01_NYC_UNATCOHQ?load=9`, 9 in no mission, empties `Current` and loads
HQ from `Maps`; a value of the mission with no file in `Current` loads the
URL's map from `Maps`, `Current` kept; and a value naming the level left
finds it there, just saved. A load from `Maps` leaves the option on the last
URL, so the console's next `open` and a map exit carry it, until a travel
names another, the console's `start` drops it or a level comes from
`Current` or a save: after `?load=9` every such travel empties `Current`, so
a level left and come back to begins again (`URLEdgesConsole` with `Short`:
the same lines in both engines but the held `LevelInfo`'s, a crate left in
each level found again only where it came back from `Current`, and the
island opened after `?load=9` from `Maps` with the URL `Index.dx`).
Before a level is saved into `Current`, the
augmentations, skills and flag base that travel with the player go from it,
a decoration in its hands too (`PruneTravelActors`; `ReturnConsole`). The
player's goals, notes and key infos, which the game makes with `new(Self)`,
are in the level's package, as the original's `new` puts an object in its
outer ([the
natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)):
a save holds them and either engine's load gives them back, on the island
and after a map exit's travel (`SaveConsole` and `LoadConsole`, also with
`Via`). A map exit's travel makes each object a travel property holds anew
in the new level, held by the new player and named by its class's count, its
text whole (`ReturnConsole` with `Items`: a goal, a note and a key, their
texts with `?`, `;` and `%` in them); a console's `open` carries none of
them, in either engine. Every element of a static array travels, and each
field of a struct (`ReturnConsole` with `Items`: two canisters' `AddAugs`,
the inventory grid's `invSlots`, the augmentation slots' `AugLocs`, a vector
and a rotator flag). The travelled items take their `TravelPreAccept`, the
game's `AcceptInventory`, then their `TravelPostAccept`, last to first, with
the player's own pair round them, once each, as the original's
`SpawnPlayActor` runs them, so each item's `GiveTo` links the player's chain
in the original's order. A travel property's reference to an actor destroyed
in the level left does not travel, by choice, where the original's export
writes it and the actor is made anew: in the original a new game begun from
the game with a weapon drawn brings that weapon to the island beside the new
game's, by the player's `LastinHand`, which the intro's `Paralyzed` never
updates; the belt's slot holds the new one, the 1 key draws the old, and no
weapon can then be put away until the old one is dropped (`NewGameConsole`:
the original's 9 items, two pistols; DXEngine's 6, one, the belt's 1 then 2
drawing the pistol and then the prod). The export walks a class's own travel
properties before its super's, `Actor`'s `Inventory` chain last, and writes
an actor only with `bTravel` set, walking nothing past one it does not
write, as the original's: so the arrival puts the chain's items back in
their belt slots before an item that only a property of the player's names
takes the belt's first free slot, and an actor without `bTravel` stays
behind with what only it names (`CarryConsole`: binoculars and soy food
named by the player's `ClientinHandPending` and `SelectedItem` alone arrive
at UNATCO HQ in the belt's slots 5 and 4 in both engines; with the
binoculars' and the prod's `bTravel` cleared, they stay behind in both, the
pistol and the two ammo with them, the chain ending at the medkit: 3 items
arrive). A value is written only where it differs from its class's default,
as the original's export writes it, so one left at its default arrives as
the new object's spawn and events left it (`DefaultConsole`: an item's
travel property set back to its default before a map exit arrives as its new
spawn set it, 2, in both engines, and one set away from it as it was set). A
string is compared with its default without case, A to Z alone, as the
original's `UStrProperty::Identical` (`appStricmp`) compares it, in the
travel's export and a save alike: one apart from its default in case alone
neither travels nor saves, and arrives or loads as the default
(`CaseTravelConsole`: `TruePlayerName` set to `JC DENTON`, its default `JC
Denton`, arrives at UNATCO HQ as `JC Denton` in both engines). A map opened
at a portal, `open <map>#<portal>` or `open <map>/<portal>`, the original's
URL reading either, starts the player at the teleporter of that tag
(`PortalConsole`; `URLPartsConsole`:
`open 01_NYC_UNATCOIsland/tunnel_drop_nyc`, the player 7 units from it in
both engines). A level's URL, which
`Level.GetLocalURL` gives, is the one it was entered by, as the original's
`LoadMap` sets it: a level returned to from `Current` names the saved file,
`..\Save\Current\<map>.dxs` with the travel's options, as the original's
browse loads it; a load -- an option starting `load`, `?loadonly`,
`?loadgame` or `?load=` -- leaves the URL the level holds, a map file's
`Index.dx` (every stock map's), so a restarted level's is `Index.dx`, or a
save's, the URL its level was entered by, which a save keeps. A travel
builds on the last URL, the one the level was entered by -- a level's from
`Current` or a save its own once the player is in, as the original's
browse makes it --, less its `?loadonly` and its
portal, as the original's game engine takes them off: a map exit's takes its
server, map and options, the player's name and class among them, a console's
`open` its options; an absolute travel, the console's `start`, takes the
player's options it lacks -- `Name`, `Team`, `Class`, `Skin`, `Face`,
`Voice`, `OverrideClass` --, after its own, logging each (`URL: Adding
default option`), so a hosted game keeps the player's name (`ServeConsole`
with a name in the user ini: the host logs in by it in both engines).
Nothing else is added: the travel after a restart has no option, and none
has the game's `?Difficulty` (`URLConsole`: the same six URLs in both
engines, from the menu map through Liberty Island, UNATCO HQ, the island
again from `Current`, its restart and UNATCO HQ again, and the same logins).
`GetURLMap` gives the last URL's map, as the original's reads the game
engine's (Engine.dll `0x103b8df0`): once a level is in, after a load from
`Maps` the map its URL named, where the level's own URL is `Index.dx`
(`URLEdgesConsole`: `01_NYC_UNATCOHQ` after `open 01_NYC_UNATCOHQ?load=9`,
the level's URL 8 characters long).
The player's options a URL starts from are every key of the user ini's
`[DefaultPlayer]`, in its order and as the ini spells it, a key given twice
its last value, and a line with no name, `=x`, the option `=x`, as the
original's `LoadURLConfig` adds the section's lines (Engine.dll
`0x103f9bc0`); `GetDefaultURL` answers the first of them that begins with
its option and `=`, so `GetDefaultURL("")` is that line's value
(`ExecLineConsole` with a `User.ini` of `=x`: `?=x` in the menu map's URL
and `x`, in both engines).
The saved level is found whatever case the URL spells the map in, with or
without its `.dx`, as the original's file system finds it: the stock exits
spell `12_vandenberg_cmd` and `00_training` (`RevisitConsole`: the island
opened again as `01_nyc_unatcoisland` has the crate left there). A travel's
`PreTravel` goes to every actor of the level that has the function, with no
argument, as the original's `SetClientTravel` sends it: the mission scripts'
among them (`TravelConsole`). A console's `open` or `start` takes the rest
of its line, past the word and its blanks, as the URL, as the original's
`Exec` takes it, and one with nothing after it travels nowhere but sends
`PreTravel` all the same (`OpenLineConsole`: an `open` alone and one of
`DX.dx?Name=Two Words` each send it in both engines, the first staying in
the level, the second, a URL [refused](#multiplayer) for the blank in its
option, loading the menu map again as `DX.dx`, the player `Player`). A
client's travel exports the viewport's player alone, by its name, and only
with its items, a server's with its items the level's own travel info and
every pawn with a player, as the original's game engine's tick exports them
(read: Engine.dll `0x1038fe90`).

A travel to a map that is not there fails after the pre-travel, as the
original's browse does, with its message for 6 s: Core's `Failed launching
URL` over `Can't find file '<map>'` -- a console's `open` of one too, which
travels as any. The map's load paints its LOADING frame before it checks
the map, as the original's does, so the pauser goes and the sounds stop
whether the map loads or not. Within the map's mission the level plays on
as the pre-travel left it, pruned and saved into `Current`, after its
SAVING and LOADING frames: the player without its augmentations, skills and
flag base. For any other map `Current` is emptied and the level's own map
loaded again on the travel's URL, with its travel info, its LOADING frame
painted again: the level begins again from `Maps`, the message gone with
the pawn that showed it, or stays where its URL names no map there, one from
`Current` or restarted (`BrowseConsole`: a missing map outside any mission
and one of the island's, alike in both engines; `FailLoadingConsole`: SAVING
then LOADING within the mission, LOADING twice outside any, and the pauser
'' after a travel asked while paused, in both engines). A travel's URL
writes its player options -- `Name`, `Team`, `Class`, `Skin`, `Face`,
`Voice`, `OverrideClass` -- to the user ini's `[DefaultPlayer]` once its
map is in, as the original's map load writes them, so one that loads no
level writes none (`FailLoadingConsole`: `?Name=DXCapBob` to a map not
there leaving `Player`, in both engines). The check is the original's
check of the map's package: a map not there, not a package, with no
`MyLevel`, or importing a package -- or one a package it imports imports
-- that is not on the paths fails the load before anything of the level
goes, logged as the original's logs it, a map of a version above the
game's own read as the original reads one (`LocalMapConsole` on the
sweep's `badpackage2.py` maps: the start on `Dx.Dx` for `NoLevel.dx`,
`Failed to find object 'Level None.MyLevel'`, and `NoPkg.dx`, `Can't find
file for package 'CoreTexGlasz'`, and on `BadVer.dx`, version 100, in both
engines); a join's map that fails it gives the join up, `Connection failed`
over the error for 4 s (read: Engine.dll `0x10390f1e`-`0x10390f71`). A
travel asked during a travel's own browse -- by the console's tick under its
SAVING or LOADING frame -- is dropped, as the original's game engine
empties the viewport's travel after the browse, and one asked while a
server's next map loads waits for the next frame (`LoadTravelConsole`: an
`open` during the island's LOADING and one during SAVING dropped in both
engines). The console's
`switchlevel` is the player's exec function, whose `ServerTravel`
Deus Ex's `LevelInfo` passes over in standalone: no travel there. A
`?restart` (the console's `RestartLevel`) loads the `DeusExLevelInfo`'s map
again with `?loadonly` and no other option, carrying nothing: the player
starts afresh, its login with no name, as the original's
([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving)).
Where that names no map -- 00_Intro's `Intro`, 99_Endgame1 to 3's
`Endgame1` to `Endgame3` -- the restart fails as any load that fails its
check, `Current` emptied first, and the level plays on (`RestartNameConsole`
on 00_Intro: `Can't find file 'Intro'`, the intro on, in both engines); a
server's restart on DXMP_Area51Bunker, whose names none, alike, its next
URL cleared. `?push`, `?pop` and `?peer` give the new level's
`HubStackLevel` as the original's map load gives it: the level left's plus
one, one less but not under 0, the same; a save's load its file's own, and
any other travel, a level from `Current`'s too, 0. A `?push` saves the level
left into the save path's `Game%04i.dxs`, numbered by its own; nothing loads
it, the original's `?pop` looking for `Game%i.usa` and travelling as any
(`HubConsole`: 0, 1 and 1 at the island, UNATCO HQ and Battery Park, and
`Game0000.dxs`, in both engines; the original's second `?push` then
asserts, which DXEngine does not copy, going on with 2, 1 and 0). No
stock map, exit or script travels so, nor reads `HubStackLevel`. Differs
(bugs.md PKG.42): a hub save holds the player's pawn, where the
original's holds none, its map load destroying the player before the save,
as DXEngine's does at no map's load.

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
within 1% of the original's). The word, which the console centres at a half
pixel for an odd size, lies on the original's rows, the canvas's text
truncated to whole pixels ([tiles](#tiles); `LoadingConsole` at 1024x768
against `D3DDrv`: `LOADING` on rows 371 to 393 and columns 439 to 584 in
both). The canvas is the viewport's pixels at any size, as the original's,
so the console and the word are drawn at their font's own size
(`LoadingConsole` full screen at 2560x1440: `LOADING` 146 by 23 at 1207,
707 in both engines). The painted frame ticks the console as every frame's draw does
(`UConsole::PreRender`), by the time since the last draw, after the level's
tick (`HopConsole`, `SpinConsole`: an actor's ticks after the console's exit
as the original's): `LoadingConsole`'s log, a tick under each level action
among its lines, alike in both engines. With the render device's
`UsePrecache` on, which the game's ini has off, the frame after each map's
load is drawn under the level action Precaching, its console's tick too:
the console's `PRECACHING` over the view, as the original's with `D3DDrv`'s
(`LoadingConsole` with `DXCAP_PRECACHE=1`: the same frames, words and ticks
in both engines, the original on `D3DDrv`). The console's `FLUSH`, which
the main menu and the brightness menu send, and `BRIGHTNESS` draw no
`PRECACHING` frame after them, as none comes after the original's
(`RenderWordsConsole` with `DXCAP_PRECACHE=1`: Precaching at the map's
first tick alone, in both engines).

A frame that travels -- a client's travel, a save's load, a server's next
map -- ends with its load and draws nothing, as the original's game engine
returns from its tick there
([the game engine's frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
a travel asked for during the level's tick is taken before anything is
drawn, and one asked for during a draw -- a console's tick -- waits for the
next frame's tick, the old level ticking once more (`SleepConsole`, its load
asked for by its own tick and, with `LoadByActor`, by an actor's: the ticks
before the load alike in both engines). A server's next map waits out its countdown by the frame's real
time, not the level's, and a join under way ticks after the level's tick in
a frame that takes no travel, its connecting message shown at each frame's
draw, as the original's.

A map's name with a space after it, as Mission 14's five travels have it
(`14_OceanLab_Lab.dx #Sunkenlab`), finds its map as the original's Windows
file system finds it, a name's trailing spaces and dots dropped, and its
save in `Current` by the name cut at its first `.dx`, as the original's
Browse cuts it; the level's URL keeps the space, as the original's does
(`SpaceURLConsole`: the five travels, and the lab left for the UC and come
back to from `Current`, alike in both engines).

An object `new` makes with no outer is made in the calling object, as the
original's (`NewOuterConsole`: the console's and an actor's
`new class'CapNewProbe'` in each, `Transient.NewOuterConsole0.CapNewProbe0`
and `DX.CapNewMaker0.CapNewProbe3` in both engines), `new(None)` in the
transient package; Deus Ex's own scripts write `new(None)` or
`new(Self)`. The console object is named after its class and a number, as
the original's.

A save writes its SaveInfo into its slot alone, as the original's; the
Load and Save Game screens read a slot's as the original's `GameDirectory`
reads it, -1 the quick save's and any other number's `Save%04d`'s, so the
row of a folder whose SaveInfo is missing (`SaveInfoMissing;;;;-2`) shows
none, `Save-002` having one only after a save to slot -2. The load of slot -2
reads `Current`'s SaveInfo, which only a load's copy of a slot puts there:
with none it loads nothing; with the quick save's (`DirectoryIndex` -1) it
copies the quick save and loads it; with another's it empties `Current`,
copies `Save-002` and fails (`MissingSlotConsole`: `GetSaveInfo(-2)` None
after slot 9's save, its load nothing, and after a quick load the quick
save, in both engines). A save on a level with no DeusExLevelInfo
(99_Endgame4, the credits' dance party) saves nothing, logging `WARNING!
DeusExLevelInfo MISSING!`, and a level saved, its file and its SaveInfo's
`MapName`, is named by its DeusExLevelInfo's `MapName`: 99_Endgame1's file
is `Endgame1.dxs`, thirteen stock maps' in another case than the map's
(`NoInfoSaveConsole`: no `Save0009`, and `Current` holding `Endgame1.dxs`,
in both engines). A slot's folder is made with the folders it is in, as the
original's makes the whole tree, so a first save into a `SavePath` whose
parent is not there is written (`SaveConsole` with
`SavePath=..\DXCapSaves\Deep`: `DXCapSaves\Deep\Save0009` in both engines);
a slot's folder, `Current` and their files are found in any case, as
Windows' file system finds them, so a slot whose folder is spelt `save0009`
loads (`LoadConsole` on `SaveConsole`'s slot, its folder renamed, in both
engines); and a slot's copy takes its save files, `*.dxs`, alone. The Load
Map window lists each map once, in any case, the first path's, as the
original's `GetMapsDirectory` passes over a name it has listed
(`MapListConsole` with `02_nyc_bar.dx` in `DXCAP_MAPS`'s folder: it alone,
in both engines). The console's `SAVEGAME` and `DELETEGAME` act only when
the text after the word begins with a digit, their slot its leading digits,
and `SAVEGAME`'s description is always none, the save taking the level's
Title, as the original's game engine reads them; every other form is taken,
doing nothing (`SaveWordsConsole`: slots 71 and 72 saved, `+73`, `-74` and
`DELETEGAME +71` doing nothing, in both engines). By choice, `GetSaveFreeSpace` measures the save
folder's file system, held to the largest int, where the original's
measures the current directory's drive, wrapping past 2 TiB free, and
answers 0 when the call fails.

## Flags

Matches the original
([flags](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#flags)):
each engine finds the other's saved flags. Mission scripts come up as the
original's: `MissionConsole` over all 83 maps finds every mission map's
script running. `DX`, `DXOnly` and `Entry` are not mission maps, and
`12_Vandenberg_Tunnels` (no `MissionScript` actor) and `99_Endgame4` (no
`DeusExLevelInfo`) run none in the original either. The player's flag base
is its own, made as the pawn is made, as the original's constructor makes
it: the pawn its outer, its name the class's count, at a spawn and at a load
alike, a load's saved base then taking its place, and a travel that brings
the player's flags making another (`ReturnConsole`: `JCDentonMale1.FlagBase1`,
`JCDentonMale2.FlagBase2` and `JCDentonMale1.FlagBase3`, with `Items`
`FlagBase2`, `4` and `6`, in both engines).

## Conversations

Matches the original
([`ConSys.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md)):
`SkipConsole` compares a skipped line's speech and each line's length, which
the script waits for, read from the sound's header as the original reads it
(an MP3's bits over its first frame's bit rate: 2.834 s for a line the
decoded samples make 2.847 s), `CaptureConsole` a jump to a comment's label,
`EndgameConsole` an ending skipped to its credits: 99_Endgame2's
conversation stopped, its Played flag, the mission script's timer and quote,
and the credits up at the level's fifteenth second, alike. A random label
event picks as the original's, its `cycleIndex` and `bLabelsCycled` too:
cycling, the labels in turn round and round, the index counted on; with
`bCycleOnce` the last label for good, the index on it, never cycled, so
`bCycleRandom` with it never picks; with `bCycleRandom` alone a random label
once the labels have cycled (`RandomLabelConsole`: `BoatWaitingForBriefing`'s
event under each, every line alike but a random pick's label).
`ConBindEvents` binds from the level's `ConversationPackage`, an empty one
or one that is not there binding nothing and keeping the actor's list, as
the original's (`ConPackageConsole`: Paul Denton's ten under the stock
package and `deusexconversations`, none under an empty one or
`NoSuchConPackage`, in both engines). An actor's names and a
conversation's owner compare as strings, without regard to case, an empty
one as any other, as the original's: a conversation with no owner is every
actor's with no `BindName`, a bark with none every actor's with no
`BarkBindName` (`EmptyBindConsole`: two of Liberty Island's conversations
made ownerless, both bound to a decoration with neither name, the plain one
alone once it has a bark name, neither once the owners are back, in both
engines); no stock conversation has no owner. Each of an actor's
`ConListItems` is made in the list's package, named after its class and a
number (`ConListItem0`) and public, the old list's deleted as the actor
binds again, as `CriticalDelete` deletes
([housekeeping](#housekeeping-not-seen-directly)): gone from `AllObjects`
at once, a reference to one None at the next collection, where the
original's frees them; `CreateConCamera` and
`CreateFlagRef` make theirs in package `DeusEx`, named and public
(`ConObjectsConsole`: Paul Denton's first item, the counts around a second
bind, two cameras and a flag reference, each one's path, name, flags and
`Outer` alike in both engines).

A `cycleIndex` a script sets below 0 gives no label, by choice: the
original's copies a string from before its labels, and crashes the game
there (`RandomLabelBelowConsole`: a Critical at -1, ConSys.dll
`0x1000a270`); no stock event or script sets one.

## What the player reads

Matches the original's parser
([`DeusExText.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusextext-dll.md)):
a comment's tags (`NOTE`, `GOAL`, `COMMENT`) parsed with their effects, a
token's tag the last a name matched, a tag's text its own, a player's name
or a bracket, else none, and the text's end ending a token as a tag does, so
that a text whose first paragraph never starts and that ends after its last
tag, as 15 email lists and `03_BulletinMenu` end, in a line break, gives a
last token, empty with no tag, which the game passes over; the call after
the last token gives an empty token with no tag, and `IsEOF` is true from it
(`ShipOpsConsole`'s `04_EmailMenu_klloyd`). Differs by choice at a comment
with no end tag: the original reads on past the end of its copy of the text,
to the first NUL there, so a tag in the memory after it changes the token
(`ShipOpsConsole` with `Provoke`: a `<B>` freed just before, read as the
token's tag), where DXEngine takes what lies past the end for NULs. That
gives `09_EmailMenu_ShipOps`, the game's one comment left open, the listing
the original gives in play: one email, its fields empty, the token's tag the
last inside the comment (`ShipOpsConsole`: five runs of the original, from
its menu map and after a map's load, every read and the ship's computer
logged in to as ShipOps alike, and DXEngine's the same, every line of the
controls' reads alike).

Its names and colours match too (`TextTagConsole`, texts of its own, every
line alike in both engines): `GetName` the last name at a NOTE, G, F or L
token, which G, F and GOAL set to what follows the tag's first `=`, None
without one; `GetColor` the last colour at DC or C, at /C the parser's
default, which nothing sets, black at any other, DC, C and /C each setting
the last colour alone; a tag's fields what follows its first `=`, a FILE
with none keeping the last ones; a new parser's tag `TT_None`. No stock text
has these forms, and no stock script calls `GetName`.

## Every NPC

What `DeusEx.dll` and `Engine.dll` do for every NPC, piece by piece.

### The native tick: `AScriptedPawn::Tick`

Matches the original
([the native tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-native-tick)),
run before the actor tick, in the original's order. A wound drips a drop
each 0.1 s at the most, however hard it bleeds, its `BleedRate` held
between 0 and 1 for the drop's period as the original holds it
(`CensusConsole`). A burning pawn's fire goes out as its `burnTimer`
reaches its `BurnPeriod`, the sum compared as the x87 holds it (read: the
original's at `>=`, as the game's script source has it). Differs by choice:
bleeding with `ClotPeriod` at 0 or under
stops, where the original divides by it, a 0 s frame's NaN sticking; no map
sets it.

### Starting up

Matches the original
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
every actor starts a level 10 s undrawn and every dynamic one ticks in its
first tick, so a pawn out of sight starts from its state code (`AIConsole`).
A pawn drawn in the first frame follows its orders at the second tick, from
its `StartUp` state's `Tick`, and one out of sight when its state code's
sleep, 0.2 to 1.2 s, runs out: on Liberty Island `SecurityBot0` at the
second tick, its patrol's first `MoveTarget` `PatrolPoint19`, and
`SecurityBot1` at its sleep's end, `PatrolPoint14`, by tick and by the
actors' time as the original's (`PatrolStartConsole`). By the level's clock
and a console's, which take in the first frame's load whole where the
actors take 0.4 s of it at most ([a long frame](#small)), the original's
pawns start later, by what its longer load took over 0.4 s: a console's
probe 1 s in comes at its first tick, before any pawn has left `StartUp`.

### Changing state

Matches the original
([events and probes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#events-and-probes)):
a script's `GotoState` into the state the object is in only goes to the
label, with no `EndState` or `BeginState` and its probes as they are, so a
`Disable` in `BeginState` holds. Private Lloyd's walk to his chair on
Liberty Island meets it, and the chair's `Bump` sends him to his own
state's `CircleToFront`: he sits in `SitBreathe` from then on in both
engines (`SitConsole`). In no state an actor's state node is its class, as
the original's state frame has it: `GetStateName` gives the class's name,
and `IsInState` is true for the class and its supers, while a script's
`GotoState` compares with None there (`NonePreemptConsole`:
`CapNonePreempter` sent to no state from within its `EndState` gives
`CapNonePreempter`, in both engines). An object `new` makes has no state
frame, as the original's, so its `GotoState` does nothing; the script's
`GotoState` logs `GotoState (<state> <label>): State not found` for a state
not reached but None and `Auto`, and `Label not found` for a named label
not found (`StateObjectConsole`: a new object's `GotoState('Foo')` leaves it
in no state, its own `Which` run, the warning logged, in both engines;
`NonePreemptConsole`'s `NoSuchState` the same).

A state's code runs in its actor's tick alone, never within the
`GotoState` that sets it: the terrorists Liberty Island's script kills as
the statue's mission ends (`Mission01.Timer`: `TakeDamage` at no health,
then each one's inventory deleted) leave empty carcasses in both engines,
spawned by their `Dying` state's code 1.3 s later (`StatueConsole`).

### Hearing: the AI event system

Matches the original
([the AI event system](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-ai-event-system)),
saved in its layout
([saved](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#saved)):
each engine loads the other's saves with every listener. DXEngine also reads
the manager in a layout of its own, which some of its saves carry.

`AIGetLightLevel` walks the level's actors at each call, as the original's,
so every light counts before a level's first frame, after a travel and a
load, and a light turned on or moved counts within the tick
(`LightStartConsole`: on UNATCO HQ at `TravelPostAccept` and the first tick
`Light150` 0.363930 in both engines; `LightTreeConsole`: 0.504307 and
0.530198 the tick one light is turned on and another moved). A listener made
during the manager's calls goes in before the frame's first receiver, and
the next walk starts after the last that had its turn, so after a full walk
it comes first (`EventRingConsole`: 3, 0, 1, 2 each tick in both engines);
an actor already destroyed raises nothing (no call after `CapNoiseMaker0`'s
`Destroy`); every turn sets a receiver's best score and sender, 0 and None
with nothing sensed, and a save keeps its last call's state
(`EventRingConsole` with `SaveAt=3.7`, `aisave.py`: 0, None and 2 in both).
A None event name does nothing, a score that is not a number drops its
sender, each actor's events go to its own level's manager and each level's
tick ticks its own, the Entry level's too, and the warnings are the
original's lines, the one of 256 senders at the 256th and the one of a
manager not cycling quickly enough once a frame with its count
(`EventEdgesConsole`, both engines: no call for a None event or a score
that is not a number, the Entry level's listener sensing its own level's
maker, the warnings' lines and counts); a non-pawn's visual check is one line check that any
wall or mover stops, whether it blocks sight or not, as the original's (by
reading: no stock listener that is no pawn listens for a visual event).
By choice, the warning of 256 senders names the event, where the
original's names the event type's own object (`AIEventType<n>`).

### Moving: wandering and tactical movement

Matches the original
([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving),
[reaching](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#reaching),
[the
search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)):
`MoveConsole` (`move.py`) has all 52 of Liberty Island's pawns move within
15% (or 200 units) of the original's distance. Each that walks to a spot and
stops there ends where the original's does, within a unit and a half, though
25 to 110 units short of its distance: the console counts from its first
line, 2 s in by its own clock, which takes in the island's first frame, the
load's, longer in the original's ([a long frame](#small)): by then
DXEngine's pawns are further along their first walk. `ReachConsole`'s 144
answers, twelve pawns each asked from its home spot, are the original's to
the unit. A strafe's move timer counts at the speed the strafe sets, all of
`MaxDesiredSpeed` held to the speed asked, as the original's `StrafeTo` sets
it before the timer; the timer takes a pawn with no physics at 200 units a
second and counts only while the pawn's physics run, as the original's
`setMoveTimer` and `performPhysics` have it, so a strafe or move of a pawn
with no physics never runs out (`StrafeConsole`). A jumper's landing is
judged by the zone its feet land in, each test move of the jump's fall
setting its feet's and head's regions as the original's test moves set them,
so a jump into a pain zone the pawn does not resist is refused. A dead robot
frozen in `Dying` for good is the original's own behaviour, and the rest of
a death is alike too (`DeathConsole`). `Terrorist34`, walking west to
`PatrolPoint86`, comes to a stop at x 1510 some 8 s in, still walking at it,
and stands there until his move's time runs out (`MoveTimer`) some 12 s
later, then steps back and goes on: in every original run, and in every one
of DXEngine's held to the original's 120 frames a second, or to 60
(`MoveConsole`'s `Watch`, `move.py --pawn`). A walker's heading drifts
with the frame rate, in both engines alike: after his turn toward the spot
he drifts across his way, 0.06 units a second toward the spot's side at 120
frames and 0.1 away from it at the original's 227 with vsync off, where he
meets the wall half a unit further along. DXEngine uncapped, at some 300,
meets it further along again, and there in some runs he slips past within
two seconds.

The script's `ActorReachable` of a navigation point, asked by a pawn no
wider than 115 units standing at a node (within 48 units across, or its
radius where wider, and its height up or down), answers from the path
network as the original's does: reached where the pawn stands at it, or
where a spec of such a node, in its `Paths` or its `PrunedPaths`, ends at it
open to the pawn; else not, but for a flyer, which walks it as a pawn at no
node does (`NavReachConsole`: twelve of Liberty Island's pawns, each put on
its nearest node, all 87 answers the original's, among them nodes only a
pruned spec reaches). A `FindPathToward` whose goal no node near it reaches
takes the original's second way, the end points about the goal and the
search from the pawn's own first node (`SecondWayConsole`: the 24 routes the
original's). `AIPickRandomDestination` with a multiplier under 1 stops at
the first step past that share of the way found, as the original's does
(`PickDestConsole`: all 48 picks the original's).

The reach tests move the pawn as the original's test moves do: passing
over pawns and decorations that are not static and what stands on the pawn,
not what it stands on, and stopping at what blocks it by the original's
`IsBlockedBy`, so a pawn on a lift walks its floor and a door stops a walker
or a flyer that collides with the world though it blocks no actors
(`LiftReachConsole` on 06_HongKong_MJ12lab: from `ElevatorMover0`'s floor
each of four points across it found and reachable, and at `DeusExMover15`
no walker, flyer or seagull through, alike in both engines but for the
distances' last digits); a test move of nothing, each axis
under 0.0001, leaves the hit of the move before it, as the original's
leaves it. A jumper's landing is found with the same moves, held off 2
units and through pawns and crates, the jump rising at the pawn's `JumpZ`
at a speed across worked out in the original's float order
(`LineEdgeConsole`'s jumps), and a fall of 350 or more logs the original's
`FAILING FBJ` line (`JumpLandConsole`: every line the original's, the logged
distances to the x87's last digits). `ReachablePathnodes` gives the
original's list: a start node's paths up to its first empty one, or with no
start node 32 nodes within 1,000 units taken in the level's order, each one
after in place of the nearest held one farther than it, then those the pawn
can reach; equal distances in the order the C runtime's qsort leaves them,
and the node None once the list is done (`PathnodeConsole`: the lists and
their order the original's, the distances to the x87's last digits).
`AIPickRandomDestination` drops the pitch for a pawn walking, or swimming
out of water, whatever its zone (`PathnodeConsole`'s pick in the water), and
`FindPathToward`'s test from the anchor traces the goal's line to the anchor.
The collision hash gives actors back in the original's order: a
coordinate's cell the coordinate plus 32768 over 256, rounded to the
nearest and a half to even, so a cell's middle lies on a multiple of 256;
an actor filed in the cells of its cylinder a unit wider and taller each
way, or of its brush's box where its coordinates put it; the cells walked
X, then Y, then Z, each cell's newest-filed actor first; the radius and
line checks' lists reversed, the encroachment check's not; a trace's hits
sorted as the C runtime's qsort sorts them, up to 8 (`HashOrderConsole` on
Liberty Island: `VisibleCollidingActors`' order, a mover's touches,
untouches and slots onto five marks, off and back, alike in both engines).
By choice, a swimmer of no mass is taken as of 1, where the
original's divides by the 0, its velocity no number; the game's one pawn of
no mass, GeneratorScout, flies and never swims.

The ladder probe goes on past a texture in no group, as the original's, the
texture's outer its package (`LadderConsole` at the foot of
12_Vandenberg_Cmd's ladder, where the wall under its plate,
`ClenCmandWall_A`, is in no group: the player climbing from the first tick,
86 units in a second in both engines).

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
(`AugVisionConsole`). An actor's proxy joins the walk at the BSP's root,
as each of the original's sprites does, cut at each plane on its way down
and dropped where every corner lies within 0.01 of one (`ProxyWallConsole`:
crates 24 to 96 units behind six of Liberty Island's walls at five yaws,
each of the 120 views alike in both engines, where the build before
differed in 4). It lies at its location's own depth, however near the eye,
its corners tested where their lines from the eye lie 1.5 units out, the
clipper's near plane cutting away what is nearer (`NearProxyConsole`:
crates 0.05 to 1.4 units before the eye, a wall 1.25 units off, counted as
drawn on six walls in both engines, the one at 2 units not; with
`Gap=2.0` every crate counted in both). A sprite whose left edge's pixel is the frame's last column or
past it has no rectangle and does not count as drawn (`SpriteEdgeConsole`:
its left edge 2.5 pixels short of the right edge seen in both engines,
1.75 and 1.25 in neither, where the build before saw them). The walk
takes a mover's polygons where the brush tracker filed them as surfaces of
the level's own, an opaque one filling the spans as a wall does, so what
lies behind a closed door, an actor or a zone portal, counts as hidden
(`DoorHideConsole`: a crate 40 units behind Liberty Island's
`DeusExMover10` not drawn while the door is shut, its `LastRendered()`
rising, and drawn each frame once it opens, in both engines, where the
build before drew it throughout). A decal counts as drawn where some of it
is left on a polygon of its surface any span of which shows, the polygon
cut to the view's sides, no span buffer asked of the decal itself
([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time)):
one behind nearer geometry, on a surface in view, counts. The game's go
when not drawn for a second, some 20 s after they land (`DeusExDecal`). A
surface's decals are drawn in the order the original's `AttachDecal` files
them, a new one before the first of its texture, and one whose texture is
`None` in the level's `DefaultTexture` -- no stock decal's. Differs
(bugs.md REN.62): a decal on a node the original's walk leaves out of its
saved polygons can count as drawn (`DecalSeenConsole`: of 29 marks the
eye's line to is blocked, on the sea floor past the dock, 23 counted, the
original's 15 among them, where the build before counted none). Differs by choice:

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
draws in 3D and the frob highlight's box all take the one angle
(`HorPlusFovAngle`); the meshes' level of detail takes the angle before it
is widened, the original's, so a mesh keeps the original's detail at the
same distance ([mesh detail](#mesh-detail)). A conversation's letterboxed view is wider than 4:3
whatever the screen, so there DXEngine sees more at the sides even at 4:3:
the intro's scenes at 1024x768 some 92 degrees across, the original's 75
(`IntroDoorConsole`).

### The near plane

Matches the original
([the depth](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-screens-pixels)):
what lies nearer than a unit before the eye is cut at view depth 1 on every
device and in every frame, a mirror's too, as `D3DDrv` cuts it, its depth
under 0, where `Render.dll` clips a frame at the view's four sides alone,
which meet at the eye, and hands it over
([which faces are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#meshes)):
`NearConsole`'s quad half a unit before the eye drawn by none, the slanted
one cut at the screen's middle row, by `D3DDrv` and by the Vulkan, GL, GL ES
and software devices alike, where Vulkan's and GL's depth clamp drew both
whole. A mesh corner within a unit of the eye is drawn where it is, by
choice, where `Render.dll` skips projecting it and draws it at the screen
place and depth last left in memory, a stray vertex. What lies past 32,769
units is cut on every device, as `D3DDrv` cuts it at 32-bit colour, its
depth over 1: each keeps F ÷ (F − 1) as a float, 1 + 1/32,768, for
`D3DDrv`'s F of 32,767 and DXEngine's 32,768 alike. No stock view reaches
it: the farthest surface a player sees lies some 24,000 units off, across
Liberty Island, whose start's view Vulkan, GL, GL ES, the software device
and D3D11 draw as `D3DDrv` does in a window, each within 1% of its frame's
brightness (`FarConsole`).

Differs by choice: DXEngine draws at 32-bit colour whatever the Texture
Color Depth ([the menus' settings](#the-menus-settings)), and so as far as
`D3DDrv` does at 32 bits. `D3DDrv` at 16-bit colour, full screen at the
game's own setting, cuts what lies past 8,193 units, which a view passes on
19 of the stock maps and in Hong Kong's skies -- a band of Liberty Island's
first view, the Statue of Liberty itself gone from the original's frame at
16 bits and there at 32 (`FarConsole` full screen at both) -- and draws the
first-person weapon with its near plane at a quarter of a unit.

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
or not, takes its style's flags, and is laid out as the original's
`WrappedPrint`, `DrawTextClipped`, `StrLen` and `TextSize` lay it
([the canvas's text](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-canvass-text)):
a glyph found on page c / `CharactersPerPage`, one the font has at no width
taking none; a line from its X and the canvas's `CurY` each truncated, a
centred one's half the room its end leaves from `CurX` added, each glyph a
whole pixel's step (its width and `SpaceX`, truncated), ending after the
last word that fits, its space out of its width, or within a word wider
than the room after its last glyph that fits; a line feed, or a first glyph
wider than the room, ending the print; `CurX` after it the furthest a line
ended, from `CurX`'s whole pixels; `StrLen` leaving `CurX` and `CurY`
truncated; `DrawTextClipped` centring nothing, cutting a glyph across the
clip and going on, its hot key's `_` over the letter's place at most as
wide, and `LargeFont` and `BigFont` giving way to `MedFont` where the
language is not int. `CanvasTextConsole` with MedFont: every measure and
place the original's, its lines alike in both engines, where the build
before parted at a line feed, a long word, a glyph of no width, a wrap's
space, `StrLen`'s place and a centred line's `CurX`. `TileConsole`, on the canvas and in a
window over a grey box: `StaticLine` modulated (the line across a camera's
and the InfoLink's static, a quarter of it colour 0, black) at 1.02 of
`D3DDrv`'s brightness; `HackInfoBackground` modulated (the hacking screen's
message, all colour 0, dark grey) drawing nothing, at 1.00; in the window,
`HUDScopeCrosshair`, a masked texture, drawn `DSTY_Normal` at 1.00, its
lines alone; "NEW GAME" in the TrueType font, drawn opaque, at a
correlation of 1.00 to `D3DDrv`'s, its edges blended into the box (the
stock menus draw it translucent); on the canvas, clipped text drawn
`STY_Translucent` translucent. By choice, a smoothed tile's edges are
clamped on every device, where `D3DDrv`'s addressing wraps, its outer half
texel blending with the opposite edge, which no stock art relies on.

The canvas's `DrawTile` and `DrawTileClipped` are the original's
([the canvas's tiles](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)):
no texture logged and the cursor left; any other tile moving `CurX` on by
`SpaceX` and its width, whatever its size, and `CurYL` to the larger of it and
the height, `SpaceY` no part of it; `STY_Masked` adding no flag of its own, the
texture's masked bit the tile's; a clipped tile of no width or height doing
nothing, one from a negative `CurX` or `CurY` cut there and the cursor made 0,
then cut at `ClipX` and `ClipY`, its texels in proportion, and the cursor moved
by what is left (`TileEdgeConsole`: every case's `CurX`, `CurY` and `CurYL` alike
in both engines, 7 of 14 lines apart on the build before; a texture not masked
drawn `STY_Masked` whole, its colour 0 drawn, 3,850 of the box's 102,400 pixels
not magenta in both, where the build before masked it, none magenta).

A window's tile and character are placed as the original's GC places them
([the GC's styles](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#styles)):
clipped to the GC's box in the window's units -- along an axis the tile
repeats, its source cut one for one with it; along one it stretches, in
proportion, then held to the texture -- and only then put on whole units,
the place, the size and the source each truncated, so a window at half a
unit draws on the unit before it. `LoginConsole` at 1280x960: a computer's
login screen, centred at half a unit, its fields' first letters lit as the
original's pixel for pixel, their frames' outlines on the original's
pixels, the whole screen alike but for the insertion point's blink and the
pointer.

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
(`shots.py --shift`). `FaceConsole`'s troop, `MeshConsole`'s Paul Denton,
the mirrors and the light maps register so too ([mesh detail](#mesh-detail),
[mirrors](#mirrors), [lighting](#lighting)), as a sprite and the canvas's
tiles do ([sprites](#sprites), [tiles](#tiles)).

Auto-panned surfaces scroll by their zone's pan speed and the level's own
time as Render.dll pans them, 35 texels a second at a speed of 1, wrapping
at 1,024, truncated to 1/256 of a texel (`PanConsole`: Liberty Island's
cloud banks where the original's are at 6 s and 8 s of the level's time,
within 2 pixels at 1024x768). A `PF_SmallWavy` surface, which no stock map
has, wobbles about that pan by 8 sin t + 4 cos 2.3t texels along U and
8 cos t + 4 sin 2.3t along V, t the level's time, as Render.dll pans it
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)),
by reading.

A NoSmooth texture is point-sampled from one mip, with no anisotropy, as
`D3DDrv` samples one, on Vulkan, GL and GL ES alike, `LODBias` given GL ES's
draws in its scene shader: its mip the floor of log2 of its texels to a
pixel (`MipConsole`: a floor of `CapMips` laid under the view, drawn
NoSmooth, one texel to a pixel and each mip from `D3DDrv`'s rows, 0 of 362
apart at 1024x768; smooth, GL ES's mips GL's, 0 rows apart).

A masked texel shows where its filtered alpha is above 127/255 on every
device, as `D3DDrv`'s alpha test keeps it (reference 127, greater:
[the fixed state](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#d3ddrvdll)):
`MaskEdgeConsole`'s chain link at 1280x720, its 16 by 16 texels at x20 33,352
pixels lit through `D3DDrv` and 33,314 on Vulkan, GL, GL ES and the software
device, as with a test of 0.5 (the filtering's own rounding, not the test,
parting them); at x5 42,173 and 42,176; point-sampled 33,200 in all.

### The software device

Matches `D3DDrv`'s picture as the GPU devices draw it without their
multisampling, bloom and HDR, settings the original lacks, by choice
([rendering](ENGINE.md#rendering)), into an 8-bit frame as `D3DDrv`'s, its
textures sampled as `D3DDrv` samples them with the game's ini, `LODBias` at
its default: bilinear, from the one mip whose texels are no larger than the
screen's pixels, where the GPU devices filter a smooth texture trilinearly
with 8x anisotropy, by choice: no seam between mips, sharp at a slant. Against
`D3DDrv`'s frames: `LightConsole`'s `Spotlight0`, with its macro texture and
without, and `Spotlight7` at 1.00, as the GPU devices'
([lighting](#lighting)), and `Light33`'s floor within 1.02; `LaserConsole`'s
wall and near floor 0.40 and 0.40 apart (RMSE, at 1024x768 with `NoDetail`)
and best unmoved, its receding floor 0.66 apart, nearer than the Vulkan
device's; `LoadingConsole`'s text on the rows `D3DDrv` draws it, as the
Vulkan device's, with multisampling and without: every column of the word's
glyphs, its first and last lit rows, the original's (the canvas's text on
whole pixels, [tiles](#tiles)). Against the Vulkan device's: `TileConsole`'s tiles and text
within a level, as are `DetailConsole`'s bands (`detail.py` as the Vulkan
device's). It draws where the original's software renderer does
([choosing the render device](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#choosing-the-render-device)):
with `-nohard`; where the game's ini names it
(`GameRenderDevice=SoftDrv.SoftwareRenderDevice`, the recreated launcher's
Software Rendering) and `Settings.json` names no device; and where the GPU
device does not start, as the original's `WindowedRenderDevice` takes
`GameRenderDevice`'s place, the error logged
([which device draws](ENGINE.md#rendering)). In the `WindowedRenderDevice`'s
place -- with `-nohard`, and for a GPU device that did not start -- it draws
with that class's settings, its section's, SoftDrv's in the game's ini, as
the original's software renderer does: no coronas and no shiny surfaces
(`CoronaConsole` with `-nohard`: no glow in any of its six shots in either
engine); a `SET` or `TOGGLEDETAILTEXTURES` saves them in that section, the
engine's own left to `GameRenderDevice`'s device. Differs by choice:

- **Detail Textures under `-nohard`**: the Display menu's checkbox, which
  sets `GameRenderDevice`'s, reaches the software device in use, where the
  original's reaches `D3DDrv`'s class, changing nothing on screen.
- **Vsync** only through SDL's renderer: X11's and Win32's presents do not
  wait for it, and the engine's own Wayland window (a dev build's) copies
  the frame into its buffer and commits nothing of itself: untried with the
  software device.

A texture's coordinate wraps at any distance, as the device's float
reference, the GPU devices and `D3DDrv` wrap it: a float past 2^23 is its own
floor (`HugeTileConsole`: the tile at U 1e12 drawn, its shot alike with the
reference's, `DXE_SW_REFERENCE=1`, and the Vulkan device's, where the device
read some 8 GiB before the texture and dumped core).

### Sprites

Matches the original
([sprites](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#sprites)):
a sprite draws upright, the texture's first row at the top, with its
actor's style -- `STY_Masked` masked, `STY_Translucent` translucent,
`STY_Modulated` modulated -- its texture's own flags and its `bNoSmooth`; a
render iterator's items with their proxy's, so a `ParticleGenerator` with
neither `bTranslucent` nor `bModulated` draws its particles masked. A
modulated sprite draws as every modulated tile ([tiles](#tiles)).
A texture has one copy on every device, as `D3DDrv` keeps it
([one copy a texture](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)):
the draw that first sets it makes the copy, its colour 0 cleared or not as
that draw is masked or not, and every later draw takes it so until a
realtime change makes it again: the next draw takes the change, as the
original's lock hands it to one lock
([a texture's mips](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-textures-mips)),
and later draws take that copy. `SpriteConsole`'s icon, its colour 0
magenta, drawn in each style beside its particles, which draw masked, shows
none of that colour in either engine, the `STY_Normal` sprite's box black,
the `STY_Translucent` one's adding nothing (at 1024x768 against `D3DDrv`:
no magenta pixel in either, the sprites' band at 1.006 of the original's
brightness, correlated at 0.998); drawn alone, `STY_Normal`, its box is
magenta in both (32,351 and 32,471 pixels of it). Drawn `STY_Modulated`,
its box is at 0.96 and within 1% of `D3DDrv`'s brightness. A sprite
animated once (`DT_SpriteAnimOnce`) shows the frame its life has reached
and holds the last, as the original's does: `OnceConsole`'s chain of three
noise frames, its life held at 7.5, 5.6, 4, 3 and 1.5 s and none of its
class's 8, shows frames 0, 0, 1, 1, 2 and 2 in both engines, each shot
correlated with `D3DDrv`'s at 1.000 -- at 5.6 and 3 the frame `n × t − ½`
rounds to, one before the frame `n × t` would. The viewer's `Sprite` (the
Matrix cheat's) stands in for every texture of the frame, as the original's
`DrawFrame` has it
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)):
a mesh's, a sprite's at its own texture's size, the level's and a mover's
surfaces' with the Sprite's own detail and macro textures, a decal's; and
a corona drawn has it written into its light's `Skin`, where it stays once
the cheat is off. `MatrixConsole` with `TargetMap` on Liberty Island: the
level tiled with the light icon in both engines, the frame at 1.001 of
`D3DDrv`'s brightness, correlated at 0.885.

### Particles and lasers: render iterators

Matches the original
([render iterators](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-iterators),
[particles and lasers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#particles-and-lasers)):
`LaserConsole`'s laser fence, a still texture in its beams' slots, unlit and
lit, has each beam's brightest row within 2.5% of `D3DDrv`'s red, its two
lines a segment in `D3DDrv`'s rows, each row within 11 levels of 256 of
`D3DDrv`'s red without multisampling (with DXEngine's 4x, its default by
choice, a beam's edge rows are partly covered, a fraction of a pixel's
difference), and one
segment of each beam drawn twice, picked at random each frame as the
original picks it; and `ParticleConsole`'s particles rise, fall, grow and
fade as the original's: its smoke's column at 1.06 to 1.10 of `D3DDrv`'s
brightness, its puffs under gravity in the same rows. Differs by choice in
the occlusion: the original occludes each item's sprite through its span
buffer; DXEngine clips items to the view (and a portal's spans) before its
BSP walk and leaves the rest to the depth buffer. The look is the same; a
hidden item's cost is paid. Whether the proxy counts as drawn is the
original's.

### Coronas

Matches the original
([coronas](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#coronas);
`CoronaConsole`): the pass runs only with the render device's `Coronas` on,
which the game's ini turns off for SoftDrv (`CoronaConsole` with
`Coronas=False`, or with `-nohard`, the software device taking SoftDrv's: no
glow in either engine); the static lights reaching the
player's leaf get coronas, and no dynamic light does, the original's pass
reading the leaf's dynamic lights only after its occlusion pass has emptied
them. `CoronaConsole`'s own movable lamp shows none in either engine, in the
player's leaf or out of it, and logs the same BSP leaves in both. A light
behind a glass mover shows none in either: the line to it is checked against
the level and the movers with every surface, the invisible collision hull
round the glass's pane among them, which a sight test passes (`GlassConsole`
at 03_NYC_Airfield's `BreakableGlass2`, `Light56` beyond it). A corona's
skin is drawn at its current frame, an animated one's or a fractal one's, as
the original's: `CoronaSkinConsole` gives `Light169` a chain of three noise
frames stepped every 4 s, its current frame at each shot logged alike in
both engines and drawn alike (correlated with `D3DDrv`'s at 0.975), and the
fire texture `LaserBeam1`, its image changing from shot to shot in both.

A corona's colour is `DrawFrame`'s own (`0x10b1a2d0`): its light's hue as
two neighbouring primaries 85 steps apart, the last third's blue over 84,
whitened by its saturation, times its fade, none of the light maps'
brightness curve: by reading.

### What a pawn holds

Matches the original
([a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments));
`HeldConsole`. By choice, a held item is drawn wherever its holder's weapon
triangle lies, where the original's is dropped as the triangle leaves the
view: an armed NPC's gun gone at the view's edge, its barrel still in it.

### Viewport windows

Matches the original
([a viewport window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#a-viewport-window)):
a new window is enabled, at a field of view of 90, with a player's view
rotation and both eye heights used, and looks from the player's eyes; each
setter takes the original's defaults, clears what the original's clears and
keeps the original's references: an origin or a watch actor holds one of the
player's actor references, and an origin actor other than the player is one
of the player's `AdditionalViews`. The frame is drawn from the origin actor,
up its `EyeHeight`, or from the window's own place, and the script's
`CalcView` may move and turn it; the window keeps where it drew from, so an
origin or a watch actor gone, destroyed, leaves the view at its last place
and turn. The frame takes the window's clip rectangle, its corner and size
truncated to the pixel, at the window's field of view, with the player in
behind view for it, so the player and what it owns are drawn as in third
person, and the origin actor hidden unless `bShowActor`; with
`bShowWeapons` on the player's own view the player is not, and its
overlays, the first-person weapon, are drawn in the window's frame as in
the main view's. A window turned off, or with not a pixel in view, tiles
its default texture instead. `ConvertVectorToCoordinates` in a viewport
window, or a window inside one, projects through the window's own frame.
So a security computer's feed shows the player in the camera's view, its
arrow keys pan the camera by `fov / 90` of a step and its plus and minus
keys zoom, and the spy drone's window and the targeting augmentation's
zoom, at its top level aimed at the target's eyes, are drawn
(`CameraConsole`: every value it logs the original's; the feed within 1%
of the original's frame, correlated at 0.97; the zoom and the drone's window
within 0.5% of
`D3DDrv`'s; the console's own windows' frames correlated at 0.968 or more
with the original's, turned by a `CalcView`, turned off, clipped by their
parent, and the Dragon's Tooth drawn in one, its blade's blue as many
pixels as the original's as it sways). Differs:

- `ConvertVectorToCoordinates` asked outside a viewport window's draw
  answers through the frame it last drew, or as a plain window's before it
  draws one, where the original stops at an assertion, by choice: an
  assertion stops the game, and no stock script asks one so;
- a window wider than 4:3 sees more at the sides, by choice
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
- **Lit from its own eye**: a mirror's frame, as a sky's and a viewport
  window's, gives its meshes their highlights and fog, its fog maps their
  lines of sight and its coronas their sight from its own eye, as the
  original's frame walks from it: the lobby floor's reflected chairs, plants
  and mannequin nearer `D3DDrv`'s frames than lit from the player's eye.
- **`ShinySurfaces` off**: the lobby's floor solid marble in both.
- **A mover's mirror**, as a BSP mirror, the original filing a mover's
  faces into the BSP
  ([which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn)):
  `GlassConsole` with `Flip` on 06_HongKong_WanChai_Underworld's
  `BreakableGlass0`, an opaque one, its reflection, the frame's middle, at a
  correlation of 0.985 to `D3DDrv`'s frame, and on 02_NYC_Smug's
  `DeusExMover7`, a translucent one, 0.997; `OpeningConsole`'s view of the
  club, a doorway's glass in it, 0.987 over the frame. A face the tracker
  files nowhere, in solid, opens no frame, as the original's walk never
  meets it: 06_HongKong_WanChai_Underworld's six glass faces buried in
  solid, `BreakableGlass9` to `14`, passed over, the three beside them that
  the tracker files still reflecting (`FarConsole` turned to them from the
  start: three mover mirror frames a scene, where two buried ones made it
  five, counted by a tool not kept), the frame unchanged.

Differs by choice:

- **The mirror's plane clips** what lies behind it: in the BSP walk, and on
  every device by a clip distance, or without one -- on OpenGL ES (the Smart
  Pro's) or a Vulkan device lacking `shaderClipDistance` -- by a discard in
  the mirror's frame alone. The original keeps what lies behind a mirror out
  with a span buffer per zone.
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
a screen wider than 4:3 the budget takes the original's angle, the view's
before Hor+ widens it ([the view's width](#the-views-width)), so a mesh
keeps the original's detail at the same distance: at 1920x1080, the
original's frame scaled onto the middle of DXEngine's, Paul's shape at
0.95 at both, where the wider angle cut him to 0.77. An environment-mapped face reflects
the eye's line in its normals in the world's axes, as the original's
([mesh textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-textures)):
`MeshConsole` with `Enviro`, the four drawn so, has their streaks where
`D3DDrv` has them. A face's texture lies as the original's, its corners'
bytes ÷ 256 of the texture's size: `FaceConsole`'s UNATCO troop at a
correlation of 0.997 to `D3DDrv`'s frame, its chest 0.999.
A budget past an int's range (a script's `LODBias` or `DrawScale`) is held
to the mesh's vertices. A plain mesh's face turned away is culled unless it
is two-sided, as the original's `DrawMesh` culls it whatever `bMeshCurvy`,
and its weapon triangle is not drawn
([which faces are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#meshes)):
`MeshConsole` with `Plain`, `DXText`'s one-sided quad unseen from behind in
both engines, the frame there at a correlation of 0.990 to `D3DDrv`'s
(0.112 while DXEngine drew the quad's back), and its front alike, 1.000;
`IonStormLogo` drawn translucent showing its near side alone, 0.998 (0.975,
1.37 times as bright, while DXEngine drew its far side too). Five of the
game's meshes are plain: the menu maps' logos and text, and the fly.

A LOD mesh's vertex normals are made at each draw, as the original's
`DrawLodMesh` makes them: each the sum of the unit normals of the faces
within the budget that use it, as they lie that draw, so that at reduced
detail the shading follows the coarse faces, the vertex moved out along it
by the fatness: `MeshConsole` with `LODBias=0.004` and `NoDetail`, the
frame at an RMS of 3.55 grey levels to `D3DDrv`'s, Paul Denton's figure
6.19 (4.07 and 6.75 while DXEngine's were the mesh's frames' own, from
every face). A corner's U and V are its bytes × the texture's size at its
scale ÷ 256, which the render device divides by again, so a skin's
`DrawScale` moves nothing; a held item's budget goes by its
`ThirdPersonScale`, which the original swaps in for its `DrawScale`; a
plain mesh's pawn holds what it holds at its weapon triangle; a
`bParticles` mesh draws each vertex of its budget in view as a tile of its
`Texture` (with `bRandomFrame` a `MultiSkins` entry by the vertex's place,
at the frame its life has reached), lit by its glow -- none of it stock
content's ([meshes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#meshes)).
By choice, a detail budget past an int's range is held to the mesh,
where the original's makes it the least int and hands on a negative count,
and a hand-made mesh's corner walk and wedges are held to its arrays, where
the original's would loop or read past them.

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
turns). A light with `bDynamicLight` is a moving one before all else,
lighting a surface with no shadow and left out with `NoDynamicLights`, as
the original's `AddLight` takes it ([light
maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps));
the stock maps' few, on 02_NYC_Street, 02_NYC_Warehouse,
06_HongKong_WanChai_Market (`HKHangingLantern35` to `37`),
08_NYC_Underground, 10_Paris_Chateau (`Lamp4`), 15_Area51_Final
(`Light809`, `Light221`, `Light810`, static and no-delete), 99_Endgame1
(`ElectricityEmitter11`) and DXMP_Cathedral (`Light53`), light a mesh as
moving ones, full from the first frame they reach it. An `LE_OmniBumpMap`
light lights no surface:
given no owner, as a surface's lights are, the original's `AddLight` makes
it a kind of its own, which no light map takes. It lights a mesh, and a
mover with `bDynamicLightMover` as a moving light over it, which the
original gives the mover for its owner. No stock light is one;
`LightConsole` with `Effect=15 Spawn=64`, a lamp of it made 64 units over
Liberty Island's dock: the dock unlit by it in both engines, DXEngine's
frame at 1.006 of D3DDrv's, where the lamp lit it to 1.608. Each light
lights the texels of its rectangle on the map alone, as the original's
`SetupLight` gives it ([light
maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)):
those within |axis| × √(max(0, 1.05 r² − h²)) of its foot on the surface's
plane, h its height over it, each edge rounded to the nearest texel and the
last left out; so a light 1.025 r or more off the plane lights none of the
map, a cylinder light high over a surface, its shape having no height,
lights a square, and a non-incidence or shell light lights none of its far
band past the last edge. `LightConsole` with `Effect=17 Spawn=300 View=600`,
a cylinder lamp 300 units over Liberty Island's dock, at 1024x768: the
square in both engines, at 1.005 of D3DDrv's frame, where a disc of the
light's radius came to 1.345. A mover's polygons are lit where the mover is,
as the original's brush tracker files them into the level ([the movers in
the
BSP](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
its static lights through the shadows raytraced at its `BrushRaytraceKey`,
the moving ones over it there, its map built again when it moves or turns,
each fragment the tracker filed with the ambient light of its node's zone
([draw order and movers](#draw-order-and-movers)). `MoverLightConsole` at
1024x768, Liberty Island's `DeusExMover10` moved 100 units off its place:
the door at 1.003 of the original's frame under its own lights and 1.000
with a lamp 48 units before it, where it was 0.82 and 0.43, lit as if still
at its key. A spotlight's cone is the original's, its cosine `1 − LightCone
÷ 256`, its value rounded down, about the light's rotation, a pawn's about
its view rotation (`LightConsole` with `PawnSpot`, the player's own light a
spotlight over Liberty Island's dock, its view straight down and its
rotation level: the pool under it in both engines, at 1.005 of D3DDrv's
frame, where the light's own rotation aimed it, 0.973), and a light or fog
map lights texel i at its centre, `Pan.X + i × UScale` along U, where both
engines draw it ([a texel's
place](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)):
`LightConsole` has 03_NYC_Airfield's `Spotlight7` and Liberty Island's
`Spotlight0` at 1.00 of `D3DDrv`'s frames, with DXEngine's detail textures
off as the original draws none (`NoDetail`), in blocks of 16 pixels within
0.98 to 1.03 across `Spotlight7`'s wall and `Spotlight0`'s world map, inside
its border ([brightness](#brightness)). A surface's light map holds the
ambient of the zone it is seen from, and pans at that zone's speeds, as the
original's draw list has it: 06_HongKong_WanChai_Underworld's floor under
its disco lights `Light2` and `Light21`, lit by its zone's ambient and one
steady light, comes out at 0.99 of `D3DDrv`'s frames. A surface on a plane's
other coplanar nodes, which may face the other way, takes the zone on its
own side, as the original's: the wall left of that map's start takes its own
zone's ambient, not the LevelInfo's of the first node's side
(`OpeningConsole`). Its fog map is drawn only with the render device's
`VolumetricLighting` on and the viewport's actor in a fog zone, for a
surface not translucent whose zone is a fog zone too, as the original's
renderer has it: 02_NYC_Underground's floor under the water below
`Light33`'s grating, seen from the fog zone above, is unfogged in both, the
frame within 1.02 of `D3DDrv`'s (`LightConsole`). The other effects' shapes
are Render.dll's own, on the level's time. `LightConsole`, shooting both
engines at one moment of it (`ShotAt`), has 06_HongKong_WanChai_Canal's
disco `Light343`, 02_NYC_BatteryPark's cylinder `Light81` and
09_NYC_ShipBelow's slow wave `Light16` within 0.99 to 1.01 of `D3DDrv`'s
frames, correlated at up to 0.997, 0.998 and 0.98 (`NoDetail`). The rest,
whose stock views come out the same under other shapes (the searchlight,
non-incidence, the rotor) or which no stock light shows (fast wave, shock,
shell, interference), with `Spawn`: a lamp of each made 64 units over
Liberty Island's dock, the dock alone in the view, `D3DDrv`'s frames
correlated with DXEngine's at 0.993 (the searchlight) to 0.999, RMSE under
1. `D3DDrv` holds that lamp's map at the frame it was first lit, 6.02 s into
the level, a defect of its cache
([textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures))
that DXEngine does not carry, by choice, the map changing as `OpenGLDrv`
shows it; so DXEngine's is shot a whole number of each shape's periods on
(`ShotAt`); through `OpenGLDrv`, which shows them changing, each at one
moment of both at 0.96 to 0.98, as its frame of non-incidence, which holds
still, is. DXEngine's own, by choice where no ID is named:

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
  - `LE_CloudCast`, the plain shape, built once, where the original re-runs
    it every frame (its table counts the shape as changing): the same light.
- **Meshes**: the moving lights of an actor the scene draws come from
  DXEngine's light tree near the actor, not the leaf's own list.
- **The blink, the strobe, the flicker and the torch and fire wavers** at
  the original's pace at 60 frames a second, by choice, whatever the frame
  rate, where the original's change with each frame it draws, paused too
  ([lighting](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#lighting)):
  a step each 60th of a second of the real time each frame is begun at,
  whatever the level's `TimeDilation` or pause, the frame nearest each 60th
  taking it, at most one a frame, so that at 120 frames each state holds two
  and under 60 every frame steps, as the original's. At each step a blink
  takes the lowest bit of its turn count at the level's time then, out when
  it is odd; a strobe turns over, every strobe together; a flickering light
  draws afresh, out below one half; and the wavers take 256 fresh draws.
  Two more by choice: the blink's count is in doubles, where the original's
  render works it out in floats (its x87 at 24 bits), so that from 2^24,
  some 7.3 s × (`LightPeriod` + 1) into the level, its count is always even
  and its light stays lit; and each flickering light has a draw of its own,
  where the original's take the frame's 256 draws by the low byte of their
  addresses, so that two can flicker as one. `LightConsole` with `Burst`, a
  lamp of each made over Liberty Island's dock, sampled frame by frame
  (`DXCAP_BURST`), both engines at 60 frames: a strobe turned over at every
  frame in both; a flicker's state changed at 0.51 of the original's frames
  and 0.48 of DXEngine's, its brightness at 0.72 and 0.68; the torch's and
  the fire's waver at 0.95 and 0.94 of the original's, 0.94 and 0.94 of
  DXEngine's; a blink of period 32 the state its count gives in floats at
  0.998 of the original's frames, in doubles at 0.998 of DXEngine's; one of
  period 1 lit for good from 14.62 s in the original, blinking on to 24 s in
  DXEngine. At 120 frames DXEngine's states each held two frames, every run
  of an even length.

A mesh's lights are picked and faded as the original's
([meshes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#meshes)):
in one pass, strongest first, a static light lit while fewer than 8 static
ones are and any other while fewer than 8 in all are, none under an eighth
of the first one lit, a light off or of no brightness among them though it
lights nothing, and a cylinder light at three quarters of its brightness
and radius, its reach the whole radius; on the actor's first draw each
light its shadow line finds open lit full at once, one of the leaf's
moving lights full when it first reaches the actor, whatever its own flags,
one of its static lights dark until its line is next run; each fade a byte
stepped by the viewport's time since its last frame. `LightPickConsole`,
both engines at 60 frames, sampled frame by frame (`DXCAP_BURST`): a white
lamp switched off over a crate beside a red one under an eighth of its
strength, the red one's sawtooth on the crate -- out of the pass, fading
out, back at 255 as a moving light new to it -- going on in both, some 22
frames a tooth (`Scene=Off`); a cylinder lamp moved from 110 to 190 units
off, past three quarters of its 225, its light fading out and back at the
same frames in both (`Scene=Cylinder`); a lamp made over it, not movable,
the crate lit full from its first frame in both (`Scene=Late`). An actor drawn on
its own (`Canvas.DrawActor`, `Canvas.DrawClippedActor`, `GC.DrawActor`), a
first-person weapon among them, takes no moving light, only the static
lights and those it kept; an actor's own light counts in the pass, so that
a light under an eighth of its strength lights nothing, but lights nothing
of it. A vertex is lit by its normal alone, whatever its faces' flags, as
the original's `Light` (`Render.dll` `0x10b027f0`) lights each once: a
two-sided face shows on both sides the light its normal gets. An unlit
face is lit at the unlit level of the mesh drawn, its `AmbientGlow` / 256 +
`ScaleGlow` / 2, a held item's its own though the pawn lights the rest of
it, and as the original's face draw it writes that level into its corners'
light (`0x10b0bab0`), so a lit face drawn after it, in either pass, takes
the level at the corners they share: `HeldGlowConsole`'s muzzle flash at a
peak of 226 before and after its holder is cloaked in the original and 224
in DXEngine; `MeshConsole` with the LAM (`Extra=DeusEx.WeaponLAM`,
`ExtraAhead=40`, `ExtraScale=3`, `NoDetail=True`) at 1024x768, over the LAM
an RMSE of 3.9 grey levels to `D3DDrv`'s frame and a correlation of 0.99 in
both shots.
`ViewConsole` with `FovAngle=90`, `HoldAnim=Idle1`, `HoldFrame=0.875` and
`Floor`, both engines at 1024x768: the Dragon's Tooth's hand correlated
at 0.973 with the original's frame, and the hilt of the one on the floor
at 0.897; `CrosshairConsole`'s pistol in hand at 0.981.
`MeshConsole` with `Hold` and `Extra=DeusEx.Plant1`: the plant's leaves
at 0.997 of the original's brightness; Paul Denton at a median 1.01;
`HeldConsole`'s JC from behind, his coat at 1.009. The
`AmbientGlow`
pulse runs on the viewport's clock, the real time, whatever the level's
`TimeDilation`. `GlowConsole`'s crate, spawned 3 s into Liberty Island:
with no glow, 0.171 of the screen's white in both engines, at full speed
and at a hundredth of it; with the pulse at a hundredth, twelve shots 1.5 s
apart run 0.18 to 0.26 in both. A mesh in a fog zone is drawn unfogged, as
`D3DDrv` draws it with the game's `UseVertexFog` off
([fog](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#fog)):
`FogConsole`'s crate 220 units into 02_NYC_Underground's fog at 0.089 of the
floor and walls beside it, the original's 0.087.

With the client's `NoLighting` on, or the viewer's `RendMap` off
`REN_DynLight` (5), no surface has a light or fog map and every mesh is
drawn unlit, its lights kept as they were, as the original's `DrawFrame`,
`SetupForActor` and an actor's polygon flags have it; a viewer who is no
admin, on a level with `bNoCheating`, has its `RendMap` put back and
`NoLighting` turned off before each frame, as the original's `DrawWorld`
does ([light
maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)).
`LightConsole` with `NoLighting` on Liberty Island: Spotlight0's
world map unlit in both engines, at 1.000 of D3DDrv's frame, where DXEngine
lit it, 0.395.

By choice, a mesh's light fades step once a
frame, where the original's step at each draw of the mesh, an armed pawn's
twice. `RMODE` sets the viewer's `RendMap`, 1 to 19, as the original's
viewport does, so the credits' Matrix egg, `RMODE 6`, draws the world
full-bright until the level's next player (`ViewWordsConsole` on Liberty
Island: `RendMap` 6 after `RMODE 6` and the egg's world all its texture at
full brightness, in both engines).
A fog map is made each frame from its volumetric lights as they are then,
as the original's light manager sets each up
([the fog map](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)):
the level's own, and a moving light in a fog zone with a `LightRadius`, a
`VolumeRadius` and a `VolumeBrightness`, each in its sphere of
(`VolumeRadius` + 1) × 25 units where it is, its colour the light maps'
record of it, its density `VolumeBrightness` × its brightness now / 64; a
texel 255 × its density along the line of sight × the colour, its alpha the
density × `VolumeFog`, each held to 127 and drawn at 2c/255, the lights
laid over each other farthest along the view first, each texel its own
plus (127 − its alpha)/127 of what lies under it. `FogTintConsole` in
02_NYC_Bar, `Light90` turned to brightness 255 and then red: the fog
bright, then red, in both engines, the fog alone at 1.01 to 1.02 of the
original's channel by channel, the faint first shot's at 1.2 to 1.4
(`Baseline`, `fogdelta.py`), where the build before drew none once the
baseline had zeroed it. By choice,
the fog map is held in floats, where the original's has seven bits a
texel, drawn at 2c/255, and its table's steps whole numbers.

### Brightness

Matches the original's picture
([gamma](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#gamma),
[the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness),
[textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)):
the Brightness slider raises the picture to 1 / (2.5 × Brightness), as
`D3DDrv` sets its ramp: the ini's value as read, a `SET`'s as set, then
held to 0 to 1 by any `SET` of the client, as the original's client is
(`UClient::PostEditChange`), `GET` and the ini keeping the value set
(`ChangeConsole`: `Brightness` set to 5, `GET` 5, in both engines); and `Settings.json`'s other gamma mode (XOpenGL's
curve) takes the same product, its `Contrast`, `Saturation` and gamma
offsets leaving the picture so at their defaults
([the keys](ENGINE.md#settings-and-environment)). The shaders take nothing
off a texture's colours (`NO_DARKCLAMP`), as `D3DDrv` draws 32-bit textures
on any display of 24 bits or more, on every device: `MeshConsole`'s shots
under Vulkan, with descriptor indexing and without (the Smart Pro's), are
within 0.2% of the GL device's. How a DXEngine shot meets the original's
frames: [scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines).
Differs, by choice, the owner judging it close enough, its cause not found:

- `MeshConsole`'s Liberty Island pier, where nothing is in hand, is 1.7%
  under `D3DDrv`'s frames with the detail textures on, DXEngine drawing the
  pier's detail texture, the original none
  ([the menus' settings](#the-menus-settings)), and 0.6% over with them off
  in both (`NoDetail`), a quarter of a level on its dark floor: as much in
  DXEngine's window grabbed as the original's is, so not the shots' gamma,
  and alike at mip biases of 0 to 2 and with the light maps sent as bytes.
  Its crate, box and barrel are within 0.6%, Paul Denton held at one pose
  (`Hold`) at a median 1.01 of the original's frame, correlated at 0.984,
  and `LaserConsole`'s corridor 2% under, within 0.5% with them off.

A `Brightness` outside 0 to 1 in the ini is held to 0 to 1 as the client
starts, and the window's first resize saves the held value, which `GET`
then answers, as the original's client holds it in `UWindowsClient::Init`
and its viewport's resize saves it (`FallbackConsole` with `Brightness=5`:
`GET` 1.000000 in both engines).

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

With Bloom on (DXEngine's setting), the world's bloom is added into its frame
before the windows are drawn, so the screen flash -- the loading screen's
dimming, the pain and water tints -- scales it as it scales the world, and the
windows and the menus' mask (`MaskBackground`) cover it, on the Vulkan, GL
and D3D11 devices alike (`LoadingConsole` with Bloom on, `bloomlevels.py`:
UNATCO HQ's SAVING frame at a 99.9th percentile of 30, 292 pixels over 40,
as with Bloom off, where 96 and 16,443 on the build before; the island's
LOADING frame 29, as with Bloom off, where 34; alike on Vulkan, GL, GL ES
and D3D11 under Wine).

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
original's 8.5. A mover's polygon is drawn a step toward the viewer in the
depth buffer, so one flush with the level's surfaces shows over them; one
the brush tracker filed in the plane of a surface of the level's own,
which hangs after that surface in its plane's chain, is drawn a step away,
so the level's surface shows over it where it covers it, as in the
original's frame. `IntroDoorConsole`: the
trap door under Bob Page and Walton Simons on 00_Intro, its top in the
marble floor's plane, under the floor's lit marble as in the original's
frame (the panel round their feet correlated at 0.91 with it, on Vulkan,
GL and the software device). A mover's polygon takes the zones of the
fragments the brush tracker last filed it into, as the original's walk
draws them: each fragment its node's zone on the side of its plane the eye
lies -- its parent's on its side, or, hung in a node's plane, that plane's
on each side --, lit by that zone's ambient light, panned at its speeds
and fogged by it
([light maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)),
and one the tracker dropped whole, in solid, is not drawn. So a mover whose
place lies in another zone than its faces, or a door across two zones, is
lit by the zone of each part: `GlassConsole` with `GlassName=DeusExMover35`
on 06_HongKong_WanChai_Street, a door whose faces lie in a zone of ambient
20 and its place in the LevelInfo's of 0, at 1024x768 the frame at 1.009
of the original's and from the other side 1.001, where it was 0.931 and
0.912; with `GlassName=DeusExMover2` on 12_Vandenberg_Cmd, a door across
its place's zone of ambient 3 and one of 0, the face in the zone of 0 as
dark as the original's (0.995 of its frame, where the zone of 3 lit it 81
times as bright), the other face as it was (0.948); `BreakableGlass8` on
03_NYC_Airfield, in one zone, drawn alike before and after. A mover's
polygon takes its texture's own
flags with its own, as the original's walk takes the surface the brush
tracker files for it, its back-face test on the polygon's alone: a grate
whose texture alone is masked (`CoreTexMetal.Metal.metalgrate_a`, the five
under 02_NYC_BatteryPark's Castle Clinton; 93 movers of 29 stock maps
draw such a polygon) shows what lies behind it (`GlassConsole` with
`GlassName=DeusExMover31`: a crate and a sprite behind the grate correlated
at 0.96 with the original's frame on Vulkan, GL and the software device). A
one-sided surface of the
level's own seen from behind is passed over by its own flags, its
texture's taken only after, as the original's walk does: a texture made
two-sided shows none from behind (`BackFaceConsole`: Liberty Island's
chain-link sheet over the statue's pedestal room, its texture
`CoreTexMetal.Metal.ClenChainlink_B` given `bTwoSided`, seen from behind
as with it one-sided in both engines). Neither engine draws one the
eye is within a unit behind, which the original's walk passes on where
it passes over one farther (the sheet and a door's face from 0.9 units
behind, correlated at 0.987 and 0.993 with the original's frames on
Vulkan). After the world the depth
buffer is cleared and the player's overlays drawn
([the overlays](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#the-overlays)),
so the first-person weapon meets no depth of the world: an assault rifle
held flush against a wall of UNATCO HQ draws whole, within 1.3% of the
original's frame (`WallConsole`). Differs (`VisibleFrame::Draw`,
`VisibleMesh`, `VisibleBrush`):

- **Back to front** by the distance of one point of each, where the
  original's lists run in the order its BSP walk met them, by choice.
- **The opaque surfaces** go by zone, the material's address and the light
  map, a stable sort over DXEngine's walk, where the original's key adds
  the zone actor's, the texture's and its palette's object indices and its
  sort keeps no order among equal keys
  ([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)):
  where two of them overlap at one depth, which shows can differ, as the
  indices the original's loads and frees gave its objects decide there (the
  gaps between the Liberty Island pier's planks, a few hundred pixels of
  `MeshConsole`'s frame), by choice: no run can give the original's
  object indices.
- **A mesh's translucent faces** are drawn after its opaque ones, back to
  front with the scene's translucent parts, where the original draws a
  mesh's faces in their order as its walk meets the mesh: by choice, the
  same picture, but for a mesh with an opaque face after a translucent
  one, whose translucent face the original loses where the later face
  behind it writes over it. Of the 61 meshes with translucent faces on 13
  maps counted, `GM_Jumpsuit`, `GM_Suit`, `GFM_TShirtPants` (and their
  carcasses') and `ComputerPublic` alone have one; they are drawn whole in
  face order, with the solid actors, so a translucent surface behind one is
  drawn after its translucent faces, by choice: sorted among the translucent
  parts by one point's distance, a troop in water could be drawn over the
  water. `FaceConsole`'s UNATCO troop face on and turned 60 degrees, an MIB
  face on and Anna Navarre from 40 units above come out the same either
  way, as `D3DDrv`'s, the MIB's lenses 0.176 of white in both engines.
- **Movers** are drawn as their brushes, through their own matrix, a step
  toward the viewer, a polygon filed in a level surface's plane a step away
  ([rendering](ENGINE.md#rendering)): their opaque
  polygons with the solid actors, their translucent ones among the
  surfaces. The original draws each fragment the brush tracker filed of a
  polygon as one of the level's surfaces, one in a wall's plane filed in
  front of the wall's. DXEngine draws a polygon whose fragments lie in one
  zone whole, by choice: the same light, without the seams the BSP's
  splits can leave between fragments (of the stock maps' 34,712 visible mover
  polygons, 11,009 are split at their load, 5,451 into three fragments or
  more, and 160 across zones, drawn fragment by fragment, each a step
  toward the viewer or, filed in a level surface's plane, away). So one
  flush with a wall shows over it in both, by the step
  in DXEngine: `FlushConsole`'s pocket door (Liberty Island's
  `DeusExMover12` slid half into the flat wall beside it) shows the door,
  head on and from 1,200 units at 75 degrees. No device here fights there
  without the step (GL and Vulkan on the desktop, Vulkan on the Smart Pro:
  the shots within 15 pixels with it and without); the step keeps the
  order whatever a GPU's rounding, by choice. A mover's one-sided polygon seen from
  behind is passed over, as the original's walk passes over a surface. A
  mover is drawn and lit where the level's BSP last filed it ([the movers in
  a line's way](#implemented-not-as-the-original)), whatever its flags, as
  the original's walk draws the polygons its tracker filed: hidden, it stays
  drawn where it was, its mirror too (`GlassConsole` with
  05_NYC_UNATCOMJ12lab's `BreakableGlass16` hidden, and with `Flip` the mirror
  of 06_HongKong_WanChai_Underworld's `BreakableGlass0`: each engine's frames
  as with it shown). No stock script hides a mover, and no stock map holds one
  hidden, nor passed over otherwise, at its load. A destroyed one is drawn
  no more, by choice, where the original's stays drawn until the next load
  or save, its drawing reading the destroyed actor, a freed one once 128
  destroyed actors are cleaned up; the game's movers are `bNoDelete`, and
  refuse (`GlassConsole` with `Destroy`, in both engines). A mover's brush
  is placed as the original's brush coordinates place it, its `PostScale`
  after the rotation along the world's axes, in its drawing, its light, its
  box and its filing ([the movers in a line's
  way](#implemented-not-as-the-original)): `PostScaleConsole`'s boxes the
  original's, WanChai Market's locked door X 268 to 500 and Z 44 to 132,
  DXMP_CMD's bot doors 6 by 80 by 80, but for a float step in three of them
  (bugs.md PHYS.37).

A decal is laid as the original's `AttachDecal` lays one
([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-natives)):
its line meets the movers' polygons where the brush tracker filed them,
passing a masked, invisible or portal polygon, only an auto-panned surface
refusing it; a square `USize` times `DrawScale` on a side, turned by
`DecalDir` in the surface's plane, or by a direction drawn from within the
unit sphere where that is zero, the texture's axes the original's; on every
node of its surface it covers, wherever they are in the BSP, and on the
surfaces facing within 45 degrees or so that a grid of `MultiDecalLevel`
squared lines meets (the line and the direction by reading). On a mover's
face it keeps the mover's rotation and is based on its owner
(`MoverDecalConsole`: a `BulletHole` on Liberty Island's `DeusExMover10`
kept and based on the door, at the same pixels in both engines, where the
build before refused it). It is drawn 0.75 units off its surface along the
surface's normal, clipped by the planes through the eye and each edge of
its surface's polygon, so that on screen it fits the polygon's outline
(`DecalLiftConsole`: the scorch mark where the original's lies, from 37
degrees above and turned 25 degrees, where the build before's lay 4 to 5
pixels lower and 2 to 4 left); on a mover's face clipped to the polygon
whole, as DXEngine draws it. A save's new tracker makes the movers'
surfaces anew without their decals, as the original's does. By choice, a
decal the grid lays on a mover's face keeps that mover's rotation, where the
original's keeps none and is drawn turned by the mover's whole rotation; and
a decal on a mover's face stays drawn on it as the mover moves, its actor
carried with it, where the
original's is drawn there no more once the door has turned, its actor left
where it was laid and its base None (`MoverDecalConsole`, the door
opened). Differs (bugs.md REN.71): a mover's face turned from where it was
built is drawn darker than the original's (the same door opened: its face
0.039 of white in both builds against the original's 0.092, alike shut).
Differs (bugs.md PHYS.37): a line's hit on a mover's
filed polygon can land a float step off the original's, the trace's own
float order against the plane not the original's, and a mover's box at a
`PostScale` a float step off (`FiledPlaneConsole`: Battery Park's MainDoors
open at yaw 10240, the edge's hits at -367.044403 where the original's are
at -367.044434, the face's at -307.592438 where -307.592468).

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
  most 60 times a second, by choice, the original's pace at 60 frames. A
  texture steps by the time since its own last update, however often it is
  drawn, as the original's: one with a `MaxFrameRate` drawn every other
  frame steps as often as one drawn every frame, and one drawn every ninth
  frame at 60 steps at every draw (`StepConsole`). That time is the real
  time each frame is begun at, as the original's viewport hands every
  texture it draws, whatever the level's `TimeDilation`, counted from when
  the texture was made or loaded: one first drawn long after its load
  steps once at its first draw, as the original's (`CoronaSkinConsole`'s
  chain, its frames alike in both engines).
- **The wave lighting table** can be one step off at a single entry, in 3 of
  200 settings: DXEngine takes the C library's cosine, the original the
  x87's, and they differ in the last bit; by choice, a port computing as
  its C library does.

The shared sine table is the original's, its entries rounded to the
nearest (read: Fire.dll `0x1050106a`): `SineTableConsole`'s
`Electricity.Xplsn_EMPG`, its eleven WhirlyThing drops drawing no random
bytes, after 100 steps matches the texture stepped with the table rounded
at all of the 7,781 texels where the two tables part (`sinecheck.py`),
where the build before matched the truncated one at all of them; the
original's frames match neither's 100 steps. A fire's colour table is made as it loads whatever its
`RenderHeat`, so that one of 0 cools 15 levels a step, as the original's:
`FireHeatConsole`'s `SparkFX1` burns yellow and `Spark_Electric` and
`Wepn_EMPG_SFX` blue in both engines, each tile at 0.94 to 1.05 of
`D3DDrv`'s brightness, where the build before drew them at 0.39 to 0.64. A fire's `SparksLimit` is held to 4 to 8,192 only where more
sparks were saved than it allows, so that one of 0 leaves the fire no
sparks, its pass going on; a save writes the placed sparks in the order
the original's leaves them and drops the ones they let go from the fire
itself; an ice texture's place is rounded to the nearest pixel, its pans
worked out as the original's, at 24 bits in the frame that draws it. By
reading: no stock texture reaches them.

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
(`FinishConsole`). Its poll comes after the tick's animation, `AnimEnd` and
`Tick`, and lets the wait go once `bAnimFinished` is set: an `AnimEnd` that
starts another animation has it wait for that one too, and a loop started
while it waits finishes at its pass over `AnimLast`, `bAnimFinished` set
for the `AnimEnd` and the code after (`FinishWrapConsole`). A sequence
name the mesh lacks is none: `HasAnim` false, `GetAnimGroup` None, and
`PlayAnim`, `LoopAnim`, `TweenAnim` and `PlayBlendAnim` of it play nothing,
as the original's `GetAnimSeq` answers
(`AnimNameConsole`); an actor whose `AnimSequence` the mesh lacks -- None,
most decorations' -- is drawn at the mesh's first frame, as the original's
`GetFrame` draws it. `PlayAnim`, `LoopAnim` and `TweenAnim` start as the
original's do: with no tween time given, a frame's tween at the last
animation's rate (in 0.025 s a frame after a tween or nothing playing, at
the velocity's rate after a velocity-scaled one), none from nothing playing;
a frame over the time given; none with 0, the frame set a shade past 0; a
one-frame sequence tweened whole over the time, or in 0.1 s; and the same
sequence looping on takes its new rate alone (`TweenConsole`).

`PlayBlendAnim` and `TweenBlendAnim` set a slot as the original's do: a
one-frame sequence -- every head turn and mouth shape -- tweened whole from
-1, over the time given or in 0.1 s, so a head turns where it would snap; a
slot's first sequence untweened at 0.001; a tween time of -1 at the slot's
last rate; `SimBlendAnim` packed as the original packs it, the same
animation again marked new (`BlendFieldsConsole`: every field after each
call alike in both engines). The slots move after the main animation, with
the passes and the time it leaves them: three passes of a tick its step
ends in its first, none on a tick that stopped it, spent its four passes,
or whose loop ended its step between `AnimLast` and 1.0 (`BlendFieldsConsole`'s
stage e: a head turn 0.4 s still while Paul Denton's breathing runs from
`AnimLast` to its wrap, at 0 some 1.41 s in, in both engines).

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
[border windows](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#border-windows),
[drawing](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing),
[small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)).
The consoles: `GetConsole`, `BeltConsole`, `BorderConsole`, `ColorsConsole`;
`ChoiceConsole` for a conversation's choices, by the keys and the pointer;
`ThemeConsole` for the InfoLink's text in the HUD theme; `SizeConsole` for
a new window's size; `ScrollConsole` for a scroll area and a slider;
`ClickConsole` for a button, a choice and a checkbox clicked, a menu dragged
by its title bar and Alt with a letter; `EditConsole` for a script's text in
an edit field, a text window and a button; `MultiEditConsole` for an edit
field of several lines; `ToolConsole` for a cheat screen's tool window.
`MenuConsole` and `ScreenConsole`
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
- **Places**, laid out at once as a window asks -- made, shown, moved,
  sized or aligned --, its parent asked, and a parent that does not take
  the request asking its own: every native container but the root passes
  it on unasked, a tab group, a modal, a clip window, a scroll area, a
  tile among them. `SetPos` makes a window left- and top-aligned with its
  x and y as margins; `SetWindowAlignments` sets both margins of each
  side, the second the first's unless given. A parent's `ConfigureChild`
  sets just what it gives, to a full-aligned window too (`ScreenConsole`:
  a tile in a clip window at -0, -0, as the clip places it), and leaves a
  window that does not show as it is, counted placed (`TitleConsole`: the
  HUD's mission start text 10 by 10 at 0, 0 until a message shows it); a
  window no parent places, or that the base script's answer to its asking
  hands back (`ResizeChild`), sits by its alignment at its preferred size
  -- centred with the half dropped, plus its margin -- and its `x` and `y`
  are that place, from the moment it asks (`SizeConsole`: a new window's
  place and size as made). A parent laid out again counts its left- and
  top-aligned children placed, and each other one its layout leaves out
  goes back to its alignment (`LayoutConsole`: a window's second child
  moved by `SetPos` placed again by its parent at once, its first, which
  the parent's next layout leaves out, centred at once). So a menu dragged
  by its title bar follows the pointer and the credits start below the
  screen (`ClickConsole`, `MenuConsole`).
- **Sizes**: a side a window was given (`SetSize`) counts as asked for; a
  side it leaves unset is its background's size, else its own -- an empty
  text window is 0 wide and keeps its height. A size set is held at 0 or
  more and lays the window out again, its parent asked, only when it
  changes, as the original's `Resize` has it; `ResetSize` lays it out again
  only when a size was set. A large text window and an
  edit field measure rows of the font's height with their vertical spacing
  between (`ScreenConsole`'s skill text and note), an edit field a space
  wider when no width is given. A window asking to be laid out again is
  laid out itself too. A large text window draws its rows as it measures
  them, the block of them at the top margin, above the bottom one, or
  centred by the whole half of the room, margins and all, each row at its
  own alignment across (`TextBlockConsole`: two lines in boxes aligned to
  the top, the middle and the bottom, their lit rows and columns alike in
  both engines); an edit field draws its own rows, as the original's. A
  large text window's script `DrawWindow` comes first, handed its GC left
  and top aligned and word wrapped, the window's own alignments and wrap
  placing the rows alone (`UndoConsole`'s `CapLargeTextGCWindow`: 0, 0 and
  True in both engines), each row the font's height below the last
  whatever its characters (read from both codes; no stock font has a glyph
  taller than its space). An edit field whose GC's drawing is off draws its
  insertion point and none of its rows (`EditDrawOffConsole`: the white bar
  alone in both engines).
- **A border window** gives its top shown child the inside: within the
  child margins, and within the border's textures while its margins are
  from the border, as they are from the start. It asks for that child's
  size with those margins, no less than the border, and draws its nine
  textures in its style, sending its script neither `ConfigurationChanged`
  nor `DrawWindow`. So the cheat screens' tool windows show their title
  bar, labels, list field and buttons (`ToolConsole`'s shot of the EditFlags
  screen, through the original's gamma: within a level of the original's
  but for the pointer). With resizing on (`EnableResizing`, which no script
  of the game turns on), a left press on its border drags the sides it is
  on, a right press all four, which moves the window; each move asks for
  the new place and size as `SetConfiguration` does, and the pointer shows
  the sides' cursor, the window's own or the root's default -- with no
  drag, the border's under it --, the script's elsewhere, as the original's
  code has it (`ResizeConsole` with `tools/dxcap/timelines/resize-drags.txt`,
  DXEngine alone: 200 by 150 made 250 by 180 by its corner, then moved 40
  by 20; the corner's diagonal cursor, the move one, the arrow inside).
- **Text from a script** (`EditConsole`): a text window takes no text that
  differs only in case. An edit field's `SetText` replaces its text as
  typing would -- its undo cleared, `TextChanged` announced up its
  parents -- and leaves the insertion point at the start, as `AppendText`
  does after adding at the end; so the save screen's Save Game button
  wakes for the name it fills in. An edit leaves the insertion point after
  what it put in, and what it put in selected when asked to. Undo and Redo
  put back what they take back or make again selected, so a key typed next
  replaces it; a field is changed while its undo steps stand off the mark
  `ClearTextChangedFlag` sets, so an undo back to the mark reads unchanged,
  `TextChanged` saying so, and a typing after the mark is a step of its
  own; `SetMaxUndos(0)` keeps no step (`UndoConsole`: every line alike).
- **An edit field's copy, cut and paste** go through the desktop's
  clipboard, by choice, so they reach other programs, where the original
  keeps a text buffer of its own: a copy or cut takes the selection and
  nothing with none, as there; the text's 16-bit characters go out as UTF-8,
  and a paste reads the clipboard's UTF-8 back as 16-bit characters, a
  carriage return left out. `PlayerPawn.PasteFromClipboard` and
  `CopyToClipboard` take the desktop's clipboard as UTF-8 so too
  (`PasteConsole`, the clipboard holding "dxcap paste é Ω €": its codes 233,
  937, 8364 alike with the original's, and the game's own copy of "dxcap
  copy é Ω €" pasted back whole in both). On X11 a paste asks for
  `UTF8_STRING`, and for `STRING`, Latin-1, only of a program that has no
  UTF-8; a paste of what the game copied takes its text as it is.
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
  the pointer and drags it, whole words on a second click. A character a
  fraction of a unit inside the field's edge is drawn whole on the unit
  before it, as the original's ([tiles](#tiles)).
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
- **The grabs** (`GrabConsole`): the root takes the keys while its count
  of the keyboard's grabs is not 0, and the mouse's buttons, axes and
  wheel while its count of the mouse's is not 0. A modal window with a
  parent raises both as it shows and lowers them as it hides, never below
  0: a message box over a menu counts 2, either hidden 1, a modal made
  hidden 0 until it shows. The root's `GrabKeyboardEvents` and
  `GrabMouseEvents` count for a script as well. The first grab of the
  keyboard clears the marks of the keys held, the first of the mouse those
  of its buttons. The counts alike in both engines at every step.
- **Keys** go up from the focus window to the root, so Esc and F1 close a
  screen; **mouse buttons**, only while the mouse is grabbed, to the window
  the mouse acts on (the grab, else the one under the pointer in the
  topmost modal, else that modal), then up its parents. A press grabs the mouse for
  that window, so a drag's moves and its drop reach the item it started on,
  and counts as a multiple click within 0.5 s and 10 units of the first.
  The root marks every press and release: a press of a key already down is
  a repeat, and a release of a key or button not down is taken while it is
  grabbed and goes to no window (`KeypadConsole` with `stray-release.txt`,
  DXEngine's alone; with no grab the game gets it).
  `IsKeyDown` reads those marks. Typed text reaches the console and the
  windows a character at a time, as the original's comes (a `WM_CHAR`
  each), a 16-bit character, one past 16 bits as its surrogate pair, the
  console's `KeyType` first with its low byte and, should the console
  leave it, the root window's focus window with the whole of it
  (`TypeConsole` with `type.txt`: "Café", "ñü", U+0100 and U+20AC typed
  whole into an edit field, 256 and 8364 in both engines, the original's
  typed by `xdotool`; U+1F600 two characters; and with a Windows build's
  Win32 window under Wine, `wincheck.exe char` typing e acute, the euro sign
  and U+1F600 as Windows types them: 233, 8364, 55357 and 56832 in both
  engines, where the build before took the pair as two U+FFFD). The text windows, the
  canvas, the GC, a computer's screen, a list's cells and a scripted
  texture take and draw text as 16-bit characters, a font's glyph found on
  its page; one past every page is drawn as nothing and takes no room
  (`TileConsole`: U+0100 and U+03A9 of `FontMenuHeaders` 0 wide on the
  canvas and in a window, `A`, U+0100, `A` as wide as two A's, in both
  engines).
  `LootConsole` searches two carcasses alike in both engines; the keys and
  the drags between the searches (`loot-drags.txt`) are a timeline's, so
  DXEngine's alone, as are `ScrollConsole`'s clicks, drags and wheel
  (`scroll.txt`) and `ClickConsole`'s clicks, drag and keys (`clicks.txt`).
- **The pointer**, while the mouse is grabbed: a move that puts another window
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
  again, draws none in either engine (`PointerConsole`), and a script's
  grab of the mouse with no modal up draws it in both (`GrabConsole`'s
  shots). The pointer drawn
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
  until the next starts typing (`ThemeConsole`). The window has the
  original's two children, a plain window that fills it, its `textWindow`,
  and a hidden edit field, its `inputWindow`, whose insertion point blinks
  as the cursor does; the cursor draws while that field is hidden, the
  text and the cursor being the computer window's own drawing, and its
  `player` is the root's (`ComputerTreeConsole`: the InfoLink's window and
  its children alike in both engines).
- **Accelerators**: a text's `|&` marks its window's key while the text is
  its accelerator -- every text window's from its start, but a large text
  window's. Alt and a key the scripts leave press, in the modal on top, the
  first window with that key -- the modal, then its children bottom to
  top, each in turn -- that shows, is sensitive and takes the focus, a
  letter in either case (`ClickConsole`: Alt+B opens Brightness from the
  Display screen, Alt+C cancels it). The letter is the character the layout
  gives the key, after the key, as the original's comes in a `WM_SYSCHAR`
  (read from both codes): the Win32 window's that message, the X11, Wayland
  and SDL2 ones' their text, the SDL3 one's the layout's character with
  Shift and Caps Lock, for which SDL sends none (`keys.txt`: Alt and B,
  Caps Lock on, give `B`).
- **Keyboard focus**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#keyboard-focus)):
  a tab group's windows in order of where they lie on screen; `MoveFocus`,
  `MoveTabGroup`, `SetFocusWindow` (only a window that can take the focus
  takes it, its modal keeping it to return to), the focus moving off a
  window hidden, made unselectable or destroyed, and the root seeding it in
  its own tick, before every other window's -- none while the keypad
  (`MFOCUS_EnterLeave`) is up, none with no modal up (`KeypadConsole`). So a
  conversation's first choice takes it at the windows' tick after the
  choices are made: the frame they are made is drawn with no focus, the next
  with the first choice's (`ChoiceConsole`'s `drawn` lines). Each window
  keeps the part of it its parents leave in view, as the original's clip
  rectangle, and a modal's tab groups go in the order of the first of their
  windows that shows, a group none of whose windows shows first
  (`FocusConsole`: a clip window scrolled so that its first button lies
  above the view, then none in view; Tab and Shift+Tab from each place, and
  each button's clip rectangle, every line alike). The arrow keys' moves go
  round a tab group's ends and stop at a clip window's, where the focus
  stays, as the inventory's ammo buttons and the medical bot's augmentation
  buttons have it (`ClipWrapConsole`: every line alike).
- **What a clip rectangle holds back**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing-a-window)):
  a window none of which shows -- scrolled out of its clip window, sized to
  nothing, past its parent's edge -- draws nothing, no background, no
  `DrawWindow` or `PostDrawWindow` event and none of its children; one
  that shows a part draws, each child by what of it shows. A list pages by
  the rows that fit the part of it in view, an edit field too, each draws
  only the rows that part reaches, and a list's sounds come from the middle
  of that part (`ClipDrawConsole`: a window's events counted as its clip
  window scrolls it in and out of view, a window of no height and one past
  its parent's edge; the page of a list in a clip window shorter than its
  parent, of one shorter than its parent and of one its clip window's
  parent cuts; every line alike).
- **A window's GC**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing-a-window)):
  a window draws through a GC of its own as its setters leave it: its
  background's style (`DSTY_None` turning drawing off) and smoothing, with
  a background texture or none, its fonts, colours and text settings, left
  and top aligned and word wrapped. So a button's texture takes the
  button's style, and the persona screens' translucent scroll arrows show
  the list through them (`BgStyleConsole` at 1024x768: a translucent button
  over a grey box at 161 and 201 through the original's gamma at its
  texture's colours 0 and 1 in both engines, the masked and normal ones at
  0 and 137). `EnableMasking`, `EnableTranslucency` and `EnableModulation`
  make the style again from the three, none while drawing is off, and
  `PostDrawWindow` takes the GC as `DrawWindow` left it, each child drawing
  through its own (`GCStateConsole`: every line alike); `DrawActor` with
  `bUnlit` or `ScaleGlow` left out draws the actor lit at a glow of 1, as
  the original's native passes them (read from both codes; every stock call
  gives both). The pointer is drawn through the root's GC, by choice never
  smoothed, where the original's is once the Snapshot
  option has turned the root's smoothing on (read from both codes). A
  window's background, stretched or tiled, is put on whole units
  as every tile is, as the original's `DrawIconPattern` puts it (read:
  Extension.dll `0x10028770`; `DrugConsole` full screen at 2560x1440: the
  drugged view's last column brighter than the four before it, where the
  build before stretched it evenly).
- **The windows** tick by real time since their last tick, not the level's,
  and a held button's first repeat counts what of the frame had gone. Each
  window, shown or not, from the root down, its higher siblings first, does
  its own work first -- a button's or a scale's repeat, a list's drag and
  hot keys, an edit field's blink and drag, a text log's expiry, a computer
  window's typing and `ComputerStart`, the root's first focus -- then its
  script's `Tick`, then its timers, then its children (`TickOrderConsole`:
  a computer window's, a button's and a text log's script `Tick`, timer and
  child, every line alike in both engines). A computer window shown is
  marked to start, and its next tick sends `ComputerStart` to it and each
  parent until one takes it, shown by then or not, paused or not; its first
  tick in a pause sets `bGamePaused` and `bPauseProcessing` and goes no
  further, the pause's end clearing both, and `bPauseProcessing` alone stops
  its typing (`ComputerStartConsole`). A text log's line lasts its whole
  time out from when it came, the frame already gone counted in
  (`LogTimeoutConsole`), as an edit field's first blink waits.
- **Timers**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#timers)):
  each is due its time out after it was added, the frame already gone
  counted in, and calls its function once a tick, the newest first, with
  its id, how many of its time outs have passed and its client data
  (`TimerConsole`'s logs alike, both engines at 60 frames a second); one
  with no function name calls the window's own, a button's ending its
  press's show, which is such a timer (`TickOrderConsole`).
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
  them, the places within 3 units of the original's, 56 of the 100 on the
  screen exactly
  ([the engine's own rotations](#implemented-not-as-the-original));
  its boxes round two decorations, the crosshair on their middle and turned
  5 degrees four ways, within 7 pixels of the original's at 1280x720 with
  the original's [view width](#the-views-width).

`Canvas.DrawActor` and `Canvas.DrawClippedActor` leave their actor hidden,
whatever it was, as the original's natives do, where `GC.DrawActor` puts the
flag back (`OverlayConsole`: the pawn drawn visible and drawn hidden, hidden
after each, in both engines); `DrawClippedActor` draws from an eye at its
own frame's origin. `GC.DrawActor` with `bConstrain` draws through the
scene's frame narrowed to the window's clip, its projection made again for
that size, as the original's
([actors in a window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#actors-in-a-window)):
`ConstrainConsole`'s crate, drawn from a window over the screen's top right
quarter, comes out at half its size about the quarter's middle in both
engines, the quarter within RMSE 6.5 of `D3DDrv`'s; no stock script
constrains. Its render time: [out of sight](#out-of-sight).

`Canvas.DrawActor`'s and `DrawClippedActor`'s `WireFrame`, which the game
passes false, draws the actor's mesh as the original draws it with the
player's `RendMap` at wire
([wire meshes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#wire-meshes)):
lines between the corners of each face within its detail budget and of its
special faces, whatever their material, in (0.6, 0.4, 0.1), green for a
selected actor, over the scene; a plain mesh's shared edges once; what a
pawn holds not drawn where the weapon triangle lies wholly past one side of
the view taken in the world's axes, as nearly anywhere. A line's colour goes
under the present's gamma with the frame, as `D3DDrv`'s goes through its
gamma ramp (`OverlayConsole` with `Wire`, a crate and a `JCDentonMale`
holding a pistol on Liberty Island: 99.7% of the original's line pixels
within a pixel of DXEngine's and 96% the other way, the lines (181, 138,
55) through the gamma against the original's (181, 138, 54), the pistol
undrawn in both, and drawn solid with `Solid`).

Differs:

- **Keys held as the window loses the keyboard** are forgotten, by choice,
  so an Alt held into Alt+Tab does not stick: the original's root keeps
  their marks, its window resetting only the engine's input (read from both
  codes), and its Alt stays down to the windows until pressed again, a key
  and a typed letter passing every window over but a modal.
- **The root window** is 4:3 (960 wide at 1280x720), where the original's is
  the whole viewport, by choice: the stock
  `ConWindowActive.CalculateWindowSizes` gives a cutscene's subtitles no
  height on a root 16:9 or wider, the Smart Pro's among them, so a cutscene
  letterboxes at 16:9. The root's children lie across the whole screen, as
  the original's, their `x` the screen's: the screen in units is the
  original's, the viewport over the scale with its fractions (853.33 by
  533.33 at 2560x1600), and the root a whole number of units in, so that a
  child at 0 lies on a whole unit and each tile the GC puts on whole units
  lies on the original's pixels at any scale and shape (`CrosshairConsole`:
  the crosshair and the accuracy marks on the original's pixels at
  2560x1440, a scale of 3 and the root 107 units in, at 1600x1001, the root
  500.5 units high, at 1366x768, the HUD 1366 units wide, and at
  1920x1080; at 2560x1080 the HUD's parts at the screen's edges, in the
  original's columns; `MenuConsole`'s and `ScreenConsole`'s window trees
  there alike with the original's but the version, the clock and the free
  space). On a screen narrower than 4:3 (1280x1024) the root is the
  screen's width, as the original's, so the HUD's left and right parts lie
  at its edges. The root's own background and its raw background cover the
  whole screen, as the original's root's do: the menus' darkening
  (`MaskBackground`) and the Snapshot option's picture reach its sides,
  the picture filtered, as `ShowSnapshot`'s `SetBackgroundSmoothing` asks
  of the root's GC, its edges clamped ([tiles](#tiles)) (`RootBackConsole`
  at 1280x720: the root's red background over the whole frame in both
  engines; `SnapSmoothConsole` on Liberty Island at 1280x720 through
  `D3DDrv`: the snapshot over the whole width in both, in soft gradients).
- **Full screen as the window loses the focus** is kept, by choice, where
  the original leaves it and minimises; SDL3 minimises a real exclusive mode
  alone, never on Wayland nor in a desktop's full screen.

In the SDL3 and X11 windows a wheel presses once a notch, as the original
presses once a `WM_MOUSEWHEEL`, which Windows sends a notch: a
high-resolution wheel's and a touchpad's fractions, which X11 and Wayland
send as events of their own, added up and pressed at each notch's half, the
sum begun again where the scroll turns (`WheelConsole` with
`tools/dxcap/wheel.py`'s eighths of a notch, the two windows alike). Each
notch gives the `MouseW` axis its delta first, 120 up and -120 down, as
Windows' `WM_MOUSEWHEEL` gives the original's
([the keys](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-keys);
`WheelConsole` with `tools/dxcap/timelines/wheel.txt`).

An optional argument left out takes the original's default: `LockMouse`
locks, `SetBaselineData` gives 2 and 1, and a button's `SetTextColors`,
`SetButtonColors` and `SetButtonTextures` set all six slots, one left out
green, white or None (`ButtonDefaultsConsole`: a button's six slots set
apart, then each call with its first slot alone, its insensitive and focused
look alike in both engines).


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
and the skins from one or two mips below the top at Medium and Low
([a texture's mips](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-textures-mips)),
World Texture Detail below High dropping the detail textures too; the
detail's fourth name, `Ultra Low`, two words, no text reaches, as in the
original. Low Sound
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
  the window's own and the screen's, a choice resizing the window; by choice.
- **Full screen in a Wayland session**, a size chosen is drawn at its size
  and scaled to the display by the compositor, the display's mode as it was,
  by choice: Wayland lets no program switch the display's mode.
- **Full screen on another display**: a window moved off the primary
  display goes full screen on the display it is on, Screen Resolution
  offering that display's modes, by choice, as a desktop program does,
  where the original's goes to the primary display wherever the window was
  ([the display](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-display)).
- **Full screen in the X11, Wayland and Win32 windows**, the fallback and
  harness windows, keeps the desktop's mode whatever `FullscreenViewportX`
  and `Y` say, by choice, where `D3DDrv` takes the display's mode nearest
  them; SDL's windows take the mode.
- **The full screen size** is kept as configured, 0 the desktop's mode, where
  the original's viewport saves each size it is given full screen as the
  client's full screen size, a size the display lacks saved as the mode it
  took; by choice, 0 having no mode to save and the next start taking the
  same nearest mode.
- **Detail Textures** draws the world's detail textures as `D3DDrv`'s passes
  draw them, all three in the scene shader's one pass
  ([detail textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#detail-textures)):
  a band each nearer than 380, 90 and 21.3 units, each band's grain 4.223
  times finer than the last, faded to grey toward its bound and modulated in
  at twice its colour, each detail placed by its own pan, not panned with
  its surface (`DetailConsole`, `detail.py`). A surface takes the detail
  texture of its texture's current frame, that texture as it is, not a
  current frame of its own, but for a portal or a surface with a fog map,
  as `Render.dll` hands them over. The original draws none here
  either way, `D3DDrv` and `OpenGLDrv` alike: `D3DDrv` turns them off when
  Direct3D does not offer `BLENDDIFFUSEALPHA`, which Wine's Direct3D 7 does
  not, and set on in the running game, its pass changes nothing
  (`MeshConsole` with `NoDetail`); what that does to the pier's picture is
  under [brightness](#brightness). Against `D3DDrv`'s passes, by choice, the
  fade is worked out for each pixel, smooth across a polygon, where theirs
  goes by a polygon's corners and passes over one with no corner within the
  band, and the bands are not rounded to 8 bits between passes.
- 16-bit **Texture Color Depth** is kept, DXEngine drawing in 32 bits.
  **Use 3D Hardware** finds no hardware, as the original finds none without
  an A3D or EAX card. Both by choice.
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
  Rendering Device does nothing, by choice.

The client's and the devices' settings are their original classes' config
properties ([a property's text](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#a-propertys-text)):
`UClient`'s and `UWindowsClient`'s, `UGalaxyAudioSubsystem`'s, and
`URenderDevice`'s with `D3DDrv`'s or `SoftDrv`'s own, as the ini's
`GameRenderDevice` names (`OpenGLDrv` has none of its own); those DXEngine
has no use for, `MipFactor` and `UseVSync` among them, are kept too. `GET`
answers each as its type writes it, and a name the class has not
`Unrecognized property <name>`, as `SET` does, nothing set or saved:
`Translucency` and `UseSpatial` among them, which no device of the game's
has. A `SET` takes its text as the type's import takes it -- a bool's or an
int's word refused, an enum by its names alone, so `0` is refused for a
detail; a float's `atof`, 0 for a word -- and saves the class's config
whole, each value as its type writes it, so the Display screen's Reset to
Defaults, which sets the details to `0`, leaves them High
(`SetWordsConsole`, every answer alike in both engines and every value saved
alike but `UseDirectInput`'s; `FallbackConsole`, every `GET` alike but
`UseDirectInput`'s; `ResetConsole`'s Display and Sound screens alike after
Reset to Defaults and OK but for the resolutions offered). `UseSound` is
read by its first token, so `0.0` silences the game. By choice: a
setting the ini lacks, or holds in a form its type does not read, takes
DXEngine's default, where the original's classes set none, their defaults
zeroed -- a damaged ini gives the original a black gamma ramp, no voices and
8 kHz mono, and of a stock ini only `UseDirectInput`, which nothing of
DXEngine's reads, and the software device's `DetailTextures` differ, True
and On where the original's are False and Off; the mouse moves by raw input,
where the original's moves by the window's pointer moves from its middle,
the system's pointer acceleration in them, forced on while it holds the
mouse; `UseJoystick` is read, saved and ignored, a pad following the
launcher's `Settings.json`, where the original's stock False keeps a
joystick out. A mouse's move turns the view as far a count as the
original's a window pixel: its `Axis` adds the speed x the move x 0.01 at
the move, which the next read scales by 20 over the tick, 1.2 a count with
the stock `MouseX=Axis aMouseX Speed=6.0`, the axis named in any case
(`MouseLookConsole`: 23.0 yaw a `MouseX` count and 23.0 pitch a `MouseY`
count by `tools/dxcap/timelines/mouse-look.txt`, the original 22.8 and 22.7
a pixel by `tools/dxcap/mouselook.py`'s uinput mouse, at the stock
sensitivity and field of view; the build before took each move 16 times and
scaled only `aMouseX` and `aMouseY` by the tick).

## Sound

Matches `Galaxy.dll`
([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)):
Deus Ex's sounds are mixed as its mixer mixes them (pan, volumes, resampling
and reverb, at `OutputRate`, which keeps its default when the ini's value is
none of its names) into one OpenAL source, one behind the level's BSP
muffled -- a closed door's too, the movers in it where it filed them ([the
movers in a line's way](#implemented-not-as-the-original)); the wall test is
the solid-space walk the original's `FastLineCheck` makes (`FastWalk`, the
hearing test's): a solid brush muffles through its invisible faces too, open
space on both sides of a polygon does not (`SoundConsole`'s wall, recorded
on both engines). The line is walked from the sound's actor to the eyes, as
the mixer asks it ([sounds behind
walls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#sounds-behind-walls)),
whatever its length: a sound at the eyes of a listener inside solid is
muffled (`SolidSoundConsole`, recorded on both engines: a fan at the eyes 10
dB down in the solid under Liberty Island's pier, and up again in the open).
Who hears a sound an actor plays is the original's too
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)):
the player within the radius ÷ √1.3, and with the level's BSP between, at
0.35 of the volume within 0.77 of that, but for the actor's instigator.
`SoundConsole` (its hearing test) and `SkipConsole` record both engines. A
window's sound is the original's too ([window
sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#window-sounds)):
in the world, one unit from the player, turned by its place on the screen,
at the window's own volume, which is 0 unless the window sets it, so at
Galaxy's floor (below). `SoundConsole`'s root beeps from the screen's left
edge, centre and right edge come out within 0.2 dB of the original's, panned
alike, the one at the root's own volume 41.9 dB under the centre's in both.
A map's load stops the sounds and leaves the music playing, as Galaxy does
when the load hands it the viewport it has ([playing a
sound](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#playing-a-sound)):
the old song fades out and the new level's starts (its player's
`MTRAN_Fade`), a level of the same song jumping to its section
(`LoadingConsole` with `DXCAP_RECORD=1 DXCAP_MUSIC=1`: no silence across a
load in either engine, the fade after it, the music on after a save of the
map it is on is loaded). `OutputRate` is one of Galaxy's seven names,
8000Hz, 11025Hz, 16000Hz, 22050Hz, 32000Hz, 44100Hz and 48000Hz, read by
the text's first token in any case, never from a number, any other text
keeping the rate (`SetWordsConsole`, alike in both engines). DXEngine holds
`Latency` to 10 to 250 ms, at a load too, by choice (Galaxy holds it so at
a `SET` alone, at a load its library's 5 to 495 ms), `Latency` the output's
mix-ahead (10 ms buffers queued to it), and so
are those a `SET` puts on what plays, where `GET` gives the value as set
([settings](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#settings)):
`DopplerSpeed` to 1-100,000 and `AmbientFactor` to 0-10. `AmbientConsole`'s
fan, recorded on both engines, plays 23.2 dB louder at an `AmbientFactor`
set to 20 than at 0.7 in the original and 23.1 dB in DXEngine, as at 10
(29.2 dB without the hold). A voice is never quieter than Galaxy's floor,
1/256 of full
([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)):
a sound gone out of its radius as it plays (an ambient one stops there), one
played at a volume of 0 and one whose own slider is at 0 while the other is
not play on at it, and only both sliders at 0 silence a sound.
`AmbientConsole` with `Factor=0.01`: the fan 14.7 dB under its level at 0.7
in the original and 14.8 dB in DXEngine (37.9 dB without the floor); with
`Volume=1` too, 0.6 and 0.7 dB under, the floor holding the fan at -60.1 dB
in both either way. `UseStereo=False` makes the whole output mono, as
Galaxy's ([the
mixer](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#the-mixer)):
each sound's voice and each of a song's voices played at the mean of its two
sides' gains, a surround voice's right side added, not cancelled, and the
sounds' mix saturated only as the music is added to it, once
(`MonoSumConsole` with `DXCAP_INI="Galaxy.GalaxyAudioSubsystem:UseStereo=False
Galaxy.GalaxyAudioSubsystem:UseSurround=True"`, by `sound.py --onsets`: a
beep seven-eighths right at -8.0 dB in the original and -8.4 in DXEngine,
two, three and four of them on one tick at -2.6, -1.8 and -1.4 dB and -2.6,
-1.4 and -1.0, where each side held before the fold gave -4.7, -3.3 and
-2.3; one and two behind at -6.9 and -2.3 dB and -7.1 and -2.0, where they
cancelled to silence; `MusicConsole` with
`DXCAP_INI=Galaxy.GalaxyAudioSubsystem:UseStereo=False`, both engines: the
pair after order 0, a tone hard left and one hard right, -19.7 and -19.3 dB
in each channel in the original, -19.5 and -18.8 in DXEngine; the stock
songs each within 0.47 dB of the original's mean power, UNATCO_Music -17.2
dB in the original and -16.7 in DXEngine, where a fold of the stereo music
cancelled its surround channels to -25.8). Not carried, by choice: Galaxy's
mono mix's losses, each voice's gain rounded down to one of 64 levels (a
quiet sound silent, a fade stepped), a 16-bit sample read without
interpolating, nothing sent to the reverb and the music saturated keeping
each sample's low byte (clip noise). `UseFilter`, `UseCDMusic` and
`UseDirectSound` are read, saved and `SET` and change nothing, as in the
original. An MP3 --
the game's speech -- plays as Galaxy's decoder fills its ring, 32,768 frames
of each channel ([playing a
sound](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#playing-a-sound)):
4,096 frames of silence first, then its sound, then on, silent, until the
fill sets its end, at Galaxy's ticks, timed by the music's tempo -- one
whose data, the silence counted, ends within the first fill to that fill's
end, 20,480 frames, one within the ring's first turn with its data, and a
longer one to within a tick of the ring's turn after its data's end; each
turn after the first passes over the ring's first frame, the ring's loop
ending at its last. Galaxy's first fill is at its first tick after the voice
starts, as its mixer's thread falls against the game's, DXEngine's at the
voice's start, by choice, the original's usual case. `LipConsole` with
`Beep=True` and `DXCAP_RECORD=1`: each of its three lines 4,085 to 4,096
frames after a beep started on the same tick, the original's 4,083 to 4,096,
or 512 more as its voice starts against its mixer's tick; and with `Speech`
naming the stereo `ConAudioMission01_450`, 4,085, the original's 4,107 and
4,608. Lip sync is Galaxy's ([lip
sync](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#lip-sync)):
a shape each tenth of a second of a line's time played, from 1,024 frames
where its voice plays, the first at the update after it starts; an 8-bit
sound's from its first 1,024 bytes wherever it plays, every other one, in
half the FFT's input, the rest kept from the read before (`LipConsole` with
`Speech` on the harness's `Cap8Bit`: 19 `O`s, then two `X`s as the place
read from passes its end, in both engines). A WAV loops by its first loop
only from a `smpl` chunk after its data chunk, as Galaxy's loader sets the
sound up afresh at the data (`CapEarlyLoop` once in both engines), and a
voice mixes the frame that falls on its loop's end or its data's end too, a
sound being its decoded samples and no more, as Galaxy's loader holds a
WAV's data (Galaxy.dll `0x10619d20`; by reading, the 1 to 4 samples of
silence DXEngine added past a sound's end too few for a run to show).
`GetSoundDuration` is the original's, read from a WAV's header and 0 for an
MP3
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)):
`OpsConsole`'s, a beep's and a mission 1 line's, alike. `LipConsole`: 30, 6
and 30 shapes on its three lines, the last running to 2.9 s past its sound's
2.6 in both engines; one after another, 78.0% of them alike between the two
engines' runs, where the original's own runs are 79.7% alike. Differs:

- The music is heard as it changes, by choice, where the original's comes
  some 0.9 s after the sounds of the same moment, the Sound menu's music
  slider too (`MusicConsole` under Wine): Galaxy's own lag, the music
  written into its output's one-second buffer where it plays, a turn before
  it is heard
  ([the mixer](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#the-mixer)),
  which no player misses, and which copied would leave the slider a second
  behind.
- Not carried, by choice: Galaxy's start of a song past its first order,
  every voice centred and 48 ticks a row until the song reaches order 0 or
  a speed command
  ([music](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#music)),
  a defect of Galaxy's. DXEngine's song takes its own pans, speed and
  tempo from its start (`MusicConsole`'s first pair: centred in the
  original, each tone to its side in DXEngine).
- Not carried, by choice: Galaxy's quirk with the Sound and Speech sliders
  equal, both kinds of sound scaled by the slider once more, everything at
  an eighth with both at half.
- A change to `EffectsChannels` (the Sound menu's Effects Channels) applies
  at once, held to 0-32, as the original's. By choice, the sounds on
  channels past a lowered count stop, where the original's play on out of
  its reach, a looping one for good, and a raised count's channels are
  voices of their own, where the original's are its music's
  ([settings](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#settings)).
- A WAV's ping-pong loop plays forward, by choice: the original's crashes
  the game at the loop's first turn (`LipConsole` with `Speech` on the
  harness's `CapPingPong`, twice), where its forward `CapLoop` plays alike in
  both engines, each tenth of a second's level within 0.1 dB over six turns.
- A stereo sound plays once, by choice, both its channels at the sound's
  pan, as a mono MP3's ring has it, a WAV of 16-bit samples too, after the
  ring's 4,096 frames of silence (the harness's `CapStereo` 8,180 output frames
  after its beep, the original's 8,192 at its 22,050 Hz, where DXEngine
  played it at once); an 8-bit one from its start. The original's plays its
  ring's first 32,768 frames over and over, its lip sync with them, until a
  sound on its slot stops it; a beep its speaker plays with each later line
  goes unheard, and the game crashes at its exit, unloading a sound
  (`LipConsole` with `Speech` on `ConAudioMission01_450`, twice, and on the
  harness's `CapStereo`): a defect of Galaxy's, ending in a crash, not
  carried. The stock game has four stereo sounds, all MP3s:
  `ConAudioHK_Shared_681`, `_1531` and `_1532`, and `ConAudioMission01_450`.
- An ambient sound on a light follows its blink, strobe and flicker as the
  lights' last step left them, at the original's pace at 60 frames a second
  ([lighting](#lighting)), by choice, where the original's follows them each
  frame.
- A sound's ID packs a number DXEngine gives each object that plays one,
  never given out twice, by choice, where the original packs the object's index
  ([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds));
  the slot and `bNoOverride` are packed alike.

With no audio output -- no sound card, server or endpoint -- the engine logs
`Audio initialization failed.` and plays on silent, as the original's drops
its audio subsystem
([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)):
a sound's ID is still given, and none is heard (`ProveConsole` with OpenAL
Soft's wave driver given no file). A song that fails to load is logged and
none plays, the game going on, as Galaxy's `glxLoadMusic` failing is a
warning.

The music's volume is Galaxy's
([music](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#music)):
127 x the fade x the Music slider / 255, cut to a whole number, squared as
Galaxy's mixer squares it with a music voice's own volume, the 100 of 127 a
song's load leaves it at: 0.601 of full at 255, 0.215 at the game's 153 and
0.036 at 64. A transition fades the song out by that square, over 1 s
(`MTRAN_Fade`), 1/3 s (`MTRAN_FastFade`) or 5 s (`MTRAN_SlowFade`), at once
for the others, holds it silent for twice `Latency`, then starts the
player's song at full volume, the old song's sound dropped before the volume
rises. The music goes to the speakers as it is, as the sounds' mix does,
never through OpenAL's HRTF, UHJ or a surround layout. A song plays as
Galaxy's IT loader converts it (`DXEngine/Audio/GalaxyMusic.cpp`): none of its mix
volume, its channels' volumes or its samples' and instruments' global
volumes; the commands Galaxy drops dropped, one effect memory a channel for
every command, a speed over 32 at 32, a T under 0x20 kept for Galaxy's
vibrato, an order whose pattern is empty passed over; read between samples
linearly, unramped; each channel panned as
Galaxy pans it, each side the square root of its share of a pan held to 127
of 128 ([the audio mixer](ENGINE.md#gameplay)). `MusicConsole` on the
harness's `CapTone`, recorded on both engines: tone A at 255 -16.5 dBFS in
the original and in DXEngine; at 153 and 64 8.75 and 24.4 dB under in
the original, 8.92 and 24.5 in DXEngine; a fade's 3 to 12 and 12 to 24 dB
steps 0.32 and 0.26 s in the original, 0.34 and 0.25 in DXEngine
(`MTRAN_FastFade` 0.12 and 0.08, 0.11 and 0.09; `MTRAN_SlowFade` 1.67 and
1.27, 1.70 and 1.24); silent 0.09 to 0.34 s before the new tone in each, no
burst of the old one; the pair after order 0, a tone hard left and one hard
right, -13.6 dBFS in the original and -13.5 in DXEngine, the right one 21.1
dB under in the left channel in both, the left one none in the right; with
`DXCAP_HRTF=1` the same. The stock songs from their first order, each laid
against the original's run by its levels, are within 0.11 dB of the
original's mean power in each channel over the same 6.5 s: Title,
LibertyIsland, UNATCO, VersaLife and NavalBase (Title, the menu map's own,
-12.0 and -13.5 dB in two runs of the original, -12.0 in DXEngine);
BatteryPark's from its onset -16.3 dB in the original, -16.4 in DXEngine.
A song plays on as Galaxy's player plays it
([music](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#music);
libopenmpt's `play.galaxy`, [the audio mixer](ENGINE.md#gameplay)): its
loop, a jump to order 0 by B00 or a section, and a pattern loop back in
order 0 give each channel the song's own pan again and the song its own
speed and tempo; a section change silences its voices at once and starts
the section as the playing row ends, a pattern delay's rows played out
first, the speed, tempo and pans as they stand, the song not played to it,
`SongSection` reading the order before it until then, 255 before order 0;
a section past the song's last order is refused, logged as Galaxy logs it
(`glxControlMusic( GLX_SETPOSITION2, Viewport->Actor->SongSection ) failed:
17`); a channel the header pans over 64 -- IT's surround, or a muted
channel, played -- is a surround a quarter of the way from the left, and
an 8xx (IT's X) over 0x80 a surround at a pan, 0x84 to 0xC4 from the left
(0xA4, IT's S9x, the middle), each with its right side inverted; its
vibrato (H, K's, and a T under 0x20) none on a row's first tick, its depth
over 8 of a semitone at the sine's top, twice IT's, and down first, each
struck note starting it at its first step on the sine; its retrigger (Q) the
voice's place back to its sample's start on each tick of the row that is a
multiple of its parameter, the first among them, nothing else; its note cut
(SCx) the note's volume 0 on each tick past x, the note playing on, so a
later volume brings it back, SCF at a speed over 16 from tick 16. The music
is saturated at 16 bits at its gain, then added to the sounds' mix and the
sum saturated, by the engine, before any volume of the system's, whatever
the output's format, the music rendered at `OutputRate` as Galaxy mixes it.
`MusicConsole` with `DXCAP_CONFIG="Songs=DXCapture.CapCommands
SongSeconds=18"` and `tools/dxcap/commands.py`, both engines: T1F's and
H1F's vibrato from 396 to 491 Hz, H1F's down first, the lows over order 0 at
the same times within 0.2 s, where T1F stayed at 441 Hz and H1F went from 418
to 466 Hz, up first; order 1's bursts 80 and 40 ms apart in turn, where they
came every 80; order 2's notes silent past each cut and back at the next
row's volume, where the build before held them through. `MusicConsole` with
the game's stream at 50% (set back to 100% while it was open): over tone A
past full scale the peak 0.125 of full scale and 0.129 with the next step's
beep over it, the original's 0.126 and 0.128, where DXEngine's beep passed
full scale to 0.192.
`MusicConsole`, both engines: after the jump from the pair to
the empty order 1 silence in both, where playing the song to it brought
back tone A at -16.5 dB; tone A on eight channels in phase at full scale in
35.2% of the samples in the original and 32.0% in DXEngine, the beep over it
-12.7 dB in the original and -12.6 in DXEngine; the pair centred by order 11,
then after the song's loop the right tone 21.1 dB under in the left channel
in both, where it stayed centred; from tone A in a row of 2.42 s, the jump to
tone B's order silent 1.42 s in the original and 1.43 in DXEngine, where B
followed at once, and the one from B to order 0 2.28 and 2.28,
`SongSection` the order before each until then, 1
and 255, in both, where it was the order at once; the pair on the header's
surround channel and on XB4: the 551.25 Hz tone 4.77 dB louder in the left
channel than in the right and the 882 Hz one 4.77 dB louder in the right,
each right side inverted, in both, where each was centred. With
`DXCAP_CONFIG="Songs=LibertyIsland_Music.LibertyIsland_Music SongSeconds=150"`
and `sound.py --music --from 138`, LibertyIsland after its loop to order 0:
its side 0.05 dB from the original's, 3.36 before. A section past the last
order is refused with Galaxy's log line and the song plays on, by choice,
where Galaxy's silences the voices all the same and plays on at the next
order as the row ends, a glitch.

By choice: an MP3, Ogg or WAV song plays, where Galaxy's loader refuses one
(its MOD loader, the last it tries, failing with 15), a looping Ogg one from
its start again at its end, its last packet played out first. By choice: the
gains a sound's update sets reach the output each 10 ms, Galaxy's each 20
ms, so a moving or fading sound steps twice as often; a song's speed
command that comes to 0 is dropped, where Galaxy's stops its sequencer, the
song halted while its sounding voices play on, which no stock song has.

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
  caller among them (`WalkConsole`). `VisibleActors` takes an actor whose
  squared distance, summed as the original's x87 sums it, is under the
  radius squared, so a negative radius reaches as far as a positive one,
  and a radius of 0 or a NaN has no limit (`LineEdgeConsole`). A line or a
  box started inside an actor's cylinder, heading in, stops at its start,
  its normal level, out from the axis
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces);
  `TraceConsole`'s boxes started near a decoration's top and at its middle).
  A cylinder is checked as the original's `UPrimitive::LineCheck` checks
  it, on the line it is given, the cut line for the multi-hit check, in
  floats, its normals made units in floats: at the x87's 24 bits in the
  console's tick and the frame's script events, where the original's
  render holds its x87 there, and in doubles in the level's tick, the
  nearest a port has to the original's 64 bits there (`PrecisionConsole`:
  7 *= 3e9 in the original -474,836,992 in the console's tick and
  -474,836,480 in the level's; `VisibleConsole`: UNATCO HQ's
  `DeusExMover24` at `SkillAwardTrigger0` at y -9.699432, Liberty Island's
  `DeusExMover18` at `DataLinkTrigger21` its normal, the original's).
- **A line** meets the level as the original's walk down its BSP does
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
  where it enters solid space, a start in solid stopping it there, at time
  0, and a polygon with empty space on both sides stopping nothing; the
  hit's normal is the plane of the node the walk crossed, turned to face the
  start, and its surface that node's. `FastTrace` is blocked by any solid
  leaf its line touches, its start's included. A mover's brush is walked the
  same way in the world, its planes turned into it as the original's walk
  turns its owner's, its hit's normal the crossed node's plane so turned,
  not turned to face the line. The hits come out as the original's
  multi-hit check hands them on, their times in its floats and its order:
  the level's one, its time `UModel::LineCheck`'s, the line cut to 5 units
  past it, its end a float; with the level met within a hundredth of the
  line and 30 units, every actor's hit but a mover's dropped; a mover's
  brush walked along the cut line, its time made the whole line's by the
  cut; a mover's hit within 2 units of the level's, with the level hit's
  normal (a float compare), set 2 units before it; the hits sorted by their
  times alone, equal ones keeping the actors first, so a mover flush with
  its wall falls before or after it as the original's two times round
  (`VisibleConsole` on DXMP_Silo: 55 movers' lines each way, their hits,
  normals, `TraceTexture`'s and `TraceActors`' alike, among them the bot
  doors whose wall lies a float step off the door's face; `TraceConsole`,
  `MoverConsole`). `FastTrace` walks every line from its start, one of no
  length too, so from inside solid it is blocked, both ends' distances from
  each node's plane and each point where the line crosses one floats in the
  original's order, at the x87's precision in the tick as a cylinder's
  check is (`LineEdgeConsole`).
- **A point's region** is found as the original's `PointRegion` finds it
  ([the zone an actor is in](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-zone-an-actor-is-in)):
  down the BSP, the point's side of each plane by its distance, the Z, Y
  and X products summed in that order less W at the x87's precision in the
  tick and compared unstored, in front at 0 or more and behind for a NaN,
  to the child on that side until there is none; a model with no nodes
  puts it in no leaf.
- **A box** meets the level as the original's box check does
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
  down its BSP on each side either end's box, grown by a tenth of its size,
  reaches, the hull of each solid leaf it reaches tried and no other, a hull
  entered within twice the trace's length counted, the hit backed off a
  tenth of the trace: a box traced 8 units down from 8.78 over the floor
  meets nothing in either engine (Liberty Island's carcass crossbow). The
  check's sums are floats, in the original's order, and its walk takes the
  side of a node the box starts on first, so a box resting on a plane or
  started inside a hull meets it as the original's does. A mover's brush
  meets the box in the world's axes, its planes turned into the world by
  its place, rotation and scale, as the original's walk turns its owner's:
  the player walked into and along the grates under Castle Clinton on
  02_NYC_BatteryPark, pitched on their sides, rests where the original's
  does within a hundredth of a unit (`GrateConsole`). The hits come out as
  the original's multi-hit check hands them on, as a line's do: the level's
  one, the line cut 5 units past it, and the actors in the cells along the
  cut line asked along it itself, so a mover's hit is backed off a tenth of
  the cut line and a cylinder's a thousandth of it; a box the level meets at
  its start meets no actor, the line cut to nothing and a box swept no way
  entering no hull, where a line from inside a mover's solid still meets it;
  and a mover's hit within 2 units of the level's, with its normal, is set 2
  units before it. `GrateConsole`'s 16-unit boxes swept round the five
  grates, at the 1,842 places a terrorist walking through does not reach,
  hit as the original's do -- the level at the start where a grate or
  `BreakableGlass0` lies there too, the level before a grate flush with it,
  a grate a tenth of the cut line short, each hit at the place its check
  left, a cage light's cylinder normal to its last digit.
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
  what stands on it carried first -- by the move and round it by the change
  of yaw, each yaw to a multiple of 4, its own yaw turned by the change, its
  pitch and roll left, as for any actor that turns (`TurnConsole`: a crate
  on a crate turned six times by `SetRotation`, alike in both engines) --,
  then its encroachment checked at its new
  place and rotation as the original's box check asks it
  (`UModel::PointCheck`): the actor's box along the world's axes, against
  the brush's planes turned into the world by its place, rotation and
  scale, the hull of each solid leaf of the brush's tree the box reaches
  tried with the bevels its planes' signs call for, a box resting on a
  plane clear of it; what it meets pushed by its move first, and moved and
  turned at once. A hinged door crushes a crate in its swing where the
  original's does: gone by yaw 14,677, 0.3 s in, the original's by 14,594
  and 14,680 (`DoorConsole` on UNATCO HQ: the crate placed at -875.43,
  362.61 in both engines).
- **The movers in a line's way**: a line meets each mover where the level's
  BSP last filed it
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)),
  whatever it collides with, hidden or not. A map's load and a save file
  every mover where it is; each frame drawn files again where it is every
  mover the frame does not pass over, as it passes over a hidden actor,
  that has moved or turned since, before anything is placed; a destroyed one
  stays filed until the next load or save. A dedicated server, which has no
  client, files none at its load, as the original's makes no tracker
  without one: `FastTrace` and the checks on its line pass its movers, a
  `Trace` meeting a colliding one only as an actor (`MoverServer` on
  DXMP_Silo, through `dxcap.sh dedicated`: a closed door's line clear to
  `FastTrace` in both engines, `Trace` meeting the door; a door moved off it
  clear at both its places). `FastTrace` and the checks on
  its line (`LineOfSightTo`, hearing, the reach tests and path searches, the
  visible-actor iterators, network relevance, mesh lights' shadows, the AI's
  sight and the mixer's walls, [sound](#sound)) meet them as the original's
  do: the movers' polygons filed into the level's BSP as the original's
  tracker files them -- each through its brush's coordinates as
  `FPoly::Transform` takes it, its corners less `PrePivot`, turned, scaled
  by `PostScale` along the world's axes and moved, each step in the
  original's float order, a mirror's normals turned with it and its corners
  reversed (`PostScaleConsole` on WanChai Market and DXMP_CMD and
  `MirrorMoverConsole` on UNATCO HQ and 09_NYC_Ship: every line alike) --,
  each
  split down the tree, a fragment in empty space a
  node of its own, the space behind it solid within its leaf, and one in
  solid dropped, one in a node's plane hung at the end of that plane's
  chain; a mover filed again takes the movers filed under it with it, the
  last filed first. A line's `Trace` and the multi-hit check meet them as
  the level's: the node the line meets, its plane, and the surface the
  original files a polygon under -- the polygon's flags without the
  editor's own (`PF_NoAddToBSP`), with `PF_SpecialLit` for a `bSpecialLit`
  mover, which it draws with its texture's own --, a polygon in the plane
  of the level's own node met as that node; and a box meets none. A mover
  with a `PostScale` meets a line, a box and an encroached actor where its
  polygons are filed, as the original's lines meet it (`PostScaleConsole` on
  DXMP_CMD: no line meets the bot door where the plane transform its checks
  are read to take would stretch it), its hit's normal the original's,
  scaled by `PostScale`. A brush
  whose solid holds a line's
  start still meets the line when the level's BSP holds the start too and
  cuts the line to nothing, as the original's actors are checked along the
  line cut so (`MoverConsole` on Liberty Island and DXMP_Silo: a door made
  not to collide, then hidden and moved, then saved; a window broken, at
  once and a frame on; `VisibleConsole` there and on UNATCO HQ, the mover
  lines alike, each hit at the place its check left, a pulled one 2 units
  before the wall it lies at: all 377 on DXMP_Silo, 133 on the island and
  177 on UNATCO HQ; `DoorConsole`: the door's leaf found from inside it;
  `GlassConsole` on WanChai's club glass: 33 in both, its polygon's own
  flags carrying the editor's processed bit too).
- **Auto-aim's targets** (`AimConsole`): `PickTarget` takes a live
  projectile-target pawn ahead, nearer than 2000 units, whose aim is
  strictly the best so far and in sight, `PickAnyTarget` such an actor that
  is no pawn, and neither any without a `Game` (a client's level).
- **Sight** (`SightConsole`), and the pawns' in the tick (`SeeConsole`:
  `SeePlayer` sent only a player, each 0.2 s, [the
  senses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-senses)),
  **a pawn's pain and speech timers**, each due once under a thousandth of a
  second (`PainConsole`, both at half speed: the tick each fires in alike),
  **the reach tests** from a pawn's home spot (`ReachConsole`) and from a
  node (`NavReachConsole`; a lift's floor, `LiftReachConsole`), and
  **the look up and down stairs**
  (`StairConsole`: every answer of `Pawn.FindStairRotation` alike on Liberty
  Island's pier and `SlideConsole`'s slope,
  [small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
- **Falls**, a whole frame at once in the original's steps, each by its mean
  velocity (a pawn's air control, a bounce left to the script, a wall slid
  along, a landing ending the frame's move), coming to rest where the
  original's do at 60 frames a second and at 20, and their landing on a
  ledge -- a crate nudged off it, a pawn fitted clear of it and pushed on at
  random (`RestConsole`,
  [moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)).
  A floor met sliding along a wall lands with none of the frame left to
  walk, as the original's physFalling hands its landing no time
  (`0x103d20b3`, `0x103d220f`), by reading.
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
- **A trailer** (`TrailConsole`): a sprite rides behind its owner's facing
  by its `Mass`, or at the owner's `PrePivot`; another actor sits on the
  owner, turned with it or against its velocity, straight up when it has
  none.
- **Water**: falls in it (`FloatConsole`: a buoyant crate rises and bobs at
  the surface at the original's speeds, and now and then comes to rest just
  under it in both engines: Engine.u's `Decoration.ZoneChange` stops one
  that falls slowly back in where no player sees it, one time in twenty)
  and swimming (`SwimConsole`: the
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
  sea, the stock maps' one shore). The speed a pawn walks, swims and flies
  at (`calcVelocity`, once a frame; one that stops brakes to a standstill),
  in the original's float order: the original's own instructions, run
  natively on the same inputs (`tools/calcvel`), give the same velocity and
  acceleration to the bit, at the level's 64 bits and at 24, walking,
  braking, a walking player alone and in a net game, swimming and flying;
  ladders (`LadderConsole`: a player at a ladder's foot hangs there, climbs
  as it pushes into it, goes down looking down and steps off onto the top);
  and the same check's slide of a walking player on a texture with a
  `Friction` under 1, its move in the original's float order (`SlideConsole`:
  on a slope at a `Friction` of 0.5 and 0, the glide to a stop and the slide
  down a slope in a zone of ground friction 0.2, at 60 and 20 frames a
  second; the game's one such texture, `Slippery_Metal`, covers two slopes
  in 14_Oceanlab_Silo). The walk after a landing goes on with the fall's
  count of steps, the eight of a frame shared (`LandStepConsole`: 107.7
  units along, at 268 a second, at the first tick after a 0.4 s landing
  tick, in both engines); its floor is the first thing a box of its size
  meets below, whether it blocks the walker or not, so over a flat actor
  that blocks nothing the walker steps down onto the floor beneath, by
  reading (`SingleLineCheck` with flags 23, `0x1039aed0`); and a physics'
  moves share one hit, a move of nothing leaving it as
  it was, so a walker head on into a wall takes `HitWall` twice a step
  (`WallHitConsole`: two on each tick, in both).
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
  leaves behind (`TeleportConsole`), whatever at the spot does not block it
  touched, what stands on the actor and what it stands on among them, and
  what stands on it left in the level's order (`HashOrderConsole`'s rider
  and base: the same four touches in both engines); a pawn, a decoration or
  an inventory item that leaves the world logging the original's "<name>
  fell out of the world!" as it falls or walks out, a flyer "<name> flew out
  of the world!" (`FellOutConsole`: the same five lines in both); a touch
  linking each side on its own,
  its Touch sent as it is linked and the other side linked unless that undid
  it, the last free of the four slots taken, and a full four freed of their
  touchers with no physics, or for a pawn of a non-pawn, or for a player of
  a non-player, as the original's BeginTouch links (`TouchConsole`); the
  rotators' conversions, `<<` and `>>` by the sine table, a rotator's `*=`,
  float `==` exact, a rotator's `==` and `!=` every bit of each angle,
  `GetEnum` from index 0, `Normal` zero under a length of 0.0001, `Chr(0)`
  the empty string, `IsInState` true for a state the state extends, whose
  labels and functions the extending state has too (`ExtendConsole`), and a
  float made an int past an int's range, wrapping, by `int`, `byte` and a
  rotator's scaling (`RotatorConsole`); a string made a `bool`, an `int`, a
  `byte` or a rotator's angles by the original's `appAtoi`, blanks and a
  sign taken, a number past an int's range held to its ends, a byte its low
  8 bits, `0.5` and `0x1` false (`SweepVmConsole`); the string natives on
  16-bit characters, a script's strings being UTF-16 as the original's:
  `Chr` its low 16 bits, `Asc`, `Len`, `Mid`, `Left`, `Right` and `InStr` by
  those characters, `Caps` a to z alone and `~=` without their case alone,
  the comparisons by each character's code (`OpsConsole`'s wide lines, alike
  with the original's); a struct's `==` and `!=` each member by its
  property's compare, a string member without case, as the original's
  `execStructCmpEq` (`SweepVmConsole`: a struct of `abc` equal to one of
  `ABC` in both ticks); `Object.Mid` 127 at its edges, its end's clamp known
  from `MidConsole`'s ten probes of both; `Object.DynamicLoadObject` with a
  group in the name, its own, another or none, and a class that is not the
  object's own (`DynLoadConsole`); and `Object.GetPropertyText` and
  `SetPropertyText`, through which the server's query answerer reads what a
  remote asks for: a public property's text as `GET` writes it, a const one
  never set, a private one or a name the class has not read empty and set
  nothing (`GetConsole`); text that is no value of the property, by
  `SetPropertyText` or the console's `SET`, leaves an int, a byte or a bool
  as it was and makes a float 0, as the original's import does, an int read
  as far as it is a number and a name its first token -- quoted, or a run of
  letters, digits, `_` and `-`, or else one character (`JunkConsole`); a
  property a subclass names again is the subclass's, as the original's field
  walk takes the class's own first, the console's `GET` and `SET` too, `SET`
  setting the one found on the class it names in every object of it
  (`SweepVmConsole`'s `bSinglePlayer`; `GetConsole`'s `GET` of
  `PlayerPawn`'s and `GameReplicationInfo`'s `Region`, and a `SET` of
  `PlayerPawn`'s). `LevelInfo`'s clock is the
  original's
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
  int's range wraps, as `int` does, its product or quotient as the
  original's x87 holds it in the tick the script runs in, rounded to a float
  at 24 bits in the console's tick and whole in the level's
  (`PrecisionConsole`: 7 *= 3e9 -474,836,992 in the console's tick and
  -474,836,480 in an actor's, in both engines); `VSize`, `Dot`, `Lerp` and
  `Normal` worked out as the original's x87 does, in its order -- `VSize`'s
  squares and `Dot`'s products Z's and Y's first, then X's, `Normal`'s X's
  first, `Lerp` (B − A) × Alpha + A -- at its precision in that tick, stored
  once as a float, `VSize`'s and `Normal`'s sums as a double for the root; a rotator times or over a float each angle by
  `_ftol` of the angle times the float, or times one over it, at that
  precision (`SweepVmConsole`'s mantissas alike in both ticks, `Normal`'s
  apart in two of three at 24 bits from a double's; rot(10, 20,
  30) × 0.7 is (7, 14, 21) in the console's tick and (6, 13, 20) in the
  level's, rot(90, 180, 270) / 90 (1, 2, 3) and (0, 1, 3)); `Clamp` and
  `FClamp` take a
  value under the lower bound to it, else one under the upper as it is, else
  the upper, so with the bounds the wrong way round the upper wins; a NaN
  compares as the original's x87 flags have it, equal to anything and under
  anything (`NaNConsole`, a NaN made as the square root of -1, alike in both
  engines): `==`, `<`, `<=` and `~=` true, `!=`, `>` and `>=` false, as
  values and as branches, `bool` of it False and of a vector each NaN axis
  0, a vector's `==` and `!=` taking it as equal, and a struct's (a Plane's,
  a Coords') too, by choice: the original's crashes the game at a Plane's
  `==` with a NaN member; `FClamp` it to the lower, `FMax(NaN, 1)` 1 and `FMax(1, NaN)`
  the NaN, `Abs` turning its sign over (and keeping -0 as -0), `Normal` of
  it zero; a float property is compared so with its default and, in a
  script class, with what a client last got, as the original's `Identical`
  compares it, so a save and the travel leave a NaN out and replication
  does not send one; a
  shift takes its count's low 5 bits; a string `switch` matches its cases
  without regard to case and a float one by its bits (-0.0 is not 0.0);
  `new` with a flag outside the script's own (Transactional, Public,
  Transient and the three NotFor) logs `new: Flags %08X not allowed` and
  leaves it out; every actor iterator given None takes Actor, and walks the
  caller's own level; `OrthoRotation` is the original's; a destroyed pawn
  keeps its `nextPawn`, so a walk through it goes on; `PlayBlendAnim` with a
  slot outside 0..3 logs and does nothing; `StrLen` measures under
  `STY_None` as under any style, and `DrawText`, `TextSize` and
  `DrawTextClipped` with no font log and do nothing (`StrLen` with none,
  which asserts in the original, answers 0 by 0, by choice); `GetNextSkin`
  walks the `.int` files' Texture objects as the original does, none in the
  game's. Differs by choice: an int divided by -1 at INT_MIN is INT_MIN on
  every port, where the original's divide faults (a Critical Error).
- **A property's text** (`TextConsole`; `SetPropertyText`,
  `GetPropertyText`, `SET`, `GET`, an ini's or a `.int` file's value, a
  travel's): as the original's import and export have it. A struct's text is
  `(X=1.000000,Y=2.500000,Z=-3.000000)`, its members by name and a static
  array's by `Name[i]`, its own before those of the struct it extends (a
  Plane's W, then X, Y and Z), a quoted string holding a comma or a
  parenthesis whole, a nested struct whole; only the members named are set.
  An element by its index, `N[1]=7`, stops the import there, logged
  (`ImportText: Illegal array element`), the members before it set, as the
  original's index reader starts on the bracket and finds none, though its
  own export writes them (`TextConsole`: `N[1]` left 0 after `Text` and
  `Key` are set, in both engines). A dynamic array is `(1,2,3)`. An object
  is `Type'Package.Group.Name'`, set from that, from `Package.Name` or from
  a bare name among the loaded objects of its class; a class is held to its
  metaclass (an Actor class from `Engine.Texture` is None). A bool takes
  its first token, `1`, `True`, `0` or `False` (`True,` and a quoted
  `"True"` among them), an int the number its text starts with from a `-`
  or a digit (`+5` none), an enum its name alone, never a number, its text
  after it passed over, as the original's imports (`SweepVmConsole`). A float's text, a
  vector's and `string()`'s too, is the original's `%f`, its runtime's:
  every digit exact to six decimals, a tie rounding up, infinity `1.#INF00`
  (`OpsConsole`: 813.1953125 as `813.195313`, 2 to the 70th whole, alike in
  both engines).
- **A `GotoState` from within `EndState` or `BeginState`**
  (`PreemptConsole`): preempts the one under way, as the original's
  `RF_InEndState` and `RF_StateChanged` have it: the actor stays where the
  inner one took it, the outer one's state and label forgotten, and
  `EndState` is not sent again from within `EndState`; one to no state, or
  to a state the class has not, sets no `RF_StateChanged`, as the
  original's, and preempts nothing: the outer one goes on into its state,
  its `BeginState` and its label run (`NonePreemptConsole`, both kinds).
  The flag is cleared only before an `EndState` or `BeginState` whose probe
  is on, so a `GotoState` to no state from a state with no `EndState`,
  made in `BeginState` after one that ended in a state, leaves the outer
  one preempted (`Vm29Console`: no `Label not found` for the outer one's
  label, in both).
- **A None context** (`A.X` with `A` None; `NoneConsole`): the zero of
  `X`'s type, as the original's `execContext` zero-fills the result -- a
  `switch (A.Physics)` takes `case PHYS_None`, a struct copied through None
  is zeroed, and a native's optional argument given through None is its
  type's zero, not left out (`Trace(,,, A.Location)` starts at the origin).
  A string, of which the bytecode has it zero-fill no byte, and an array's
  element, of which the original copies nothing, leave a variable assigned
  from them as it was; a Let through None stores nothing.
- **A function that ends without a value** (`ResultConsole`), as any with
  one can by running off its end (UCC takes no bare `return;` in it), writes
  nothing, as the original's
  ([variables and calls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#variables-and-calls)):
  its `Let` has the call write into the variable itself, and the return of
  nothing every function ends with writes nothing. So a variable assigned
  from it keeps what it held, of every type, a struct's member and an
  array's element too, through a `return` of the call, after a foreach, in a
  state's function and in state code; a bool is cleared, the original's
  `LetBool` reading the value into a zeroed int of its own; whatever else
  takes it reads its type's zero, but a native's optional parameter, which
  keeps its default as though left out (a `Spawn` at the spawner's place, a
  `Trace` from the tracer's). A call of a probe's function while the probe
  is off writes nothing so too, and so does one a net game sends to the
  other side or does not run ([multiplayer](#multiplayer)). A `Let` has a
  script function's call write its value into the variable itself, then the
  call copies its `out` parameters back, the last first, as the original's
  does: so an `out` parameter that is also the variable assigned ends with
  the parameter's value, through a `return` of the call too, and of two
  `out` parameters given one variable the first's; a bool, which `LetBool`
  sets once the call is done, takes the value returned (read)
  (`ResultConsole`). The post-increment and post-decrement natives, int and
  byte, write their result into the variable assigned before they step
  their own, as the original's: so `I = I++` ends stepped, of a local, an
  instance variable and an array's element, through a context too
  (`ResultConsole`). Differs: a native's vector or struct parameter given
  such a result reads zero, where the original's reads what its stack held,
  by choice: no run can match that, and no stock script gives one such a
  result.
- **An actor spawned into water** keeps its physics, the spawn setting its
  region alone, as the original's does: an AirBubble, an Effects with
  PHYS_Projectile, rises as a swimming player's breath does
  (`BubbleConsole`: an AirBubble, an Effects and a Rat spawned under water,
  alike).
- **A spawn fitted into the world**, an actor that collides with it or
  when placed, is fitted where the original's `FindSpot` fits it and
  refused where it fits nowhere, the original refusing it without a word
  where DXEngine logs it: a broken window's pieces and a destroyed
  container's trash are refused at the same spots in both engines
  (`DebrisConsole`: of 128 directions round Liberty Island's
  `BreakableGlass0`, 111 placed alike, within 0.0001, and 17 refused in
  both; of 125 spots round 02_NYC_BatteryPark's `Trashbag1`, which falls
  out of the world at the map's start, 50 placed alike and 75 refused in
  both). A spawn's and a `SetLocation`'s encroachment asks what stands at
  the spot by its cylinder and never a mover, as the original's hash passes
  over a brush that is not static: a LAM, an EMP, a gas and a nano-virus
  grenade placed on Liberty Island's `DeusExMover21`, 2.5 units into its
  brush, and a `CapReacher` 5 units into it, are placed in both engines
  (`EncroachMoverConsole`).
- **The engine's own rotations** -- the view's, a mesh's and a mover's, a
  laser's beam, movement's and the AI's -- take the original's sine table,
  as the scripts' conversions do, the half turn's 1.2246468e-16 among them
  ([a rotator's turn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)):
  at 20 frames a second `FlightConsole`'s gull, flown level into the pier's
  wall, turns and slides 10.8 units along it in both engines, its track
  within 0.2 units of the original's.
- **A map's actors at rest** find their base at its start as the original's
  load finds it, a box of the actor's size traced 8 units down
  (`BaseConsole`: Liberty Island's 395 alike, a carcass's crossbow among
  them, its box 8.78 over the floor finding none).

Differs:

- **`DeusExPlayer.GetDeusExVersion`**: DXEngine's own string, by choice,
  so a player can tell which engine they run
  ([the version](ENGINE.md#running-on-our-devices)). The original's is "Mon
  Mar 19 12:06:14 2001 v1.112fm".
- **`DeusExPlayer.UnloadTexture`** frees nothing, by choice, the picture
  the same: the original frees the texture's mips, which the game asks for
  as a menu, a Persona screen, a computer screen or an image closes, and
  reads them from the package again when the texture is next drawn, where
  DXEngine keeps a texture's mips until nothing holds it and a collection
  frees it; the game unloads still textures alone.
- **The console's `showlog`** opens nothing: the original shows its log
  window, which DXEngine has not (the log says so); by choice.
- **Swimming out from under the water** (`SwimConsole`'s d: the player 100
  under the water line off the pier, looking 45 degrees down, the forward
  axis held): at 60 and 120 frames a second the two engines' players swim
  out alike, 4 units out at 0.2 s and 38 at 0.75 s. At some 1,000, either
  engine uncapped, the original's ticks each take its floor of 5 ms, which
  DXEngine does not carry ([small](#small)):
  its swimmer is 134 units out at 0.23 s, its velocity each tick the move
  rounded to the floats of its place (0.001 apart at y 9,920) over the
  5 ms, 5 such steps more each tick. DXEngine with that floor, in a test
  build, swims alike: the same velocity out and the same distance out each
  tick until its speed's cap, the same speed after, its first tick a little
  longer than 5 ms, 0.4 units further out from there. Without it,
  DXEngine's swimmer goes by the real time, 3.4 units out at 0.2 s and 37 at
  0.75 s, a float step each of its first ticks, whose moves are under one.

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
An actor with no physics runs none, its pending touch's `PostTouch` with it,
which waits for it to have some, and so does its `AutonomousPhysics`
(`PendingTouchConsole`: none with no physics, one once it has rotating
physics, in both engines).
Before each level tick the level's `bDropDetail` and `bAggressiveLOD` are
cleared, as the original's game engine clears them with a client: nothing
drops detail for the frame rate.

**Garbage** is collected as the original's map load collects it
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-map-loads-collection);
DXEngine's: [objects and memory](ENGINE.md#objects-and-memory)), the new
level's actors that are not in its `Actors` (an editor brush, a camera) with
it. `GarbageConsole` and `DeleteConsole` run both engines. Differs by choice:

- **When**: at the end of the frame of the load, once the new level has
  begun and the player is in, where the original's runs before the new
  level begins.
- **A level left behind** is eliminated: every reference to one of its
  objects is made None, where the original's keeps the objects a live
  reference reaches. Without it a conversation event's `speaker`, the game
  replication info's `PRIArray` or a HUD's `clientObject` keeps a whole
  level. No stock script reads such a reference before setting it again.
  The original's browse takes the level's mission from the first
  `DeusExLevelInfo` in its object list (DeusEx.dll `0x100120a0`), which a
  level kept so can be: with the menu map's `LevelInfo` held, its browses
  from Liberty Island and UNATCO HQ read the menu's -2 and empty `Current`
  as for a new mission, where DXEngine's read the level's own
  (`URLEdgesConsole` with `Short` and `Hold`).
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
taken by the next name made. A level's game, which the engine spawns, is
named so too: `DeusExGameInfo2` on Liberty Island opened from the menu map,
after Entry's and the menu's (`ReturnConsole`, `SaveConsole`). Differs by
choice: every other name -- from a package, an
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
then deletes them; DXEngine frees them at the next collection, by choice, which makes
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
DXEngine's flags it (`EliminateObject`), by choice: it goes at the next collection,
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
  shorter. A save's load likewise, the level's time going on from the time
  saved (`SleepConsole`: past the save's within a millisecond of the wall
  clock's step over the load in both engines, 2.87 s in the original's run
  and some 0.14 s in DXEngine's). Where the player then rests follows its
  landing, so that first frame's length: 4.25 over the floor at Liberty
  Island's start in both engines' runs, and after each walk alike
  (`StandConsole`, two runs of each); after the island's travel to UNATCO
  HQ, a frame a little under 0.4 s in DXEngine's runs, some 0.01 under the
  original's, whose actors get 0.4 (`SaveConsole` with `Via`: 607.74 and
  607.75). Not carried, by choice: the original's floor of 5 ms, the step
  of every tick past 200 frames a second, which would run the game fast
  there: uncapped, the original's flyby on the menu map sweeps between its
  logos in a tenth of a second, where held to 60 frames both engines' keep
  together (`IntroConsole`). The level's `TimeDilation` is held to 0.0025
  to 25, by choice, where the original's is not held; the game sets it only
  through `SetGameSpeed`, to 0.1 or more (`DilationConsole`: at 0, over half
  a second, the original's level time stands, its actors' ticks 5 ms each,
  0.155 s in all, and DXEngine's moves 0.000645 s, its actors' with it).
- **A travel's items**: the travel is taken on any login but a `?loadonly`
  one, whatever the travel, and whether or not the game's Login spawned a
  new pawn, as the original's SpawnPlayActor takes it (`URLConsole` with
  `Absolute`: an absolute travel's items arrive in both engines); its info is
  looked up by the URL's name in any case, as the original's, then, by
  choice, by its first 20 characters, the stock Login's cut of the name the
  info is kept by, which the original's never tries, so a player of a
  longer name loses its items at a net game's travel there; standalone with
  none found the first entry is taken; a
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
- **Input axes**, read as the original's input reads them
  ([a player's input](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)):
  in the player's tick, before `PlayerInput`, every `input` float a script
  left since the last read is scaled by 20 over the tick, and the keys held
  and the mouse add theirs; after `PlayerTick`, and after `PlayerInput`
  while paused, every one is 0 again. So a script's own axis walks the
  player as the original's does (`AxisConsole`: an `aBaseY` of 1 set from
  the console's tick, at 60 frames, accelerates the player at 480, its
  speed climbing 8 a tick, in both engines).
- **A key's binding**, as the original's
  ([a key's binding](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)):
  a press runs its line once per hold, a release only after its press. The
  line, and an alias's, is cut into commands as the original's `ParseLine`
  cuts one (Core.dll `0x10147e60`): at a `|` outside double quotes, a `//`
  outside them dropping the rest of its line, a `|` in it cutting all the
  same, 255 characters a command, a longer line's rest the next. A press's
  commands go to the input's own words and its aliases, the render device's
  and the viewport's own words (`SHOT`, `EXEC` and the rest), then the
  console's exec functions, then the player's -- its level's words, the exec
  functions of the game, the HUD, the player and each of its items in turn
  --, then the game engine's commands, the window's words last, each word as
  the original's `ParseCommand` takes it (`0x10147a20`), the command's leading
  letters and digits in any case, so `exec"x"` is `EXEC` (`ExecLineConsole`,
  an exec file's lines cut alike: every mark the original's); a typed or
  scripted command goes the same way, so `DefUser.ini`'s `GreyStar=Shot`
  takes a shot, and `TIMEDEMO` is the console's own, whose result its script
  logs. A release runs the input's words alone, and a command after
  `OnRelease`, the word as `ParseCommand` takes it, the press's way.
  `Button` sets its byte to 1 on the press and to 0 on the release, so a
  byte a script clears while the key is held stays clear until the next
  press (Deus Ex's player clears `bDuck` as it enters water); `Pulse` sets
  it on the press alone; `Toggle` flips its top bit on the press; an alias's
  line runs with its key's press or release, an alias inside it not expanded
  again (`BindConsole` with
  `tools/dxcap/timelines/bind-keys.txt`: a toggle's byte 128 then 0, a
  pulse's left at 1, a button's 1 then 0 with its press's command and its
  release's, a key pressed three times over run once, `Shot` the viewport's
  and not the console's, `ShowPath` the console's before the player's; the
  original's input takes no scripted key on this desktop, and its code is
  read; the timedemo's result in both engines). Each held key's line runs
  again at each read as held, its input words alone. An `Axis` binding's
  `speed=` word is taken in any case, and 1 without one, as the original's
  parse has it (`DefUser.ini`'s `JoyU=Axis aturn speed=5.9`; `PadConsole`
  with `pad.txt`). A `Button` naming a property the pawn lacks, or an
  `Axis` naming no float of it, is the original's "Bad Button command" or
  "Bad Axis command", logged and left alone, on the press, at each read
  while the key is held and on the release, as the original's input runs
  the line each time (a User.ini's `F=Button bFirre` and
  `G=Axis aNope Speed=1` pressed by `tools/dxcap/timelines/bad-keys.txt`).
  Keys held on one axis add, as the original's held bindings add into its
  float: W and S held stand still, and W walks on once S is let go. Key and
  alias names are matched in any case, as the original's names are: the
  console's `set input w Jump` binds W (`PadConsole` with `axis-keys.txt`).
  A command of the engine's that fails from a key, an exec file or the
  launcher (a `SHOT` whose file cannot be written) is logged and the game
  goes on, as one typed in the console is.
- **A key's name** from the SDL2 and SDL3 windows, the launchers', is the
  original's
  ([the keys](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-keys)):
  a shift, control or alt its name for both sides alone, as a tap in the
  original's window (`SideKeyConsole` beside `tools/dxcap/sidekeys.py`, each
  tap `Shift`, `Ctrl` or `Alt` alone in both engines; by choice its side's
  own, `LShift`, `UnknownA4` for the left alt, which the
  original's poll presses for one held at an input reset, a map's load, a
  join or the focus, never pressed), Caps Lock, the punctuation, the
  keypad's operators and Enter, Print Screen (`PrintScrn`), the Windows and
  menu keys (`Unknown5B` to `Unknown5D`), the ISO key by the left Shift
  (`JoyZ`, Windows' `VK_OEM_102`); with Num Lock off the keypad's digits and
  point are the keys they move by (`Up`, `Insert`, `Delete`; 5 `Unknown0C`,
  `VK_CLEAR`). A key that types follows the layout: on Windows the code
  Windows' layout gives its scancode, on Linux the name Wine's X11 driver,
  the original's there, gives it -- the layout's family told by what a few
  keys type, a QWERTY's keys by their places, a QWERTZ's, an AZERTY's, a
  Dvorak's, a Swedish's (and a Finnish's), a Brazilian ABNT's and a
  Japanese 106's by its family's table, any other layout's by their places.
  A key is released as the name it was pressed as.
  `tools/dxcap/timelines/keys.txt` with `DXCAP_BACKEND=SDL3` or `SDL2`
  presses each through the window's own events, the keypad with Num Lock on
  and off, the typing keys under each family (`keylayout`), and logs the
  names the game gets (the original's input takes no scripted key on this
  desktop, and its code is read). Enter with Alt held
  toggles full screen, the game's input getting no press of it, and Alt+F4
  closes nothing, as the original's window takes them (`PadConsole` with
  `axis-keys.txt`: the root window full screen's size and back). On Windows
  Alt+Space presses Space and, in a window while the game takes no typed
  text, opens the window's menu, as the original's window leaves Windows a
  character typed with Alt that its game does not take
  ([the keys](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-keys)):
  none opens in a menu or with the console typing (a Windows build under
  Wine: Alt+Space's X key events to the game's window in play open Windows'
  window menu, where none opened before; `alt-space.txt`: Alt+Space typed
  into the console its space, F4 with Alt pressed and nothing closed). By choice: no other character typed with Alt
  reaches Windows, where the original's Alt+Enter and a letter its game
  leaves do, and Windows beeps; full screen the window has no menu, where
  the original's keeps its own and opens it over the game; Alt+Tab,
  Alt+Esc, Ctrl+Esc and Ctrl+Tab reach the desktop in full screen, where the
  original's takes them as its own hot keys and drops them; on Linux
  Alt+Space is left to the desktop, where the original's under Wine opens
  Wine's window menu if the desktop leaves it the key. By choice,
  the other windows name fewer, and a side's own
  never: the X11 one, the
  harness's, has no `` ` ``, no keypad Enter, and Print Screen as `Print`;
  the Wayland one, the engine's own on a Wayland desktop (a dev build's, no
  port's), names each key by its first level, the key below Escape `` ` ``
  whatever the layout, and lacks only the Windows and menu keys. The X11
  window presses `Alt` for either Alt key, AltGr's too, as the original's
  window presses `VK_MENU`, so the stock `Alt=Strafe` strafes
  (`SideKeyConsole` beside `sidekeys.py`: each Alt tap `IK_Alt` pressed
  and released, as in the original).
- **Maximize** -- the title bar's button, a double click on the title bar,
  the window menu's -- toggles full screen in the game's window, as
  Alt+Enter does, as the original's window takes the command
  ([the window's commands](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-windows-commands)):
  full screen at `FullscreenViewportX` by `Y`, back at the window's size,
  the window never left maximized (`PadConsole`, the window maximized from
  outside through the window manager: DXEngine's X11 and SDL3 windows and a
  Windows build's under Wine full screen, the root window at full screen's
  size, and toggled back at the window's size, where each was maximized; the
  original's log the maximize's `WM_SYSCOMMAND` calling `ToggleFullscreen`,
  `AttemptFullscreen`, its OpenGL renderer then failing to start again
  under Wine). On Windows the window takes it before Windows maximizes the
  window, as the original's; on Linux the window manager maximizes the
  window first, as it does the original's under Wine before Wine passes the
  command on, and the window is restored as it goes full screen. The Fatal
  Error window maximizes. Full screen the game's window has no menu to ask
  it, by choice (above), where the original's menu's Maximize goes back to
  a window. Each toggle logs `AttemptFullscreen` or `EndFullscreen`, as the
  original's (`PadConsole` with `maximize-back.txt`: both lines in
  DXEngine's log; the original's `AttemptFullscreen` as `maximize.py` takes
  it full screen). The console's `ENDFULLSCREEN` is taken in any state,
  after the exec functions and the engine's words, as the original's window
  takes it
  ([the window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-window)):
  `EndFullscreen` logged; full screen, the toggle's way back to a window;
  in a window, the window as it is, where the original's is sized again at
  its windowed size, the window's own (`EndFullscreenConsole` in a 1280x720
  window, full screen 2560x1440: `EndFullscreen`, `AttemptFullscreen` and
  `EndFullscreen` logged in that order in both engines, the window as it
  was after the first `endfullscreen` and back at its size after the
  second; the root's own size each engine's, 4:3 in DXEngine's). No stock
  script sends it. The window's own words -- `togglefullscreen`,
  `endfullscreen`, `getcurrentres`, `getcurrentcolordepth`,
  `getcolordepths`, `setres` -- are taken after the exec functions and the
  engine's words, as the original's window takes them, and `getres` before
  the exec functions, as the original's `D3DDrv` takes it
  ([a key's binding](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)):
  an exec function of the name runs in their place, the window left as it
  is (`WordOrderConsole`, the original through `D3DDrv`: every word's
  exec function called in both engines but `GetRes`, which neither calls,
  `getres` giving the device's sizes, the window not toggled, the root
  1280x720 before and after in the original's, 960x720 in DXEngine's, where
  DXEngine's window had taken the words and gone full screen). No stock
  exec function shares one.
  The render device's words are taken before the exec functions, as the
  original's device takes them
  ([a key's binding](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-keys-binding)):
  `GETDETAILTEXTURES` answers the device's `DetailTextures`, 1 or 0;
  `TOGGLEDETAILTEXTURES` flips it and saves the device's config, saying
  nothing; `LODBIAS <n>` says `Texture LodBias <n>` and sets the textures'
  mip bias from the next frame, neither held nor saved, until the device's
  mode is set again -- full screen or back, `SETRES`, the window's new
  size --, which puts the launcher's back, as the original's mode puts its
  -0.5 back; `SHOWPOOLS` says `Dumping D3D Texture Pool Info` alone, by
  choice, DXEngine having no Direct3D 7 texture pools to list under it.
  `BRIGHTNESS`, an engine word, steps the brightness a tenth up, from 0.9
  back to 0, flushes the render device and saves the client's config, and
  sends the player `Brightness level <n>/10`, as the original's client
  does (`UClient::Exec`, Engine.dll `0x10375966`; `RenderWordsConsole`, the original through `D3DDrv` and DXEngine on
  each device: the same answers in both, the planks before the view losing
  their detail under the bias and getting it back after full screen and
  back, the brightness 0.6, 0.7, 0.8; where the build before took none of
  the words). On Vulkan the bias is held within the GPU's
  `maxSamplerLodBias`, which no sampler may pass.
- **The window's least size** is the original's 640x480 as it is dragged
  ([the window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-window)):
  on Windows the whole window, its frame too, as the original's; on Linux,
  where the desktop draws the frame, the window's own area, by choice
  (`maximize.py --size 320x240` beside a run: DXEngine's X11, SDL3 and SDL2
  windows held at 640x480, its Windows build under Wine at 632x446 as the
  original's, where they went to 320x240). A size the window is given in
  a window, a drag of its frame or the window manager's, is the client's
  windowed size, its width made even -- one more where it is odd, the
  window sized to it --, saved to the ini at once with the client's config,
  as the original's client keeps it
  ([the window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-window)):
  so the way back from full screen and the next start come at it.
  DXEngine keeps it once the window has held its size for a tick, where
  the original keeps every size of a drag; the same at the drag's end
  (`WindowSizeConsole` beside `maximize.py --size 1001x701`: the
  original's window and DXEngine's X11, SDL3 and SDL2 windows each at
  1002x701 after the drag and after the way back from full screen, and
  1002x701 in the ini after a kill, where DXEngine's went back to
  1280x720). A size of nothing, a
  window minimized, is not kept, as the original's.
- **The mouse held in play** by the Win32 window, as the original's viewport
  holds it while its window has the focus
  ([the focus](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-keys)):
  the pointer hidden, put in the client area's middle, captured and clipped
  to the client rectangle, the clip following the window as it moves or goes
  full screen; let go and put back where it was as the focus goes, and taken
  again as it comes back (`wincheck.exe clip` beside `WheelConsole` in play
  on Liberty Island, a Windows build's Win32 window under Wine: the clip the
  client rectangle, (4, 30) to (1284, 750), and the capture the window's in
  both engines, where the build before left the whole screen and captured
  nothing). While the window's menu is open (Alt+Space) the window takes no
  mouse move, as the original's, so the view does not turn by the moves made
  in it as it closes (by reading: WinDrv.dll `0x1110b626`, `0x1110b70c`,
  `0x1110b351`). A wheel message of no delta presses nothing, as the
  original's (`wincheck.exe wheel` of 0 and then of 120 beside
  `WheelConsole`: nothing, then `MouseW` 120 and `MouseWheelUp`, in both
  engines, where the build before pressed `MouseWheelUp` for each). The
  screen saver and the display's power-down are refused in a window and full
  screen, each logged as the original's logs it, `Received SC_SCREENSAVE` and
  `Received SC_MONITORPOWER` (`wincheck.exe syscmd`: the lines in both
  engines' logs, by the Win32 window and by SDL3's on Windows, none in the
  build before's). By choice, the Win32 window keeps the display on while it
  is open, as SDL3's does, and takes a mouse button's release while its menu
  is open, where the original's drops it. The windowed sizes Screen
  Resolution offers are those of the display the window is on, by that
  display's own DPI, on a desktop of mixed DPI too (by reading).
- **The field of view** is the original's setting: the player's `DefaultFOV`
  is Deus Ex's own config, 75 (`[Engine.PlayerPawn]` in `User.ini`), the
  angle across a 4:3 view ([the view's width](#the-views-width)).
- **A singular function** does not run while any singular function of its
  object does, as the original's
  ([variables and calls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#variables-and-calls)):
  the game has 32, a trigger's `Touch`, a pawn's or a decoration's
  `BaseChange` and `SupportActor` among them (`CallConsole`: one calling
  itself, its object's others directly and through a function that is not
  singular, and its object touched while it runs, all held off; another
  object's run). A call held off gives nothing: a variable assigned from it
  keeps what it held, as the original's Let has the call write into it, a
  bool is cleared, as its `LetBool` reads into a zeroed int of its own, and
  an expression reads it as zero (`CallConsole`: `Answer()` held off into 5
  leaves 5, and tests as 0, in both; `ResultConsole`: a bool cleared).
- **The windows' icon** is the original's, `DeusEx.exe`'s icon group 128
  ([the frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#the-frame)):
  the original's window under Proton and
  [DXEngine's](ENGINE.md#running-on-our-devices) game and error windows,
  through X11 and SDL3, give the window manager its 32x32 of 256 colours
  (their `_NET_WM_ICON`).

An exec function's parameters are taken as the original's: each by its
property's import from where the last one stopped, a class or an object by
its name, a refused one Core's BadProperty as the command's output and no
call made, a missing optional one zero (`ExecParmConsole`: with cheats on
`killall Terrorist` kills, `behindview True` turns it on, a bare
`behindview` is refused, `augadd AugSpeed` gives it, alike in both
engines).

Each argument and operand is taken as the original takes it, as it is
reached: a variable's value before a later argument runs, and an `out`
argument the variable itself, which a native reads and writes there, and
which a script function's parameter, starting with its value as it was
reached, is copied back to after the call; a struct `==` and a `switch`
condition alike (`SweepVmConsole`, both ticks, alike in both engines:
`I + SweepBump(I)` 1, `Pair` 1,0, `Text $ AppendText()` ab,
`I - SweepBump(I)` and `I - byte(SweepBump(I))` 1, `AddInto(I,
SweepBump(I))` 1, `I += SweepBump(I)` 10, `A == BumpPair(A)` True, a
`switch` taking `case 1`). A static array is taken whole as it is reached,
and an `out` one copied back whole; a call held off -- a singular one, a
disabled probe's, one sent to the other side of a net game -- copies its
`out` arguments back from its frame all the same, as the original's
`CallFunction` (Core.dll `0x1013f4e0`) does (`Vm29Console`, alike in both
engines: `FirstOf(Arr, BumpArr(Arr))` 1, an `out` array set to 7,8,9 read
back 7,8,9, `HeldSingular(I, SweepBump(I))` held off leaving `I` 1, a
disabled `Probe4`'s the same).

A string constant is the original's: its 8-bit text a byte a character, at
most 1,023, as `execStringConst` widens it (Core.dll `0x10130890`), so bytes
that read as UTF-8 are their own characters (`OpsConsole`: C3 A9 t C2 A9 as
195, 169, 116, 194, 169 in both). A cast and every iterator take the very
class, a mod's class of another's name not it, and `IsA` a class by its name
(`CastConsole`: `DXCapTwin.u`'s `CapMarker` cast and iterated as none of
`DXCapture.u`'s, its `IsA('CapMarker')` true, in both engines).

A name keeps the spelling it was first made in, every spelling of it after
answered with that one, as the original's name table answers them
(`FName::FName`, Core.dll `0x1014b950`): the names the original makes at its
start in its spelling (`FName::StaticInit`), then each package's as it
loads, so `string()` gives DeusEx.u's `annadies` for `ANNADIES` and Core's
`Location` for `LOCATION`, and the cheats' EditFlags lists `COOLANTCUT` as
Mission15's `coolantcut` (`SpellingConsole`, line for line the original's);
a package's file, a `.int`, a save's level file and a class's config file
are found in any case, as on Windows, and make no name. A name a package
holds for neither the client nor the server, a map's for the editor alone,
is None and makes no name, as the original's linker maps it (Core.dll
`0x10160543`), and an export for neither is not made: so
06_HongKong_WanChai_Garage's `Barricade` is spelt so after
02_NYC_BatteryPark, whose editor's brushes are grouped `BArricade`
(`NameFirstConsole`, alike in both engines). A dedicated server's context
is the server's alone, so there one for the client alone is not made
either, where a game with a client makes it: a single-player level keeps
a client-only actor, which no package of the game's has
(`ClientOnlyConsole` and `ClientOnlyServer` on a copy of Liberty Island
whose `CrateUnbreakableSmall0` is client-only, `tools/dxcap/clientonly.py`:
there in single player, not on a dedicated server, in both engines). A
package's names are its characters, a byte each or, saved 16-bit, two, as
the original's reads them (`0x101459b0`), and a name's text is the same
characters whether a package, a file name, a URL, the ini, a script's
string or `StringToName` made it, so `string()` and `GetPropertyText` give
them as the original's do.
A package is an object of Core's class `Package`, the `Outer` of each of its
objects that has none of its own, as the original's: `new(None)`'s the
package `Transient`, a level's and its actors' their map's, a texture in no
group its package's (`Vm29Console`, `NewOuterConsole`, `ConObjectsConsole`:
every `Outer` alike in both engines; each engine's save loads in the other).

A move of nothing is the original's
([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)):
one whose every axis is under the double 0.0001 as the x87 compares it, a
float's 0.0001 and a NaN under it too, is none, however long -- a script's
`Move` and `MoveSmooth`, a mover's, a walking step, a roll, a trailer's
turn, a thrown thing's flight, the reach tests' moves and a net client's
smoothing alike (`NothingMoveConsole`: 0.0001 along X and Y moved by
neither engine, a mover's 0.0001 none, a trailer of an owner moving 0.0001
turned straight up, in both engines). A direction is the original's
`SafeNormal`'s wherever its physics, sight, reach tests and net priority
take one: none for a vector under 0.0001 long (1e-8 squared), a NaN's
too, else each axis over the length as the x87 works it out. So a walk, a
flight or a swim toward an acceleration that short turns toward none and
only slows, the acceleration itself taken where each axis is 0; and a
slide into the corner of two walls all but facing goes nowhere
(`SafeNormalConsole` at 60 frames a second: walkers and fliers heading
out at 200 units/s, an acceleration 0.00009 long to their left turning
neither in either engine -- under 0.1 to the left each tenth of a second,
a walker along its heading 73, 31, 13, 5.6 and 2.4, a flier 151, 118, 93,
73 and 57 --, one 0.00011 long turning both, to 105 to the left walking
and 38 flying; DXEngine's before turning the first as the second). A life span ends when what is
left, the tick taken from it as the x87 holds it, is 0.0001 or less to
the double's compare, the float stored after; with the original's ticks
of 5 ms or more no test of the float tells the two apart, so no run does
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)).
A pawn in `PHYS_Spider`, which no stock script sets, climbs as the
original's `physSpider` (Engine.dll `0x103d6370`): along its `Floor`, the
normal of the face it holds to, looked for again where none is beyond it
(a box of its size `MaxStepHeight` down, then along -Y, +Y, -X and +X),
turned to stand on it, let fall from a face steeper than 0.6 that its
acceleration points out of; a step's hit is its new floor, a ceiling a
wall (`SpiderConsole` on UNATCO HQ: the climber up the wall
stopped by the ceiling at 714.30, its floor the wall's, the one let go
falling and walking off at 21.7 a second, the walker taking the wall it
met, each alike in both engines to a hundredth but the walk-off's speed,
where none moved before). `PickWallAdjust`, which no
stock script calls, is the original's (`0x103c4150`): a jump, or a step
aside, up or down that becomes the `Destination`, each tested against the
world and movers (`PickWallConsole` on UNATCO HQ with `Reach=1500`: a
walker before a tall face refused, a flier sent over it to the same
`Destination` and refused under it, in both engines, where DXEngine's
walker had jumped).

The renderer's words are the original's (`URender::Exec`, Render.dll
`0x10b147b0`): `STAT` toggles its panels a word in their order, `stat fps`
the line 10 pixels over the view's bottom -- the frame rate, the frame's
and the render's times, each the mean of the last 32 frames, the surfaces
drawn as its nodes and polygons, the textures the frame used and their size
as the original counts them --; `MLOD`, `MLMODE` and `MLFIX` set the shape
LOD, its mode and its fixed factor, which the mesh detail takes, `TLOD` a
distance nothing draws by, each answered as the original's (`MLOD` alone
0.250000), and `REND LEAK` and `REND T` say they are recognized
(`ViewWordsConsole`: the line after `STAT FPS` in both engines). Differs
(bugs.md WIN.29): of the panels the line alone is drawn; the others list the
original's own timings, connection counts and caches. By choice, `STAT
RENDER` toggles a panel of DXEngine's own at the screen's right, where the
original's toggles nothing: the frame rate, the level's actors, the objects
the garbage collector holds and the memory they take. Differs (bugs.md
WIN.30): no demo is recorded or played: `DEMOREC` and `DEMOPLAY` ask for a
name as the original's do, and a name's recording or playback fails, said so.

The engine's smaller words are the original's, none logged as unknown
(`EngineWordsConsole` from the menu map; `ViewWordsConsole`): `MEMSTAT`'s
two lines of the memory available, as a 32-bit process sees it; `EXIT`'s
"Closing by request"; `GET` and `SET` of a class not there, "Unrecognized
class", and of a name the class has not, "Unrecognized property"; `GTIME`'s
line to the log; `MEM`, whose allocator dump the game's build leaves out;
`OBJ MARK` and `MARKCHECK` to the log, `OBJ CLASSES`'s tree and `OBJ LIST`'s
tables, of DXEngine's own objects; `CRACKURL`'s parts of a URL, its `String`
line too, where the original's faults there, and "BAD URL"; a server's or
client's `SOCKETS`; the viewport's `SHOWALL`, `SHOWACTORS`, `HIDEACTORS`,
`INJECT` and `REPORT`, the report into the clipboard; a dedicated server's
`OPEN`, `START`, `DISCONNECT` and `RECONNECT`, which browse with no
`PreTravel`, at the frame's travel. `HIDEACTORS` takes every actor out of
the frame as the original's render takes them while the player's
`ShowFlags` lacks `SHOW_Actors` (Render.dll SetupDynamics `0x10b22dd0`):
no sprite, mesh or render iterator drawn, no moving light lighting the world
or its fog, no mover filed again, the movers filed drawn where they were,
the level's own polygons as ever (`ViewWordsConsole` on Liberty Island: the
statue, the bottles and the bin gone from the frame in both engines, the
build before drawing them). Differs (bugs.md WIN.31): `CONFIGHASH`,
`DUMPNATIVES` and `OBJ`'s `REFS`, `HASH`, `DEPENDENCIES`, `VFHASH` and
`LINKERS`, which list the original's own tables, are taken by nothing;
`DUMPCACHE` lists nothing, DXEngine having no memory cache; `OBJ LIST`'s
bytes are each class's properties', where the original's are what its
serializer walks of each object. Differs (bugs.md WIN.38): Galaxy's words
(`MUSICORDER`, `ASTAT`, `CDTRACK`, `CDVOLUME`, `RECORDSOUND`,
`ENDRECORDSOUND`) and the original launcher's `EDITACTOR` are taken by
nothing. By choice, `OBJ GARBAGE` collects at the frame's end, the
collector running with no script under way, where the original's collects at
once.

## The command line

The original's command line reaches DXEngine as one string, `--cmdline=`,
which main's launcher and DXLauncher pass on as given
([running it](ENGINE.md#running-it)).
Its flags are found as the original finds them
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)),
an `X=` value as its `Parse` takes it: in double quotes to the closing
one, else to a space, tab, CR, LF or comma, no more of it than the
original's buffer holds (255 characters for `INI=`, `USERINI=` and
`MULTIHOME=`, 4,095 for `EXEC=`), and `PORT=`'s number from the text
straight after it. There is one exception, by choice: a flag's name (`-X`
or `/X`) must end at a space, a `?` (a URL's next option, where the Host
screen's [`RELAUNCH`](#the-menus-settings) puts `-server`) or the line's
end, where the original's `ParseParam` checks nothing after it. Both take
`/` as a switch, and a Linux path is full of them: the original's would
find `-server` in `INI=/srv/server/x.ini`. The launchers parse alike
([where it differs](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#where-it-differs-from-the-original)).
What DXEngine does with each:

- **The start URL**: with `-hax0r` or `-server`, the first word (the second
  after `SERVER`) unless it starts with `-`; else the game's own start,
  `DX.dx`, whatever the ini's `[URL] LocalMap` names
  ([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine);
  `LocalMapConsole` with `LocalMap=01_NYC_UNATCOIsland.dx`: `DX.dx` in both
  engines).
  `?loadgame=N` there, or as `--url`, starts a client on slot N's save,
  loaded before any map but Entry, as the original's first browse loads it
  (`StartConsole`, the original's with `DXCAP_START`), whatever map it
  names; a slot with none starts at `DX.dx`. A start URL that does not
  read is fatal, `Invalid URL: <url>`, as the original's. A start whose
  browse fails with no error's text -- a server's address, its join begun,
  a URL of another protocol, a slot with no save or whose level does not
  load -- is browsed once more at `DX.dx` on the player's options, unless
  the URL was `DX.dx`, as the original's Init does, so the menu's map plays
  meanwhile; one that fails with a text, or fails again, is fatal,
  `Failed to enter <url>: <error>`, a dedicated server's of a server's
  address with `Servers can't open network URLs`
  ([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)).
  A start map that is not there is followed by `Dx.Dx` on the same URL, as
  the original's first browse loads it, having no level whose map to load
  again (`LocalMapConsole`, DXEngine's map and the original's `DXCAP_START`:
  `NoSuchMap.dx` starts on `Dx.Dx`, `?loadgame=9990` and `127.0.0.1:7799` on
  `DX.dx`, each line alike in both engines). So is a start map whose file
  is not a package, and a slot whose level's file is not, as the
  original's map load checks the package before anything of the level goes
  (`LOAD_Verify`): `The file '<file>' contains unrecognizable data`, then
  `Failed to load '<map>': Aborted` and `Failed to load 'Level
  None.MyLevel': Aborted`, the slot's start browsing `DX.dx`; and so is a
  start map with no `MyLevel` or importing a package that is not there
  (above, [saving](#saving-loading-and-travel)); a
  file cut short within its header is fatal in both, as the original's
  reader stops there (`LocalMapConsole` with `tools/dxcap/badpackage.py`'s
  files: `BadMap.dx` starting on `Dx.Dx`, `?loadgame=9` on `DX.dx`, each
  logged alike, and the file cut at 40 bytes fatal, in both engines, where
  DXEngine's start had ended at once). The original's start then fails
  in `DeusExRootWindow.InitWindow`, `GlobalSetProperty` critical, a crash
  DXEngine does not copy. The start
  empties `Current`, as the original's first browse does from the Entry
  level, which holds no `DeusExLevelInfo` and so is in no mission: no level
  an earlier run left there comes back (`RevisitConsole` begun on UNATCO HQ
  twice, the original's with `DXCAP_START`: the second run's island from
  `Maps` in both engines, its crate alone).
  Differs by choice: the player is the one saved, as the game's own load
  gives it (`LoadConsole`), where the original's start logs the player in
  with the travel info the level was saved with -- on a map the player
  travelled to, the inventory, skills, themes and key ring it arrived
  with, made anew, the saved ones left in the level, held by no one.
- **`-server`**: a dedicated server, as `--server`.
- **`-lanplay`**: a dedicated server ticking at `LanServerMaxTickRate`, as
  `--lanplay`
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)).
- **`-alladmin`**: every player an admin, a server's clients too, as the
  original's `SpawnPlayActor` makes them ([multiplayer](#multiplayer)).
- **`PORT=`, `MULTIHOME=` and `-PRIMARYNET`**: a server's port, and the
  address a net socket binds, as the original's net driver takes them
  ([multiplayer](#multiplayer)).
- **`INI=` and `USERINI=`**: the inis read and written back, as `--ini` and
  `--userini`, each found by its own name, never `INI=` within `USERINI=`
  (`DXCAP_USERINI_FIRST`: the two given `USERINI=` first, `MidConsole`'s
  run alike in both engines). One not there yet is empty, as the original's
  config cache takes it, and written whole at its first save; a missing
  `Default.ini` or `DefUser.ini` the same. Each is a path from `System`, its
  folders and its name found in any case, as the original's file system
  finds them (`DXCAP_INI_CASE`: `MidConsole` with both named in capitals,
  its lines alike in both engines). One given empty is none, the game's
  own, by choice: the original's names the file named empty, which no file
  is, so an empty `INI=` stops it at start (`Can't find
  'ini:Engine.Engine.GameEngine' in configuration file`) and an empty
  `USERINI=` plays with no bindings and saves nothing
  ([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)).
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in,
  the console's `exec`, as the original's runs it: its file the first token
  past the word, as the original's `ParseToken` takes it -- a quoted one
  whole, 63 characters at most, none running nothing -- found in any case,
  its text read as the original reads a text file, and its lines run as
  commands as its `ExecMacro` cuts them by `ParseLine` (Engine.dll
  `0x10374700`): at a `|` outside double quotes, a `//` outside them
  dropping the rest of its line, 255 characters a command, a longer line's
  rest the next (`DXCAP_FLAGS="EXEC=ENG19.txt,x"`: `Execing ENG19.txt` in
  both engines, `MidConsole`'s lines alike; `ExecFileConsole`, with
  `DXCAP_FLAGS` naming its long file: the same files run in both engines;
  `ExecLineConsole`'s `PKG27.txt`, its comments, chains, quotes and a
  264-character line: every mark alike in both engines).
- **Safe mode's flags**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music. The original makes no audio
    subsystem; DXEngine keeps its device, on OpenAL Soft's null driver, by
    choice. The game engine's `UseSound=False` (`[DeusEx.DeusExGameEngine]`)
    does the same, as the original's `InitAudio` takes it (`MusicConsole`
    with `DXCAP_INI=DeusEx.DeusExGameEngine:UseSound=False`: silent
    throughout in both engines).
  - `-defaultres`: the window and fullscreen sizes 640x480 for the run, not
    saved; differs by choice, the original's window keeping 640x480 in the
    ini at its first resize
    ([the flags](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/windrv-dll.md#the-flags)).
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

An `INI=` name with no extension is one file, by choice, where the
original's is two, a defect that sends the launcher's choices where the
game does not read them: its config cache adds `.ini` to a name of fewer
than five characters, or with no `.` fourth or fifth from its end, before it
takes the system ini's name as given, so its class configs and `ini:` names
read `MyMod` and the reads that name no file -- the `[URL]` section,
`GetConfig`, the launcher's keys -- `MyMod.ini`
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)).

The window's words are the original's: `GETCOLORDEPTHS` answers "16 32",
`GETCURRENTRENDERDEVICE` the device class the ini names (its
`WindowedRenderDevice` with `-nohard`), and `EXEC` of a file not there
Core's "Can't find file" to the caller (`WindowWordsConsole`, D3D: every line
alike in both engines). Differs (bugs.md D.81): `PREFERENCES` opens
nothing: the original's opens its Advanced Options window, out of full
screen, which DXEngine has not (the log says so). By choice:
`GETCURRENTCOLORDEPTH` answers 32, the depth DXEngine draws in, where the
original's answers the viewport's, 16 full screen at the stock
`FullscreenColorBits`; `-nommx`, `-nokni` and `-nok6` do nothing, where
the original's clear the processor's features for Core and Galaxy, whose
mix without MMX takes its plain routines, 64 levels of gain and one
interleaved buffer; the mouse is held in relative mode throughout, where
the original's lets it go while the game is paused, a menu is up or the
system's pointer shows; the hat's codes 0xF0 to 0xF3 are named `JoyPovUp`
to `JoyPovRight`, the original's `UnknownF0` to `UnknownF3`, for the
launcher's pad preset.

## Mods

A mod gets the original's loader and natives:

- **Its textures**: Deus Ex's formats (`TEXF_P8` to `TEXF_RGBA8`), drawn
  from the compressed mips when they are DXT1, as the original's
  `UTexture::Lock` picks them; a format past those, which no Deus Ex
  package has (a UT 469 one), is drawn white, with a line in the log. By
  choice, the original crashing there: `D3DDrv` draws P8, RGBA7 and DXT1
  alone, and any other format, `RGB16`, `RGB8` and `RGBA8` too, stops the
  game with a fatal error
  ([textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)).
- **Its folders.** Its packages and maps are found by the `Paths` of its own
  ini (`--ini`), relative to `System` and written with backslashes as the
  game's are, their folders and extensions in any case and a map's name's
  trailing spaces and dots dropped, as the original's on Windows; so are the
  game folder's own `System` and `Maps` and the `SavePath` and `CachePath`
  folders, as a copy recased on Linux leaves them
  ([a package's
  file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
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
- **A mod's packages and saves.** A package's or a save's property tag goes
  to the class's own property of its name before its supers', as the
  original's loader finds it; one of a name no property has, of another
  type, of an element past its static array, of another struct, or of a
  native or transient property, is logged and skipped, the object loading
  on, as that loader skips it, in its order and words, then `Skipping %i
  bytes of type %i`, an `XLevel` tag's without a line: so every save's load
  logs `Type mismatch in Region of GameReplicationInfo: file 10, class 2`,
  a GameReplicationInfo's tag of `Actor`'s struct `Region` meeting the
  class's own int `Region`, and the loaded GameReplicationInfo's `Region`
  as an Actor is its class's default, in no zone (`LoadConsole`, either
  engine's save, alike in both engines); a
  struct's static-array members are read every element, its native and
  transient members passed over on disk, and a struct extending another is its
  own members, then those of the struct it extends, as the original's binary
  has them: a save's `SimAnim` and `SimBlendAnim`, Planes, are W, X, Y, Z in
  either engine's (`SaveEdgeConsole`: the level's two set to 1, 2, 3, 4 and 5,
  6, 7, 8 load so from either engine's save); a map's or a save's BSP nodes
  lose `NF_IsNew`, `NF_IsFront` and `NF_IsBack` at the load, which play sets
  and ten of the game's maps and the original's saves carry, and its surfaces
  and brush polygons the editor's `PF_Highlighted`, as the original's load
  clears them; a tag's element index past 16,383 is written in four bytes, as
  the original writes it; a tag of exactly 65,536 bytes is written with its
  size, by choice, where the original's writer gives it the size 0, which
  no load reads back
  ([packages](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)),
  no stock property being that large; a script's Unicode string constant of more than
  one character loads on the Linux ports; an expression 1,024 deep loads
  (`DepthConsole`'s 100); a mesh whose faces run past its vertices or wedges
  loads, those faces passed over; a mesh DXEngine saves loads back; a palette short of 256 colours is
  padded with black; an uncompressed mip shorter than its size is padded
  with black, a compressed one not: on every device a texture whose top
  mip is short of its bytes draws white, and on the GPU devices a short
  mip below it draws black, none read past (`ShortMipConsole` in an
  AddressSanitizer build on GL and GL ES, S3TC on and hidden: the short
  top mip white, the short lower one black, where the build before read
  past both), and a texture past GL's largest size draws white, as on the
  Vulkan device; a map whose surfaces or nodes point outside
  its arrays -- a
  node's links, zones, surface, render bound, vertices and their points, and
  its collision hull's list -- is refused with their numbers, the vertex
  pool's entries no node names left alone, every map of the game having
  thousands past its points (`MapWalkConsole`: every map of the game loads);
  a fire texture's sparks and drops outside it are masked into it and a
  water texture's cells checked, a row's shift taken from the texture's
  width, where the original's writes with the package's `UBits`, every stock
  one its width's; a fire's `SparksLimit` past 1,048,576 is held there, where
  the original's makes room for any; a scripted texture drawn from outside its source wraps, and one drawn with a
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
- **A mod's actors drawn.** A mesh sequence of no frames draws its first
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
  place (a NaN location) sorts
  nearest among the translucent.
- **A mod's deletions and sizes.** A light in the level's light lists
  is kept, destroyed or not, so the lists keep their places; a destroyed
  mover's polygons keep their texture for a `TraceTexture` through them; a
  collision size or location past the cells an int names, or no number, is
  held to them.
- **A net game's edges.** A client's reference to a map actor destroyed and
  freed is refused, not loaded afresh in no level; a channel destroyed
  leaves nothing for the next bunch to merge into; an enum of more than 256
  names goes as a byte; a coordinate that is no number or past an int is
  held; a download past its size, or past 2 GiB, ends at once; a first actor
  bunch that opens nothing closes the connection, and a spawn that fails
  closes its channel (the original's are a fatal error and an assertion); a
  `TcpLink`'s failed connect waits for the script's timer on Linux as on
  Windows.
- **`ScriptedTexture.ReplaceTexture`**, which no stock script calls, takes
  a texture of the scripted texture's USize and VSize, steps it to now, as
  the original's Update does, and copies its current frame's top mip over
  the scripted texture's; else logs, as the original's (`ScriptedConsole` on
  the harness's `CapScripted`: `plaque_b` copied, `Compu_A04`'s size and
  None refused, `WhiteNoise_A01` stepped to `WhiteNoise_A02` and copied,
  each shot of it alike in both engines, correlated at 0.996 to 1.000). A
  scripted texture's source texture is taken once, as it loads, scaled by
  the two textures' bits, its palette with it, and that copy laid over it
  each tick before its notify actor draws, as the original's: a source set
  later, or a `ReplaceTexture` on it, never shows (`CapSourced` keeps
  `plaque_b` through both in both engines).
- **Config files.** A class's config file that is not there is empty, its
  properties keeping their defaults, until the class writes it
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has. A key
  with an empty value is a value, what the original's `ImportText` makes of
  no text: a string empty; a name, object or class None; a float 0; an int,
  byte or bool its default. Of a key given twice in a section the last is
  read and the last written, wherever the engine reads the ini: a class's
  config, the default URL, the settings it reads again by hand -- the
  client's, the audio and render devices' (`IniConsole`). The player's
  settings are its class's config, as the original's: a pawn spawned takes
  its class's defaults, which the user ini came into by the import, so a
  bool's `Yes` and a key not there leave Engine.u's default (`IniConsole`
  with `bInvertMouse=Yes`, `bMaxMouseSmoothing=No` and no `MouseSensitivity`
  nor `MouseSmoothThreshold`: False, True, 3 and 0.07 in both engines; its
  `MouseSensitivity`, `Bob`, `DodgeClickTime`, `Handedness` and
  `MouseSmoothThreshold` given empty, 0 in both). Those read by hand do the
  same: an empty float is 0, and an empty texture or skin detail, an enum's
  byte in the original, keeps its default. An ini or a `.int` file is read
  as the original's `appLoadFileToString` reads it, UTF-16 after a
  byte-order mark and else a
  byte a character, and an ini written back is written as its
  `appSaveStringToFile` writes one, UTF-16 after `FF FE` when a character in
  it is past Latin-1 and else a byte a character ([the
  configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration);
  `ConfigConsole`: a config string of U+03A9, U+2122 and e acute written by
  each engine as UTF-16 and read back whole by each; `LocalizeConsole`: a
  UTF-16 `.int` of a mod's folder, U+03A9, U+2122 and e acute, and a Latin-1
  one's e acute, whole). A script's console command takes its text so too,
  16-bit (`OpsConsole`: `set` of U+03A9, U+2122, a, alike in both). An ini
  written back has a changed value changed in its own line,
  a key new to a section added after the section's last key, a section new to
  the file at its end, and the rest of the file as it was, its line endings
  with it (CR LF, LF or a CR alone), by choice, so a player's or a mod's
  comments and layout stay; a file not there yet is written whole, CR LF, as
  the original writes it (`ConfigConsole` with no `DXCapConfig.ini`: the file
  each engine makes the same to the byte). An ini whose write fails fails
  quietly, as the original's config cache's does
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  nothing logged or shown, the game playing on, the values saved kept for
  the rest of the run. A read-only one, every write bit off, is left as it
  is, to root too, the Smart Pro's user, as Windows' read-only attribute
  holds for an administrator (`ReadOnlyConsole` with `DXCAP_READONLY=1`:
  `CacheSizeMegs` 5 and the default `MouseSensitivity` 4.25 after the
  saves, both inis the same to the byte after the run, and a clean exit, in
  both engines; DXEngine's as root in a user namespace too). Each ini
  changed since it was last written is written at every `SaveConfig` --
  a script's, `StaticSaveConfig`'s, a `SET`'s, `netspeed`'s and
  `lanspeed`'s -- and every resize of the window, the system ini, the user
  ini and a class's own file alike, whichever class's config was saved, as
  the original's config cache is flushed
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)),
  and every ini at a clean exit: a setting changed in play outlives a crash
  or a kill (`IniFlushConsole` with `DXCAP_KILL=1`: `CacheSizeMegs=7`, `Name=DXFlushName`
  and `MouseSensitivity=3.75` in the inis after the kill in both engines,
  where DXEngine's kept none). A run begun with no engine ini puts the client's and the
  devices' settings into the system ini before its first write, as at the
  exit. The original rewrites each file
  whole, CR LF, comments gone (`ConfigConsole`: a class's own
  `DXCapConfig.ini`, CR LF, read and written back alike). `StaticSaveConfig`
  from an object writes the class's defaults, as from the class
  (`ConfigConsole`). A config value of any type is read and written as the
  original's import and export have it (a property's text, above): a
  struct's members, a class or an object as `Type'Path'`, a dynamic array
  one line of its name per element (`ConfigConsole`: a vector, a class and
  two strings written and read back). A file's lines are read as the
  original's reader reads them: a header a line that begins `[` and ends `]`
  as it stands, a key up to the line's first `=` and its value after it,
  nothing trimmed, so `Runs = 5` is a key `Runs ` no property reads and a
  value's last blank stays (Engine.int's `ViewingFrom`,
  `NameChangedMessage`), a value that begins and ends with `"` read without
  them, an ini's and a `.int`'s alike; `;` and `//` begin no comment; a line
  ends at a CR or an LF; a key with no name (`=x`) is kept and written back;
  and the text ends at a NUL, what is past it neither read nor written
  back. A setting read by hand takes a bool and a texture or skin detail by
  its first token, as the import does, a bool's 1, `True`, 0 or `False`
  alone (`ConfigConsole` seeded with
  `Runs = 5`, `Words[0]="quoted"` and `Words[1]=two `, the file written back
  the same to the byte; `LocalizeConsole`'s Engine.int lines; `IniConsole`
  with a `User.ini` of `Name="Quoted"`: alike in both engines). A value
  written over one that differs from it in case alone leaves the old, as
  the original's `SetString` does: `JC` over `jc` keeps `jc` in the file and
  in what is read of it after (`IniEdgeConsole`, twice on each of a file
  whose `Word` is `jc`, one of CR line ends, one with `=x` and one with a
  NUL: the second run's lines alike in both engines, and the files as each
  engine writes them, the original's whole).
  A `SaveConfig`, an object's or the class's own (`StaticSaveConfig`, the
  console's `SET`), ends as the original's does (Core.dll `0x10151540`): the
  config read again into the defaults of every class deriving from the
  topmost config class above the one saved, its own among them, each class
  its base classes' sections first, its localized properties left as they
  are. So what the write kept, a value that differs in case alone, is the
  file's in the defaults again, a float is what the file holds of it, to six
  decimals, and a subclass takes what its base's section now holds
  (`IniEdgeConsole`: after `JC` is saved over `jc` the default `Word` is
  `jc`, a `Ratio` of 0.1234567 is 0.123457, and `IniEdgeProbeSub`'s `Count`
  the one saved, in both engines).
  Each section a class's config reads and writes, its base classes' and a
  `globalconfig` one's declaring class's too, is in the config file of the
  class whose defaults or object is read, as the original's `LoadConfig` and
  `SaveConfig` take their object's class's file: a player's `[Engine.Pawn]`
  (Pawn's `bNeverSwitchOnPickup`) is in the user ini (`ConfigFileConsole`:
  after the player's `SaveConfig` the user ini has the section in both
  engines).
- **The natives no stock script calls** (`ModConsole`): `ResetConfig`, a
  System or User class's section copied from `Default.ini` or `DefUser.ini`
  and its config read again into the class, its subclasses and their
  objects
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)),
  each object then told of it (`PostEditChange`), as each of a `SET`'s is:
  a mover put at its base and key the editor's way, which in play moves it
  (`ChangeConsole`: a `SET` of `DeusExMover` sends Liberty Island's
  `DeusExMover16`, at rest at its second key, to −112, 0, 0 in both
  engines), a client's brightness held ([brightness](#brightness));
  `DeusExPlayer.SetBoolFlagFromString`
  ([the player](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-player));
  `ExtString`'s text in parts
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small));
  and `clock`, `unclock` and `CyclesToSeconds`
  ([the timing natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#clock-unclock-and-cyclestoseconds)),
  which count the steady clock's nanoseconds where the original's count the
  CPU's cycles, by choice.
- **Spawn notifications.** A `SpawnNotify` on the level's list is asked
  about each actor spawned of its `ActorClass` (not one spawned while a
  notification runs, as the original's, a flag held over the walk, so a
  notification spawning its own class does not spawn without end), its
  answer standing for the actor from then on: the next notification is asked about it, and the
  spawn returns it, as the original's
  ([spawning](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#teleporting-an-actor);
  `CensusConsole`'s `CensusNotify`). Differs by choice: a None answer ends the walk,
  where the original's next notification with a class faults on it.
- **Two path natives no stock script calls**: `Pawn.FindRandomDest` 525
  answers None, as the original's does (its `findRandomDest` returns nothing
  in any level with navigation points, and finds nothing without them);
  `FindBestInventoryPath` 540, the way to the inventory the pawn wants most,
  is the original's search
  ([the best inventory](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)):
  each inventory spot's item weighed by its `BotDesireability` over the cost
  of the way there, the best way's start the answer, a pawn placed but not
  anchored given its nearest node at a weight of 0.00005. No stock map has an
  inventory spot: `InventoryPathConsole` on Liberty Island, each pawn asked
  three ways, the 48 that stood at one place in both engines answered alike,
  routes and weights.

A macro texture, which no stock texture has, is drawn as `D3DDrv` draws it
([the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness)):
modulated at twice its colour, the light map after it twice as bright as on
one pass, and not panned with the surface, at its own scale
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame));
the macro texture of the surface's texture's current frame, or of the
Matrix cheat's Sprite, as it is, as `Render.dll` hands it over
(`LightConsole` with `Macro` and `Matrix`, the world map drawn in the light
icon, which has none: no macro texture in either engine, at 1.001 of
D3DDrv's frame, where an empty one was passed, 1.62).
`LightConsole` with `Macro`, Liberty Island's `Spotlight0` on a world map
given itself as its macro texture, has the GL and Vulkan devices' frames at
1.00 of `D3DDrv`'s (`NoDetail`): in blocks of 16 pixels inside the map's
border, within 0.94 to 1.04 (without the macro, [lighting](#lighting)). The
D3D11 device draws it as the Vulkan device does, and its detail textures in
`D3DDrv`'s three bands, its blends clamped at 1 in ten-bit buffers, a mover's
surfaces a step in the depth and the frame at the render scale, under Wine
through DXVK against the Vulkan device on Linux: `DetailConsole` (`detail.py`'s
bands within 0.0002 of the Vulkan device's at every height, none failed, where
the build before failed 11; its first shot within a level of the Vulkan
device's, where 64% of the pixels lay up to 11 levels off), `DrugConsole` with
`HudWindows` (the HUD over `DrunkFX` within a level of the Vulkan device's),
`ShortMipConsole` (a 2x2 DXT1 texture over a blue 1x1 mip drawn red from its
top level, as on Vulkan, where the build before blended in the blue),
`ProveConsole` with `RenderScale` 0.5 (a shot of 640x360 at a window of
1280x720); the mover's depth step and the fallback swap chain by reading.

A fixed array (`FixedArrayProperty`) and a map (`MapProperty`), which no
stock package has, are the original's
([a property's text](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#a-propertys-text)):
a fixed array's elements read and written each by its inner property, in a
package's or a save's tag, a struct and a dynamic array, its size taken from
its inner once that is loaded, its text `(a,b,c)` for `GET`,
`GetPropertyText`, the ini and the travel, read from a `(` with the array
zeroed first, one character passed over after each element, the last's too,
then a `)`, and copied whole by a `Let`, an argument and a return; a map's
text empty, its ini key written none, nothing of one in a struct read, and
neither sent nor read by replication (`FixedMapConsole` on `fixedmap.py`'s
`DXCapture.u`: `(0,0,0)` as loaded, `(4,5,6)` and `(7,8,9,)` taken, `(1,2)`
as `(1,2,0)`, `GET` and `SET` alike, `Mapped` empty throughout and `OneB`
`(5)` after the `Let`, in both engines). A map's own tag, which no engine
writes, is skipped whole, by choice, where the original reads nothing of it
and takes its bytes for the next tag's. Core's classes are among the
original's 35: none is `PointerProperty` or `StringProperty`. By choice, a
save's exports are made whatever their flags, where the original's makes only
those of the load's context: its saves flag every export for both, and
DXEngine's older saves wrote spawned actors without them. An export of a
class named `Camera`, the editor's viewport, is never made, a reference to it
None, as the original's linker makes none.
Every import is checked as its package opens, as the original's linker
checks each
([packages](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)):
the export of its name in the package its outermost import names, of exactly
its class by name and package, directly in the group its outer found or at
the package's top, a Mesh found as a LodMesh too, UnrealShare standing for
UnrealI; one that is not public refuses the package (Core's
`FailedImportPrivate`), and one not found refuses it (Core's `FailedImport`),
but for a texture's, a palette's, a sound's or a song's, which is None
(`LocalMapConsole` on `badpackage.py`'s bar with a sound's name renamed,
`Sink` to `Sinx`, and with a texture's, `ClenMedmWalnt_A` to
`ClenMedmWalnt_q`: the start on it in both engines), so a map or a package a
script loads that another version of a package does not suit is refused
whole, a map's load failing as its check fails (`LocalMapConsole` on the
sweep's `BadImport.dx`: "Failed to load 'BadImport.dx': Can't find Class in
file 'Class DeusEx.Mission0X'", the start on Dx.Dx, in both engines). In a
save, one not found is None too, by choice, where the original refuses the
save: the camera of a third-person conversation going on as its
level was left, which ConSys makes in package `DeusEx`, is one, the original
never entering that level again (`ConSaveConsole`: Battery Park left 3 s in,
during its first conversation, entered again from the street in DXEngine;
the original's load of it failing, "Can't find ConCamera in file 'ConCamera
DeusEx.ConCamera0'", the player kept in the street). A sound's, a song's and a mip's skip offset is
read from package version 62 on (`OldVerConsole`: `vergen.py`'s version-62
`MoverSFX.uax` and `CoreTexStucco.utx` load as their sources in both
engines), a string's count at every version and a name's NUL ending below
64, and a function's code at any version as the original's reads it, a
`LetBool` its two expressions and a `Return` its one.

## Multiplayer

Matches the original's client and server
([the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md)):
joining and playing, listen and dedicated servers, replication with the
original's native lists, a simulated proxy's `Physics`, `Acceleration` and
`bBounce` in its first bunch alone whatever its `bSimFall` (`SimFallServer`:
the original's server's change of a `CapSimFall`'s `Physics` every 2 s
sent a DXEngine client never, its first bunch's alone), what each client
is sent picked from where its view is and, every other tick, from where it
will be 0.9 s or 0.4 s on, short of the world past the nodes that do not
block sight (read: Engine.dll `0x10362f04`, `0x103a37d8`), a struct's
members sent its own first, then those
of the struct it extends, the server's calls, downloads both ways, server
travel, the Entry level for a lost server -- loaded as `entry.dx` with no
option, a lost server's client travelling there by `?failed`, absolute, the
player's options from its server's URL on it, and logging in with them
(`TravelJoinConsole` with a server that stops: `Index.dx?failed?Name=...`,
alike in both engines but for the name, below) --, the login, the player's net
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
`PickJoinConsole`, `AskServer` with `AskJoinConsole`, `AnimEndServeConsole`
with `AnimEndJoinConsole`, `ViewServeConsole` with `ViewJoinConsole`,
`PauseServeConsole` with `JoinConsole`, `UnpackServeConsole` with
`UnpackJoinConsole`. Differs (bugs.md NET.38): a DXEngine server sends no
client `SimFallServer`'s `CapSimFall`, hidden and always relevant, a
simulated proxy with `bSimFall`, which the original's server sends.
DXEngine joins live
public servers, the mods they run included (MTL, ANNA, CDX, DXAG, SG), and
stays in. A replicated string goes as the original's does, 16-bit when a
character in it is past 0xFF, else 8-bit (`JoinConsole`: its player renamed
DXCapture, U+03A9, U+2122 through `SetName`, the name back whole from each
engine's server to the other's client); a `TcpLink`'s and a `UdpLink`'s
text goes 8-bit, a character past 0xFF as 0x7F, as the original's `ToAnsi`
narrows it, and comes in a byte a character (`LinkConsole`: A, U+03A9, e
acute sent to itself received as 65, 127, 233 in both engines). A client
unpacks an actor's animation as the original's does: its frame, rate,
tween rate and last frame each the 16-bit number sent times the double
0.0001, 0.0002, 0.001 and 0.0001, rounded once to a float, and a mover's
alpha and rate the number times the double 0.01 (`UnpackServeConsole`
with `UnpackJoinConsole`: each value of the first two stages the original
client's to the bit, an original server's to DXEngine's client and
DXEngine's server's to the original's, where DXEngine's client was a float
step off each; the third stage's alike in all). The rate it asks of a server is 20,000 where the ini holds the
game's stock 2,600 or none, by choice: set at the start as a `SET` sets it,
the ini, `Engine.Player`'s defaults and the viewport, so a mod's player that
sets its net speed from it once in asks the same (MTL's `MTLPlayer`); a net
client runs at most its rate / 64 frames a second, as the original's, 40 at
2,600 (`tools/dxcap.sh live` on an MTL server: 40 frames a second at 2,600,
120 at 20,000 on a 120 Hz display), and a server holds a client to its own
`MaxClientRate`. Any other rate the player set stays, and each `netspeed`
and `lanspeed` is logged. The multiplayer Player Setup's connection shows
it as the game's own "DSL or better" (`SettingsConsole`), where the
original's shows "Modem" for 2,600. Until its pawn arrives, a joining client's player waits in the
Entry level, logged in with the join's URL, its HUD, the console's
CONNECTING and the join's `Connecting (F10 Cancels):` and
`deusex://<host>/<map>` drawn over a black view: the frame that loads the
map draws once more before the `JOIN` goes, the join still pending, which
gives the waiting player the join's lines for a minute, and the
connecting message's time is the player's own level's, as the original's;
the Entry level is ticked twice a frame, after the game's level, as the
original's; the player is destroyed at the next load wherever it stands,
and once the pawn arrives it stays in Entry, let go, as the original's
(`TravelJoinConsole`, each engine joining the other: the player
`Entry.JCDentonMale1` in the wait, its first two wait ticks in the load's
frame at one Entry time, the original's Entry level stepping 0.0485 s in a
frame of 0.0239, each wait frame black but the HUD and the words).
A remote player's pawn destroyed by script (`Kick`) has its connection
closed instead, as the original's `DestroyActor` has it (`ServeConsole`
with `Kick`, each engine the server and the other's `TravelJoinConsole` the
client: the connection closed, the client back on Entry by
`Index.dx?failed?...`, alike). The server-only stubs are under
[not needed for single player](#not-needed-for-single-player). Differs:

- **The net driver** is DXEngine's own, by choice, over UDP with
  `[IpDrv.TcpNetDriver]`'s timeouts; the connection, its channels and the
  handshake are the original's, but for the challenge's check (below), and
  so is its socket: broadcast and address reuse on, 32 KB buffers for a
  client's and 128 KB for a server's, a server's port the URL's or the next
  free of the 20 from it, or with `PORT=` on the command line that port
  alone, its address every one of the machine's but the one `MULTIHOME=`
  names or, with `-PRIMARYNET`, the first of the machine's own name, where
  the script's links bind too (`ServeConsole` with `PORT=7795`, then with
  `MULTIHOME=127.0.0.1`: the serving URL's address `0.0.0.0:7795`, then
  `127.0.0.1` with no port, alike in both engines; with
  `MULTIHOME=127.0.0.2`, the game port, the query answerer and the beacon
  bound there alone, nothing answering at 127.0.0.1, in both engines); a
  server's host name is
  looked up on a thread, the game going on; the socket reads each datagram
  waiting, of up to 576 bytes, a longer one or any failure but nothing
  waiting, a reset among them, ending the tick's reading, the run's first
  logged (read: IpDrv.dll `0x1070a440`); a pawn's `GETPING` and `GETLOSS` ask its own player, as the original's:
  on a server a remote player's connection answers its lag of the last
  second in ms -- the mean of its acks' round trips less half the
  connection's last frame time, the first second's sum begun at 9999 as the
  original's constructor begins it (read: Engine.dll `0x10403240`,
  `0x10404dc6`) -- and its incoming packet loss in percent, each after a
  blank, which the player's replication info takes for the scoreboard's ping
  and a server's GameSpy answers, and a local player's viewport `0` and no
  `GETLOSS` (`WordsServer` with `PingJoinConsole`, each engine the server: the
  remote player's ping some 90 and loss ` 0`, a client's own `GETPING` `0`
  and `GETLOSS` nothing, in both engines); and each second sets the player's
  `bBadConnectionAlert`, which no stock script reads, by the original's test
  (read: `0x10404e1c`). Differs (bugs.md NET.39): the alert is on for a
  player's first second in the game, where the original's is off from its
  first half second (`AlertServer`).
- **The challenge's check**: a login whose answer to the challenge is wrong
  is refused, by choice, where the original's server compares the answer
  with whether the right one is 0, a slip, and so refuses only a missing
  answer or a 0
  ([joining](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#joining)).
  DXEngine's makes the comparison the original's means, which a login that
  never saw the challenge fails; no stock client answers wrong.
- **`InternetLink.GetLocalIP`** answers the address this machine reaches the
  network from (a UDP socket's local end toward 8.8.8.8; 127.0.0.1 with no
  route), by choice, where the original answers the first address of the
  machine's own name, on Linux often a loopback address.
- **The download cache.** A download is written as its GUID and `.uxx`
  whatever `CacheExt` says, and found by its GUID and `CacheExt`, as the
  original's: with another extension there it is not found, the join failing
  with `Can't find file for package '<name>'` (`JoinConsole` downloading a
  package from the original's server with `DXCAP_CACHEEXT=.ucx`: `<GUID>.uxx`
  written and the join failing so in both engines; with the game's `.uxx` the
  join goes on). A package a join loaded from the cache is let go by name at
  the next map load that does not use it, and that load's collection frees
  what nothing uses of it but its code, which stays loaded, by choice, where
  the original's collects it all
  ([housekeeping](#housekeeping-not-seen-directly)). A server sends a client
  four files at once at most, a fifth refused, by choice, where the
  original's sends any number; a client, the original's too, asks for one
  at a time. A download that fails deletes its temporary file, by choice,
  where the original's leaves its `NNNN.tmp` to the next start's cache
  cleaning.
- **A listen server runs at most 144 frames a second**, by choice, where the
  original's runs as fast as it draws: a remote client's queue drains its rate over a
  tick, so the faster the ticks, the less room an unreliable call finds
  ([packets](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)).
  `ServeConsole`'s 8 beeps reach the original's client 7 of 8 at 100 and at
  144 frames a second, 4 at 200 and none at some 1,300 (uncapped on the
  desktop with no vsync), as the original server's reach DXEngine's, 6 to 8:
  the first goes as the host stops, its last step's packet just sent.
- **A link's binary data**: `ReceivedBinary` gives the script the bytes that
  came, each element of its array, by choice. The original's gives it
  others, the count right: 20 bytes sent to a `UdpLink` in binary mode reach
  its script as 20 bytes of what else is there, whatever was sent, and its
  `SendBinary` answers False; no mod can have counted on them. A
  `UdpLink`'s `SendText` and `SendBinary` answer False when nothing went.
  A `TcpLink`'s `ReceivedBinary` is raised for each 255 bytes that came, by
  choice, where the original's is raised once a read with its whole count,
  up to 999, its array 255 bytes of its stack. An accepted link's
  `RemoteAddr.Port` is in host order, by choice, where the original's is as
  the socket address holds it, and a second connection to a listener of no
  `AcceptClass` that holds one already is closed, the original's left open.
- **The player's name in a net game**: a name the player never chose --
  none, or the game's stock `Player` -- logs in as `DXEnginePlayer`, a
  client's to its server and a listen server's host's, by choice, so
  DXEngine's players show as such (`Engine::SetMultiplayerName`). A name the
  player chose is kept; single player, the menus and the inis keep the
  game's.
- **A value too big for its bunch** rolls its whole field back, by choice,
  the client keeping the value it had, where the original's sends the
  field's number with no value, which the client reads as zero, and asserts
  where the number itself does not fit; a bunch's fields go in export
  order, the original's native ones first in code order, then the script's
  (`BigServer` with `BigJoinConsole`: from the original's server the
  client's string goes to '' as the 600 characters overflow, from
  DXEngine's it stays `abc`, then `xyz` comes in both). No stock update
  passes 4,015 bits.
- **A conformed package**, of more than one generation, joins by its GUID,
  by choice, its counts held to the server's, where the original's client
  refuses it: "Package '%s' version mismatch". Every DXMP map also imports
  New Vision packages of other GUIDs, which both refuse, so only a lone
  conformed package parts them (a dedicated `WordsServer` with
  `JoinConsole`, New Vision's `CoreTexSky` on the client's paths alone:
  the original's client back in the menu with the mismatch, DXEngine's
  joined).

As in the original:

- A root window per pawn, made at its first Possess and kept in it, as the
  original's `InitRootWindow` makes it; the engine draws, ticks and feeds
  the viewport's own player's: a client joining a listen server gets a root
  of its own and leaves the host's in place (`ServeConsole` with
  `JoinConsole`: the server's log names each root's pawn).
- On a dedicated server `GetPlayerPawn` answers None, as the original's
  with no client or viewport, and `AttachDecal` attaches none, as the
  original's with no client.
- A URL is read as the original's reads it (Engine.dll `0x103f7b60`;
  [addresses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#addresses)):
  its first 1,023 characters, as its buffer holds them (`0x103f7d00`), one
  past U+FFFF counting two (`URLEdgesConsole`: `DX.dx#`, a portal of 1,007
  `p`s and `?Name=Cut1234`, the player `Cut1`; the original's process ends
  at that open, its log unflushed, a crash not copied, by choice); then its
  options and
  a `#` portal, then a drive letter's file, a
  protocol, the game's in any case, a host and its port, and the map, a `/`
  after it marking a portal for the game's protocol; a relative travel on
  the last URL's protocol, server, port, map and options, a partial one on
  its options, an absolute one with its player options. A blank in an
  option or the portal, a `/` alone before the map, anything past a second
  `/`, or no server, map or option refuses it: the URL is then the default
  one, `Index.dx` with no option, which a travel goes to as to any map not
  there, so the level's own map is loaded again with no option
  (`URLPartsConsole`: `DX.dx?Name=Two Words` and
  `open 01_NYC_UNATCOHQ/a/b` from Liberty Island load it again as
  `01_NYC_UNATCOIsland`, the player `Player`, in both engines). An option
  is matched whole, `?listen=1` no
  listen server (`URLPartsConsole`: net mode 0 in both). A URL cleared is
  empty whole, its port the game's default again. On Linux a path from `/`
  is a file as a drive letter's is, by choice, where the original's parse
  refuses a `/` alone: a map opened by its full path, as on Windows.
- Entry's actors find their regions in Entry's own model, with its zone
  actors and its LevelInfo, while a map is current, as the original's
  (`EntryConsole`: markers spawned into Entry from Liberty Island, as the
  game's server lists spawn their links there, and moved, alike in both
  engines). A level's LevelInfo is in its own zone, in no leaf and zone 0,
  wherever it stands, as the original's `SetActorZone` puts it, where
  DXEngine found it by its place (Entry's in zone 1, leaf 4).
- A client replicates its own player's pawn to the server each tick, before
  its packets go (`ULevel::TickNetClient`): the values the script classes'
  statements send from a client, and of the engine classes' native lists
  the player pawn's alone, as the original's: its `Password`,
  `bReadyToPlay` and each of its `AdditionalViews` whenever it differs, so
  a client's spy drone and camera feeds are viewers the server makes what
  is round them relevant for (`ViewsServer` with `ViewsJoinConsole`, each
  engine the other's server: the client's `AdditionalViews[0]`,
  `PlayerStart15`, reaching the server and the marker placed there
  arriving, in both engines). A value the other side sends is kept as what
  it last got, so it does not come back (read: Engine.dll `0x103fdfe6`).
  Deus Ex's `FrobTarget` goes so: the client finds what the
  player looks at, and the server's `ParseRightClick` takes it, so a weapon
  or an item is picked up on a net game only so (`PickServer` and
  `PickJoinConsole`, each engine as server and as client). Each side keeps
  what the other last got of each value, and a value goes when its
  property's compare finds it apart from that, a string's without case, as
  the original's `Identical` and its native lists' `appStricmp` take it: a
  name changed in case alone does not go (read: Engine.dll `0x103fead0`,
  `0x103616e0`). The native lists' floats, a vector's and a plane's too,
  differ by their bits, so a change from 0 to -0 goes and a NaN that stays
  does not go again (`FloatBitsServeConsole` with `FloatBitsJoinConsole`,
  each engine the server and the other's client: each change of `Deaths` the
  client was in for reaches it, -0 and the NaN among them).
- A call that goes to the other side, and one a simulated or dumb proxy does
  not run (a function not simulated), writes nothing: the original's
  `ProcessInternal` runs none of it
  ([variables and calls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#variables-and-calls)),
  so a variable assigned from it keeps its value, a bool is cleared, and
  whatever else takes it reads its zero (`AskServer` with `AskJoinConsole`,
  each engine's server with its own client: a client's call of each type
  that goes to the server, one its proxy does not run, the game's
  `GivePlayerAugmentation`, and the server's three that go to the client
  each leave the variable as it was, every client line alike).
- A simulated proxy pawn -- another player's or an NPC on a client -- is
  sent no `AnimEnd`, as the original's actor tick has it
  ([the main animation's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)):
  its animation plays, ends, finishes, loops and calls its notifies as any
  other's. Only a simulated `AnimEnd` would run there, and no stock pawn
  has one, only `DeusExWeapon`'s `ClientFiring` (`AnimEndServeConsole` with
  `AnimEndJoinConsole`, each engine the server and the other's client: the
  server's pawn sent 3, 5 and 3 by stage, the client none, its stops and
  wraps the server's).
- A pawn whose own role is simulated proxy moves along its velocity and
  runs its `Tick`, nothing more -- no state code, timer, life span or
  physics --, whatever the net mode, as the original's actor tick has it
  (`SimRoleConsole`: a pawn made one in single player moves 120 units in
  3 s, its timer, state code and life span never run, in both engines).
- Every other actor ticks by its roles alone too, whatever the net mode, as
  the original's actor tick has them
  ([ticking by role](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#replication)):
  a dumb proxy only falls; an autonomous proxy -- a client's own player --
  moves only in its moves; a proxy runs its state code, and counts down its
  latent action, in a simulated state alone, so a client runs none of its
  player's, a weapon's or a mover's other state code; an actor whose remote
  role is autonomous -- a server's copy of a client's pawn -- runs its state
  code and timer alone, and with any role but the authority nothing
  (`RoleGatesConsole`: in single player each kind's ticks, timers, state
  code passes, moves and expiry alike in both engines, the one made a proxy
  mid-sleep waking half a second after it has the authority again). A
  viewport's player has the remote role simulated, so single player is
  untouched.
- An actor this side has no authority over goes only with its channel or its
  player's connection, unless it was sent once to keep, as the original's
  `DestroyActor` refuses the rest whatever the net mode ([destroyed
  actors](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#destroyed-actors)):
  a proxy whose life span runs out stays, its `Expired` again each tick, its
  timer and state code going on and its physics stopped; one that falls out
  of the world stays, its physics none; its script `Destroy` answers False
  (`RoleGatesConsole`: the simulated and autonomous proxies expiring tick
  after tick and there at 3.25 s, the falling dumb proxy out of the world
  and there, each one's `Destroy` False, in both engines).
- A paused level reads each player's input through its `Player`: the
  viewport's keys and mouse for its own pawn, nothing for a remote player's,
  whose `PlayerInput` takes its input as it stands, as the original's paused
  tick has it (`PauseServeConsole` with `JoinConsole`, each engine the
  server and the other's client: the remote pawn's `aMouseX` of 1 taken as
  1, the host's 0, in both).
- A server's copy of a client's player viewing a pawn takes that pawn's view
  rotation, eye height and weapon's `PlayerViewOffset` into its
  `TargetViewRotation`, `TargetEyeHeight` and `TargetWeaponViewOffset` each
  tick, the offset kept while the pawn holds no weapon, as the original's
  server does; they go to the client, whose `PlayerCalcView` puts them on
  its copy of the viewed pawn (`RoleGatesConsole`'s viewers;
  `ViewServeConsole` with `ViewJoinConsole`, each engine the server and the
  other's client: the targets the host's on both sides).
- A player logged in with no net driver -- in single player, or waiting in
  the Entry level -- is an admin, and with `-alladmin` every player, a
  server's host and its clients too, as the original's `SpawnPlayActor`
  makes them, so `admin <command>` runs in single player (`AdminConsole`,
  plain and with `-alladmin`; `ViewServeConsole`'s joined player with
  `-alladmin`, alike in both engines).
- **A dedicated server** makes each joining player's own root window, as the
  original's `Possess` does, and lays its windows out on the base 640 by
  480, having no viewport, its windows' scripts measuring with the GC as it
  is, there being no render subsystem; nothing there is drawn
  (`PickServer`, `AskServer`: a player joined and the server running on).
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
- A bunch for a channel not open, with no control channel yet, ends its
  packet, nothing past it read; an unreliable bunch is taken only once its
  channel's open has come, or when it opens the channel and closes it too,
  any other dropped, as the original's: an actor's open lost and a later
  reliable bunch of its channel come first, its updates wait for the open's
  resend, the client playing on (`OpenLossServer` with `OpenLossJoinConsole`
  behind `netdrop.py`, the actor's open taken out for 3 s, each engine the
  other's server: the actor and its call coming after the 3 s, the client
  in the game to the end, in both engines; `bunchfirst.py` against each
  engine's server: the packet acknowledged and its `HELLO` never read).
- A join logs in with its URL's own text, the host left out with any game
  type, a port not the default's kept, `:7791/Index.dx?Name=...`, which both
  servers read as the map `:7791` and the portal `Index.dx`
  (`PortJoinConsole` against `LoginServer` with `DXCAP_PORT=7791`, each
  engine the other's server: the two servers' login request lines alike); a
  join whose connection closes fails with the localized `ConnectionFailed`;
  a refused `JOIN` sends `FAILURE` and the game's error, bare where it gave
  none; `HAVE`'s `GEN=` is taken as it is; `WELCOME`'s `CHALLENGE=` is
  taken, and a package whose file is not found but whose `<name>.dll` is in
  `System` counts as had (read: Engine.dll `0x1040b4dd`, `0x1040c170`,
  `0x1039f780`, `0x1040a880`; `HandshakeConsole` against `handshakeserver.py
  --mode guid --pkg Window`: no file asked for, in both engines).
- A link's `Resolve` takes a dotted address at once, `Resolved` raised
  within the call, and fails `255.255.255.255` at once, a name looked up
  and answered from a later tick; an ephemeral `BindPort` leaves the link's
  `Port` 0; a `UdpLink`'s read of a datagram longer than its room
  (`ReadBinary`'s count, `ReadText`'s 4,096 bytes, the tick's 4,095) fails
  and the datagram is gone, `ReadBinary: Error reading text.` or `ReadText:
  Error reading text.` logged, a `ReadBinary`'s array holding the
  datagram's first bytes all the same, and `ReadText` empties its string
  first; `GetLastError` answers WinSock's last error of the thread on
  Windows, as the original's, and on Linux, by WinSock's numbers, the last
  error a link's socket met, 0 after a read that took a datagram, as
  WinSock's under Wine (`UdpEdgesConsole`: each line alike in both engines,
  but `SendBinary`'s answer, above, and `ReadText`'s string). By choice,
  `ReadText`'s string is the datagram's text alone, where
  the original's runs on past it into its buffer's earlier bytes to a NUL,
  at most 1,023 characters: `short` read after a 5,000-character datagram
  as `short` and 1,018 of its x's.
- A refused join shows the server's `FAILURE`, `Rejected By Server:` and its
  text, and a `FAILCODE` after it goes to the console's `ConnectFailure`
  alone, with the URL less its password, for the menu to ask again
  (`PassJoinConsole` against a server with `?GamePassword=dxcap`, each
  engine the other's client: the two refusals' and the join's lines alike).
- A login is refused by the game's `PreLogin` error alone, a fail code with
  no error letting the player on, and the server logs each refusal,
  `PreLogin failure: <error> (<code>)`, as the original's does
  (`PassJoinConsole` against DXEngine's server: NEEDPW's and WRONGPW's
  lines). A `TcpLink`'s `SendText` answers all it queued, the CR LF of line
  mode counted, a link with no connection yet queuing too (read: Engine.dll
  `0x1039f780`, IpDrv.dll `0x10708870`).
- A `TcpLink` whose `BindPort` failed is left as the original's, its
  `Socket` 0: a second `BindPort` answers 0, logged as already bound;
  `Open`, `Listen` and `Close` answer true and do nothing; the sends and
  reads answer 0. `Open` and `Listen` empty the send queue, `Listen`'s
  backlog is 1, and `ReadText` reads 255 bytes at most and leaves its
  string as it was where it reads nothing (`TcpLinkConsole`: each answer of
  the link bound to port 1 alike in both engines, its string still `kept`;
  the queued `lost` and `early` never sent; the 600 characters read as 255,
  255 and 90; `ss` showing the listen's backlog 1). A `UdpLink` whose
  `BindPort` failed is left so too, its socket closed and its `Socket` 0: a
  second `BindPort` answers 0, logged as already bound, and nothing is sent
  or read (`UdpRebindConsole`: B asked again answering 0, A receiving
  nothing, in both engines).
- A `TcpLink`'s connection ends as the original's: `BindPort` makes its
  socket's close abortive (`SO_LINGER` on, 0 s), so a link destroyed while
  connected resets its connection; `Close`, once the queue is sent, ends
  its side and goes on taking what the peer sends, raising nothing for it,
  until the peer's end comes, then closes the link and raises `Closed`
  ([the script's links](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-scripts-links);
  `LingerConsole` with `lingerpeer.py`: the destroyed link's peer reset;
  the closed link's peer reading the end, its `bye` sent, the link closed
  as the peer's end came, 2.02 s after opening, in both engines).
- A `TcpLink`'s tick reads in the tick its connection is made, after
  `Opened`, so text that came with the connection is raised at once.
  `Close` answers true: a listening link holding no connection of its own
  goes to `ListenClosing` at once, listening still and raising nothing,
  and `Listen` takes it back; one holding one shuts that connection once
  its queue is sent, its own socket kept; any other link pends its close
  (`ConnectClosePending`), one not connected keeping its socket, its
  shutdown failing each tick (`Close: Error while attempting to close
  socket.`), so a `BindPort` is refused as already bound. `IsConnected`
  answers whether the socket takes a send, in any state but `Initialized`:
  true while a close is pending (`LinkEdgesConsole`: A's greeting read at
  its opening tick; A closed and asked `IsConnected` true; C closed
  unconnected in state 6, the error each tick and its `BindPort` again 0;
  L2 closed in state 7 and `Listen` true again; in both engines).
- A server of another version's `UPGRADE` is shown as the original's
  pending level shows it: for a `MINVER` above 1100 the player's upgrade
  menu (`ShowUpgradeMenu`, empty in the stock game), else `Connection
  failed` over `Server's version is outdated` for 6 s (`HandshakeConsole`
  against `handshakeserver.py`, `--minver 400` and `1200`). The join then
  ends, that shown left as it is, by choice, where the original's stays
  pending, its `Connecting` lines back once those run out, until the
  player cancels it.
- The handshake's lines are read as Core reads the command line's
  ([the command line](#the-command-line)): the command word as its
  `ParseCommand` takes it, past spaces and tabs before and after it; a key
  in any case, but not straight after a letter or digit, so `HELLO`'s
  `VER=` is not `MINVER=`'s; its value as `Parse` takes it, in double
  quotes or up to a space, tab, CR, LF or comma, into the original's
  buffers, a login's `URL=` 1,023 characters, a package's `PKG=` 63 and a
  level's `LEVEL=` 4,095; a number `appAtoi` of all that follows the key,
  so `VER= 1100` is 1100 (`FLAGS=` its `wcstoul`), `LONE=` true for `On`,
  `True` or `1` alone, and a `USES` line's `GUID=` four words, each the
  base-16 number of its eight characters, all 0 where its text is not 32
  characters long, written back as 32 hex digits, so the cache's names are
  never a server's text (`ParenJoinConsole` against `LoginServer`, each
  engine the other's server: a login as `JC(1)` with a class and a password
  taken whole, the two servers' lines alike; `HandshakeConsole` against
  `handshakeserver.py --mode values`: the login answering challenge 7 with
  its `Checksum=`, and `Connection failed` over `Rejected By Server: spaced
  text`; against `--mode guid`: the package's file asked for by the same
  four words, and its refusal's lines alike, in both engines).

A client's travel with its items, its
player's replication info not come yet, leaves the player out, by choice:
the original's export reads the info regardless and faults (Engine.dll
`0x10390951`). A map loaded again is made anew, by choice, where the
original's makes the same objects anew in place, so a reference held across
the load names the level's object of that name, where DXEngine's is None
([a level left behind](#housekeeping-not-seen-directly); `URLEdgesConsole`:
the menu's `LevelInfo` held across its own load is the level's in the
original); a URL of another protocol is logged as the original's logs it,
`LaunchURL <url>`, and opened nowhere, by choice, the level playing on,
where the original's hands it to the system to open, so that a server's
travel would open any handler on its clients' systems.

A command runs where the original's runs it: a pawn's `ConsoleCommand` on
its own player -- the local player's viewport, a remote player's connection
on a server, that player its exec functions' executor, from the login that
spawns it on, so that what its root window sends as it is possessed runs
too --, and an actor's on the game engine, which takes none of the
viewport's, the render device's or the window's words (`WordsServer` with
`CapWordsMatch`, each engine the server: the actor's and the remote
player's words run the game's exec functions of their names, `DxWho`'s
executor None and the player, the remote player's `SAY` its own, and
`KEYNAME` and the colour depths answer nothing, alike in both engines; the
joining player's root window's three loops over the key names,
`ConditionalBindMultiplayerKeys`' among them, run `KeyName` 767 times on
each engine's server, none on the build before's). A dedicated server says an actor's `SAY` to
every player with a beep, and answers `GETCURRENTTICKRATE`, the frames it
last counted in a second, and `GETMAXTICKRATE` (`WordsServer`: `CapHearer`
hears the say, beep True, and the rates some 20, in both engines). A
dedicated server takes command lines on its standard input, in the place of
the original's log window, each run as that window runs a typed line: on
the game engine, 254 characters at most, its output to the log, Core's
"Unrecognized command" logged where nothing takes it, the input's end
ignored, so an operator can `say`, `servertravel` and `exit`, on Windows
from a pipe or a file the engine is given ([running
it](ENGINE.md#running-it); `TypedServer` fed `SAY`, a word nothing takes,
`GETMAXTICKRATE`, `SERVERTRAVEL` and `EXIT`: the say heard with its beep,
"Unrecognized command", "20.000000", the next map's start and "Closing by
request" in its log, none of them in the build before's).
By choice, PlayerPawn's clipboard natives on a dedicated server, which has
no window, hold the text in the engine, a copy pasted back, where the
original's reach the system's clipboard; an object
or a name the package map has not goes as None, once, where the original's
goes as -1, which the other side reads as channel 511's actor where it has
one and as another name; and a refused file request's close goes reliably,
where the original's goes unreliably, a lost one leaving the client's
download waiting and its channel closing for the connection's life.

## Not needed for single player

Stubs a single-player game does without, by choice: none is needed unless a
mod needs it, and Revision's scripts call none past the stock ones. The
audit lists each:

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

`StatLog`'s and `StatLogFile`'s natives do nothing too (empty to the
audit), so a DXEngine server logging world stats writes no log of them.
