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
choice. The game's 26 calls pass identifiers alone.

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
either engine's saves and the original's hub save, `ReturnConsole`). The
console's `open ?loadgame=N` loads the slot as the game's own `LoadGame`
does (`LoadConsole` with `Open`); a slot that is no number loads nothing in
either, DXEngine reading it as slot 0. The console's `SAVEGAME n` takes any
number as the original's `SaveGame` does: -1 the quick save, 0 a new slot,
and any other `Save%04d`, a negative one too -- `savegame -3` writes
Save-003, and `savegame -2` its SaveInfo to Save-002 with the level saved
into Current -- and the Load Game screen lists every `Save*` folder, both of
those among them, by name, as the original's `FindFiles` on NTFS gives them,
by upper case, `[`, `\`, `]`, `^`, `_` and `` ` `` after the letters
(`SlotConsole`). A bare `?loadgame`, a slot named by no number, is no load,
as the original's browse asks for `loadgame=`: the console's `open` of it
goes to the default map, `Index.dx`, which is not there, so the level's own
map is loaded again, as for any map not there (below): from the menu map the
menu again, its URL a load's, `Index.dx` (`URLConsole` with `Bare`: the same
lines in both engines). Before a level is saved into `Current`, the
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
at a portal (`open <map>#<portal>`) starts the player at the
teleporter of that tag (`PortalConsole`). A level's URL, which
`Level.GetLocalURL` gives, is the one it was entered by, as the original's
`LoadMap` sets it: a level returned to from `Current` names the saved file,
`..\Save\Current\<map>.dxs` with the travel's options, as the original's
browse loads it; a load -- an option starting `load`, `?loadonly` or
`?loadgame` -- leaves the URL the level holds, a map file's `Index.dx`
(every stock map's), so a restarted level's is `Index.dx`, or a save's, the
URL its level was entered by, which a save keeps. A travel builds on the
last URL, the one the level was entered by, less its `?loadonly` and its
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
The saved level is found whatever case the URL spells the map in, with or
without its `.dx`, as the original's file system finds it: the stock exits
spell `12_vandenberg_cmd` and `00_training` (`RevisitConsole`: the island
opened again as `01_nyc_unatcoisland` has the crate left there). A travel's
`PreTravel` goes to every actor of the level that has the function, with no
argument, as the original's `SetClientTravel` sends it: the mission scripts'
among them (`TravelConsole`).

A travel to a map that is not there fails after the pre-travel, as the
original's browse does, with its message for 6 s: Core's `Failed launching
URL` over `Can't find file '<map>'` -- a console's `open` of one too, which
travels as any. Within the map's mission the level plays on as the
pre-travel left it, pruned and saved into `Current`: the player without its
augmentations, skills and flag base. For any other map `Current` is emptied
and the level's own map loaded again on the travel's URL, with its travel
info: the level begins again from `Maps`, the message gone with the pawn
that showed it, or stays where its URL names no map there, one from
`Current` or restarted (`BrowseConsole`: a missing map outside any mission
and one of the island's, alike in both engines). The console's
`switchlevel` is the player's exec function, whose `ServerTravel`
Deus Ex's `LevelInfo` passes over in standalone: no travel there. A
`?restart` (the console's `RestartLevel`) loads the `DeusExLevelInfo`'s map
again with `?loadonly` and no other option, carrying nothing: the player
starts afresh, its login with no name, as the original's
([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving)).

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
in both engines, the original on `D3DDrv`).

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
`NoSuchConPackage`, in both engines).

Differs (bugs.md DX.7): the ConListItems' and the conversation camera's
names.

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

Differs (bugs.md DX.4): the text parser's `GetName` and `GetColor` and its
G, F, GOAL, DC and FILE tags, which no stock text reaches.

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
`CapNonePreempter`, in both engines).

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
(`MoveConsole`'s `Watch`; `move.py --pawn`, which flags no tick before the
reference's start or past its end: bugs.md X.6). A walker's heading drifts
with the frame rate, in both engines alike: after his turn toward the spot
he drifts across his way, 0.06 units a second toward the spot's side at 120
frames and 0.1 away from it at the original's 227 with vsync off, where he
meets the wall half a unit further along. DXEngine uncapped, at some 300,
meets it further along again, and there in some runs he slips past within
two seconds.

Differs (bugs.md AI.9): `ActorReachable` of a navigation point, 5 of 87
answers on Liberty Island apart, the original's asking the path network for
a pawn standing at a node. Differs (bugs.md AI.10): `FindPathToward`'s
second way, 10 of 24 tries apart. Differs (bugs.md AI.11):
`AIPickRandomDestination`'s second try a step nearer. Differs (bugs.md
AI.13, AI.14): `GetPathnodeList`'s 32 nodes, the pick's pitch,
`ReachableFromAnchor`'s direction. Differs (bugs.md PHYS.20, PHYS.21): the
reach tests' moves pass through the pawn's base, and `JumpLanding`'s steps
are raw traces.

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
of some 400 to 700 made in either. Differs by choice:

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
or not, takes its style's flags, and lands on whole pixels, as the original's
`WrappedPrint` and `DrawTextClipped` place it
([the loading screen](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-loading-screen)):
a line from its X and the canvas's `CurY` each truncated, a centred one's
half the room it leaves added before, each glyph a whole pixel's step
(its width and `SpaceX`, truncated), and `CurX` where the line ended;
`DrawTextClipped` centres nothing. `TileConsole`, on the canvas and in a
window over a grey box: `StaticLine` modulated (the line across a camera's
and the InfoLink's static, a quarter of it colour 0, black) at 1.02 of
`D3DDrv`'s brightness; `HackInfoBackground` modulated (the hacking screen's
message, all colour 0, dark grey) drawing nothing, at 1.00; in the window,
`HUDScopeCrosshair`, a masked texture, drawn `DSTY_Normal` at 1.00, its
lines alone; "NEW GAME" in the TrueType font, drawn opaque, at a
correlation of 1.00 to `D3DDrv`'s, its edges blended into the box (the
stock menus draw it translucent); on the canvas, clipped text drawn
`STY_Translucent` translucent.

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
within 2 pixels at 1024x768).

A NoSmooth texture is point-sampled from one mip, with no anisotropy, as
`D3DDrv` samples one, on Vulkan, GL and GL ES alike, `LODBias` given GL ES's
draws in its scene shader: its mip the floor of log2 of its texels to a
pixel (`MipConsole`: a floor of `CapMips` laid under the view, drawn
NoSmooth, one texel to a pixel and each mip from `D3DDrv`'s rows, 0 of 362
apart at 1024x768; smooth, GL ES's mips GL's, 0 rows apart).

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
([which device draws](ENGINE.md#rendering)). Differs by choice:

- **Vsync** only through SDL's renderer: X11's and Win32's presents do not
  wait for it, and the engine's own Wayland window (a dev build's) copies
  the frame into its buffer and commits nothing of itself: untried with the
  software device.

Differs (bugs.md REN.37): at the near plane the software device and D3D11
clip at 1, Vulkan and desktop GL clamp the depth.

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
realtime change makes it again. `SpriteConsole`'s icon, its colour 0
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
A corona's skin is drawn at its current frame, an animated one's or a
fractal one's, as the original's: `CoronaSkinConsole` gives `Light169` a
chain of three noise frames stepped every 4 s, its current frame at each
shot logged alike in both engines and drawn alike (correlated with
`D3DDrv`'s at 0.975), and the fire texture `LaserBeam1`, its image changing
from shot to shot in both.

### What a pawn holds

Matches the original
([a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments));
`HeldConsole`.

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
of the original's frame, correlated at 0.97, where at 8cde7490 it lay a
pixel lower, 0.84; the zoom and the drone's window within 0.5% of
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
  club, a doorway's glass in it, 0.987 over the frame.

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
turns). A light with `bDynamicLight` is a moving one before all else,
lighting a surface with no shadow and left out with `NoDynamicLights`, as
the original's `AddLight` takes it ([light
maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps));
the stock maps' few, on 02_NYC_Street, 02_NYC_Warehouse, 08_NYC_Underground
and 99_Endgame1, are moving ones anyway, neither static nor no-delete. A
mover's polygons are lit where the mover is, as the original's brush tracker
files them into the level ([the movers in the
BSP](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)):
its static lights through the shadows raytraced at its `BrushRaytraceKey`,
the moving ones over it there, its map built again when it moves or turns,
its zone's ambient light the mover's own (bugs.md REN.34).
`MoverLightConsole` at 1024x768, Liberty Island's `DeusExMover10` moved 100
units off its place: the door at 1.003 of the original's frame under its own
lights and 1.000 with a lamp 48 units before it, where it was 0.82 and 0.43,
lit as if still at its key. A spotlight's cone is the original's, its cosine
`1 − LightCone ÷ 256`, its value rounded down, and a light or fog map lights
texel i at its centre, `Pan.X + i × UScale` along U, where both engines draw
it ([a texel's
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
of the first one lit; on the actor's first draw each light its shadow line
finds open lit full at once, a moving light full when it first reaches the
actor, a static one new to it dark until its line is next run; each fade a
byte stepped by the viewport's time since its last frame. An actor drawn on
its own (`Canvas.DrawActor`, `Canvas.DrawClippedActor`, `GC.DrawActor`), a
first-person weapon among them, takes no moving light, only the static
lights and those it kept; an actor's own light counts in the pass, so that
a light under an eighth of its strength lights nothing, but lights nothing
of it. A vertex is lit by its normal alone, whatever its faces' flags, as
the original's `Light` (`Render.dll` `0x10b027f0`) lights each once: a
two-sided face shows on both sides the light its normal gets.
`ViewConsole` with `FovAngle=90`, `HoldAnim=Idle1`, `HoldFrame=0.875` and
`Floor`, both engines at 1024x768: the Dragon's Tooth's hand correlated
at 0.973 with the original's frame, where at 654e675d its own light's pick
left its thumb black (0.773), and the hilt of the one on the floor at
0.897 (0.818); `CrosshairConsole`'s pistol in hand at 0.981 (0.711).
`MeshConsole` with `Hold` and `Extra=DeusEx.Plant1`: the plant's leaves
at 0.997 of the original's brightness, where at 724acfd9 the cosine's size
lit them from behind as from before, 1.113; Paul Denton at a median 1.01
(1.03); `HeldConsole`'s JC from behind, his coat at 1.009 (1.095). The
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

Differs (bugs.md REN.32): a light's texels are not held to the rectangle
Render.dll lights, a cylinder light's to none. Differs (bugs.md REN.33): an
`LE_OmniBumpMap` light lights surfaces. Differs (bugs.md REN.35):
`PF_SmallWavy`, the detail and macro textures' frames, `NoLighting` on
meshes, a pawn's spotlight's aim.

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
original's 8.5. A mover's polygon is drawn a step toward the viewer in the
depth buffer, so one flush with the level's surfaces shows over them; one
the brush tracker filed in the plane of a surface of the level's own,
which hangs after that surface in its plane's chain, is drawn a step away,
so the level's surface shows over it where it covers it, as in the
original's frame. `IntroDoorConsole`: the
trap door under Bob Page and Walton Simons on 00_Intro, its top in the
marble floor's plane, under the floor's lit marble as in the original's
frame (the panel round their feet correlated at 0.91 with it, on Vulkan,
GL and the software device). A mover's polygon takes its texture's own
flags with its own, as the original's walk takes the surface the brush
tracker files for it, its back-face test on the polygon's alone: a grate
whose texture alone is masked (`CoreTexMetal.Metal.metalgrate_a`, the five
under 02_NYC_BatteryPark's Castle Clinton; 93 movers of 29 stock maps
draw such a polygon) shows what lies behind it (`GlassConsole` with
`GlassName=DeusExMover31`: a crate and a sprite behind the grate correlated
at 0.96 with the original's frame on Vulkan, GL and the software device,
where at 94968ca9 its gaps were black, 0.64). A one-sided surface of the
level's own seen from behind is passed over by its own flags, its
texture's taken only after, as the original's walk does: a texture made
two-sided shows none from behind (`BackFaceConsole`: Liberty Island's
chain-link sheet over the statue's pedestal room, its texture
`CoreTexMetal.Metal.ClenChainlink_B` given `bTwoSided`, seen from behind
as with it one-sided in both engines, where at cc9b9ef0 DXEngine drew the
sheet, 31% of the frame's pixels changed). Neither engine draws one the
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
  surfaces. The original files their polygons into the level's BSP and
  drops what lands in solid, a polygon in a wall's plane filed in front of
  the wall's. So one flush with a wall shows over it in both, by the step
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
  refuse (`GlassConsole` with `Destroy`, in both engines).

Differs (bugs.md REN.34): a mover's faces all take the mover's zone, the
original's each its node's.

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

Differs (bugs.md AI.15): a loop that wraps with a `FinishAnim` waiting lets
the wait go with `bAnimFinished` as it was, where the original's sets it.

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
  both engines); an edit field draws its own rows, as the original's.
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
  what it put in, and what it put in selected when asked to.
- **An edit field's copy, cut and paste** go through the desktop's
  clipboard, by choice, so they reach other programs, where the original
  keeps a text buffer of its own: a copy or cut takes the selection and
  nothing with none, as there; the text's 16-bit characters go out as UTF-8,
  and a paste reads the clipboard's UTF-8 back as 16-bit characters, a
  carriage return left out. `PlayerPawn.PasteFromClipboard` and
  `CopyToClipboard` take the desktop's clipboard as UTF-8 so too
  (`PasteConsole`, the clipboard holding "dxcap paste é Ω €": its codes 233,
  937, 8364 alike with the original's). On X11 the paste asks for `STRING`
  and the copy serves Latin-1, so a character past it comes back `?`
  (bugs.md WIN.12).
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
  typed by `xdotool`; U+1F600 two characters). The text windows, the
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
  until the next starts typing (`ThemeConsole`).
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
  each button's clip rectangle, every line alike).
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
  at its edges.
- **Full screen as the window loses the focus** is kept, by choice, where
  the original leaves it and minimises; SDL3 minimises a real exclusive mode
  alone, never on Wayland nor in a desktop's full screen.

In the SDL3 and X11 windows a wheel presses once a notch, as the original
presses once a `WM_MOUSEWHEEL`, which Windows sends a notch: a
high-resolution wheel's and a touchpad's fractions, which X11 and Wayland
send as events of their own, added up and pressed at each notch's half, the
sum begun again where the scroll turns (`WheelConsole` with
`tools/dxcap/wheel.py`'s eighths of a notch, the two windows alike).

An optional argument left out takes the original's default: `LockMouse`
locks, `SetBaselineData` gives 2 and 1, and a button's `SetTextColors`,
`SetButtonColors` and `SetButtonTextures` set all six slots, one left out
green, white or None (`ButtonDefaultsConsole`: a button's six slots set
apart, then each call with its first slot alone, its insensitive and focused
look alike in both engines).

Differs (bugs.md EXT.23): five window classes' own ticks run after their
script's, timers and children. Differs (bugs.md EXT.24): the computer window
makes neither of its children. Differs (bugs.md WIN.12): Alt+Enter back to a
window on X11 puts it at the display's corner.

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
  the window's own and the screen's, a choice resizing the window; by choice.
- **Full screen in a Wayland session**, a size chosen is drawn at its size
  and scaled to the display by the compositor, the display's mode as it was,
  by choice: Wayland lets no program switch the display's mode.
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
at the window's own volume, which is 0 unless the window sets it, so at
Galaxy's floor (below). `SoundConsole`'s root beeps from the screen's left
edge, centre and right edge come out within 0.2 dB of the original's, panned
alike, the one at the root's own volume 41.9 dB under the centre's in both.
A map's load stops the sounds and leaves
the music playing, as Galaxy does when the load hands it the viewport it has
([playing a sound](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#playing-a-sound)):
the old song fades out and the new level's starts (its player's
`MTRAN_Fade`), a level of the same song jumping to its section
(`LoadingConsole` with `DXCAP_RECORD=1 DXCAP_MUSIC=1`: no silence across a
load in either engine, the fade after it, the music on after a save of the
map it is on is loaded). Galaxy's holds on `OutputRate` (8,000 to 48,000 Hz)
and `Latency` (10 to 250 ms) are kept, `Latency` the output's mix-ahead
(10 ms buffers queued to it), and so are those a `SET` puts on what plays,
where `GET` gives the value as set
([settings](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#settings)):
`DopplerSpeed` to 1-100,000 and `AmbientFactor` to 0-10. `AmbientConsole`'s
fan, recorded on both engines, plays 23.2 dB louder at an `AmbientFactor`
set to 20 than at 0.7 in the original and 23.1 dB in DXEngine, as at 10
(29.2 dB without the hold). A voice is never quieter than Galaxy's floor,
1/256 of full
([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)):
a sound gone out of its radius as it plays (an ambient one stops there),
one played at a volume of 0 and one whose own slider is at 0 while the
other is not play on at it, and only both sliders at 0 silence a sound.
`AmbientConsole` with `Factor=0.01`: the fan 14.7 dB under its level at 0.7
in the original and 14.8 dB in DXEngine (37.9 dB without the floor); with
`Volume=1` too, 0.6 and 0.7 dB under, the floor holding the fan at
-60.1 dB in both either way. `UseStereo=False` makes the sounds mono, the music staying stereo (bugs.md AUD.14); `UseFilter`,
`UseCDMusic` and `UseSpatial` are read, saved and `SET` and change nothing,
as in the original. An MP3 -- the game's speech -- plays as Galaxy's
decoder fills its ring, 32,768 frames of each channel
([playing a sound](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#playing-a-sound)):
4,096 frames of silence first, then its sound, then on, silent, until the
fill sets its end, at Galaxy's ticks, timed by the music's tempo -- one
whose data, the silence counted, ends within the first fill to that fill's
end, 20,480 frames, one within the ring's first turn with its data, and a
longer one to within a tick of the ring's turn after its data's end; each
turn after the first passes over the ring's first frame, the ring's loop
ending at its last. Galaxy's first fill is at its first tick after the
voice starts, as its mixer's thread falls against the game's, DXEngine's at
the voice's start, by choice, the original's usual case. `LipConsole` with
`Beep=True` and `DXCAP_RECORD=1`: each of its three lines 4,085 to 4,096
frames after a beep started on the same tick, the original's 4,083 to 4,096,
or 512 more as its voice starts against its mixer's tick; and with `Speech`
naming the stereo `ConAudioMission01_450`, 4,085, the original's 4,107 and
4,608. Lip sync
is Galaxy's
([lip sync](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#lip-sync)):
a shape each tenth of a second of a line's time played, from 1,024 frames
where its voice plays, the first at the update after it starts; an 8-bit
sound's from its first 1,024 bytes wherever it plays, every other one, in
half the FFT's input, the rest kept from the read before (`LipConsole` with
`Speech` on the harness's `Cap8Bit`: 19 `O`s, then two `X`s as the place read
from passes its end, in both engines). A WAV loops by its first loop only from a `smpl` chunk after its data
chunk, as Galaxy's loader sets the sound up afresh at the data (`CapEarlyLoop`
once in both engines), and a voice mixes the frame
that falls on its loop's end or its data's end too.
`GetSoundDuration` is the original's, read from a WAV's header and 0 for an
MP3
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)):
`OpsConsole`'s, a beep's and a mission 1 line's, alike.
`LipConsole`: 30, 6 and 30 shapes on its three lines, the last running to
2.9 s past its sound's 2.6 in both engines; one after another, 78.0% of them
alike between the two engines' runs, where the original's own runs are 79.7%
alike. Differs:

- The music plays on an OpenAL source of its own, by choice.
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
every command, a speed over 32 at 32, an order whose pattern is empty
passed over; read between samples linearly, unramped; each channel panned as
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
Differs (bugs.md AUD.15): the music is heard as it changes, where the
original's comes some 0.9 s after its sounds. Differs (bugs.md AUD.16): a
song started past its first order takes its pans, speed and tempo from its
start, and a loop keeps the pans its patterns set, where Galaxy's sets them
only at order 0; a section is reached by playing the song to it, where
Galaxy's silences the voices and plays on from there; the music is not held
to 16 bits before the sounds are added. Differs
(bugs.md AUD.14): `UseSound=False` is read nowhere.

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
  caller among them (`WalkConsole`). A line or a box started inside an
  actor's cylinder, heading in, stops at its start, its normal level, out
  from the axis
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
  `MoverConsole`).
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
  tracker files them, each split down the tree, a fragment in empty space a
  node of its own, the space behind it solid within its leaf, and one in
  solid dropped, one in a node's plane hung at the end of that plane's
  chain; a mover filed again takes the movers filed under it with it, the
  last filed first. A line's `Trace` and the multi-hit check meet them as
  the level's: the node the line meets, its plane, and the surface the
  original files a polygon under -- the polygon's flags without the
  editor's own (`PF_NoAddToBSP`), with `PF_SpecialLit` for a `bSpecialLit`
  mover, which it draws with its texture's own --, a polygon in the plane
  of the level's own node met as that node; and a box meets none. A brush
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
  **the reach tests** from a pawn's home spot (`ReachConsole`; the others
  not: bugs.md AI.9, PHYS.20, PHYS.21), and **the look up and down stairs**
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
  the upper, so with the bounds the wrong way round the upper wins, and
  `FClamp` takes a NaN to the lower, as the original's x87 compare does; a
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
  both).
- **The engine's own rotations** -- the view's, a mesh's and a mover's, a
  laser's beam, movement's and the AI's -- take the original's sine table,
  as the scripts' conversions do
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

Differs (bugs.md PHYS.22): the line checks' edges and precision:
`VisibleActors`' radius, a line under 0.01, `FastWalkNode` in doubles,
`FastTrace`'s direction, `FindRegion`'s sums.

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
it. `GarbageConsole` and `DeleteConsole` run both engines. Differs by choice:

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
  a press runs its line once per hold, a release only after its press. A
  press's commands go to the input's own words and its aliases, the render
  device's and the viewport's own words (`SHOT`, `EXEC` and the rest), then
  the exec functions -- the console's, the game's, the HUD's, the player's
  and each of its items' in turn -- then the engine's commands; a typed or
  scripted command goes the same way, so `DefUser.ini`'s `GreyStar=Shot`
  takes a shot, and `TIMEDEMO` is the console's own, whose result its
  script logs. A release runs the input's words alone, and a command after
  `OnRelease` the press's way. `Button` sets its byte to 1 on the press and
  to 0 on the release, so a byte a script clears while the key is held
  stays clear until the next press (Deus Ex's player clears `bDuck` as it
  enters water); `Pulse` sets it on the press alone; `Toggle` flips its top
  bit on the press; an alias's line runs with its key's press or release,
  an alias inside it not expanded again (`BindConsole` with
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
  a shift, control or alt its name for both sides, then its side's own
  (`LShift`; `UnknownA4` for the left alt), Caps Lock, the punctuation, the
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
  `axis-keys.txt`: the root window full screen's size and back). By choice,
  the other windows name fewer, and a side's own
  never: the X11 one, the
  harness's, has no Alt but the Meta key, no `` ` ``, no keypad Enter, and
  Print Screen as `Print`; the Wayland one, the engine's own on a Wayland
  desktop (a dev build's, no port's), names each key by its first level, the
  key below Escape `` ` `` whatever the layout, and lacks only the Windows
  and menu keys.
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
`switch` taking `case 1`).

A string constant is the original's: its 8-bit text a byte a character, at
most 1,023, as `execStringConst` widens it (Core.dll `0x10130890`), so bytes
that read as UTF-8 are their own characters (`OpsConsole`: C3 A9 t C2 A9 as
195, 169, 116, 194, 169 in both). A cast and every iterator take the very
class, a mod's class of another's name not it, and `IsA` a class by its name
(`CastConsole`: `DXCapTwin.u`'s `CapMarker` cast and iterated as none of
`DXCapture.u`'s, its `IsA('CapMarker')` true, in both engines).

Differs (bugs.md VM.25): a name keeps each spelling it is made in, where the
original's keeps the first made in the session: `string()` gives the case it
was made in (`SpellingConsole`: `ANNADIES` where the original's gives
DeusEx.u's `annadies`), and the cheats' EditFlags lists a flag as its setter
spelt it; lookups take any case in both. Differs (bugs.md VM.29): small edges no stock path reaches -- a static array
argument read when the call reads it, a call held off copying no `out`
argument back, the original's `GotoState` warnings not logged, an object
made by `new` able to take a state.

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
- **`-lanplay`**: a dedicated server ticking at `LanServerMaxTickRate`, as
  `--lanplay`
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)).
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
  its lines alike in both engines).
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in;
  `exec` runs a file's lines as commands, as the original's does.
- **Safe mode's flags**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music. The original makes no audio
    subsystem; DXEngine keeps its device, on OpenAL Soft's null driver, by
    choice.
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

Differs (bugs.md ENG.19): an unquoted `X=` value is cut at white space
alone, where the original's `Parse` cuts it at a comma too.

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
- **A mod's packages and saves.** A package's or a save's property tag of an
  element past its static array, of another type or of another struct is
  logged and skipped, as the original's loader skips it, the object loading
  on; a struct's static-array members are read every element, its native and
  transient members passed over on disk, as the original's binary has them,
  but a struct extending another written base first, the original's own
  members first (bugs.md PKG.22), and a native or transient property's tag
  loaded, where the original's loader refuses it (bugs.md PKG.23); a
  script's Unicode string constant of more than one character loads on the
  Linux ports; an expression 1,024 deep loads (`DepthConsole`'s 100); a mesh
  whose faces run past its vertices or wedges loads, those faces passed
  over; a mesh DXEngine saves loads back; a palette short of 256 colours is
  padded with black; an uncompressed mip shorter than its size is padded
  with black, a compressed one not, which GL and GL ES read past (bugs.md
  REN.38); a map whose surfaces or nodes point outside its arrays -- a
  node's links, zones, surface, render bound, vertices and their points, and
  its collision hull's list -- is refused with their numbers, the vertex
  pool's entries no node names left alone, every map of the game having
  thousands past its points (`MapWalkConsole`: every map of the game loads);
  a fire texture's sparks and drops outside it are masked into it; a
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
  sets the flag, and a decal is drawn with its surface); the render device's
  `Translucency` is read, saved and `SET`, and changes nothing, as under
  `D3DDrv` and `OpenGLDrv`, which have no such setting -- of the game's
  devices only the software one names it
  ([the binary](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-binary));
  an actor at no place (a NaN location) sorts
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
  client opens four downloads at once at most; a `TcpLink`'s failed connect
  waits for the script's timer on Linux as on Windows, nothing is read while
  connecting, and a listening link's close lets its port go.
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
  player's as it spawns one, the client's, the audio and render devices'
  (`IniConsole`). Those read by hand do the same: an empty float is 0, and
  an empty texture or skin detail, an enum's byte in the original, keeps its
  default (`IniConsole`: the player's `MouseSensitivity`, `Bob`,
  `DodgeClickTime`, `Handedness` and `MouseSmoothThreshold` given empty, 0
  in both engines). An ini or a `.int` file is read as the original's
  `appLoadFileToString` reads it, UTF-16 after a byte-order mark and else a
  byte a character, and an ini written back is written as its
  `appSaveStringToFile` writes one, UTF-16 after `FF FE` when a character in
  it is past Latin-1 and else a byte a character ([the
  configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration);
  `ConfigConsole`: a config string of U+03A9, U+2122 and e acute written by
  each engine as UTF-16 and read back whole by each; `LocalizeConsole`: a
  UTF-16 `.int` of a mod's folder, U+03A9, U+2122 and e acute, and a Latin-1
  one's e acute, whole). A script's console command takes its text so too,
  16-bit (`OpsConsole`: `set` of U+03A9, U+2122, a, alike in both). An ini
  written back, at a clean exit, has a changed value changed in its own
  line, a key new to a section added after the section's last key, a section
  new to the file at its end, and the rest of the file as it was, a CR LF
  file's line endings with it, by choice, so a player's or a mod's comments
  and layout stay; a file not there yet is written whole, CR LF, as the
  original writes it (`ConfigConsole` with no `DXCapConfig.ini`: the file
  each engine makes the same to the byte). The original rewrites each file
  whole, CR LF, comments gone (`ConfigConsole`: a class's own
  `DXCapConfig.ini`, CR LF, read and written back alike). `StaticSaveConfig`
  from an object writes the class's defaults, as from the class
  (`ConfigConsole`). A config value of any type is read and written as the
  original's import and export have it (a property's text, above): a
  struct's members, a class or an object as `Type'Path'`, a dynamic array
  one line of its name per element (`ConfigConsole`: a vector, a class and
  two strings written and read back), but for the reader's: a value's quotes
  kept, a key's and a value's white space trimmed (bugs.md PKG.21).
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
with `AnimEndJoinConsole`. DXEngine joins live
public servers, the mods they run included (MTL, ANNA, CDX, DXAG, SG), and
stays in. A replicated string goes as the original's does, 16-bit when a
character in it is past 0xFF, else 8-bit (`JoinConsole`: its player renamed
DXCapture, U+03A9, U+2122 through `SetName`, the name back whole from each
engine's server to the other's client); a `TcpLink`'s and a `UdpLink`'s
text goes 8-bit, a character past 0xFF as 0x7F, as the original's `ToAnsi`
narrows it, and comes in a byte a character (`LinkConsole`: A, U+03A9, e
acute sent to itself received as 65, 127, 233 in both engines). The rate it asks of a server is 20,000 where the ini holds the
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
  handshake are the original's, and so is its socket: broadcast and address
  reuse on, 32 KB buffers for a client's and 128 KB for a server's, a
  server's port the URL's or the next free of the 20 from it, or with
  `PORT=` on the command line that port alone, its address every one of the
  machine's but the one `MULTIHOME=` names or, with `-PRIMARYNET`, the first
  of the machine's own name (`ServeConsole` with `PORT=7795`, then with
  `MULTIHOME=127.0.0.1`: the serving URL's address `0.0.0.0:7795`, then
  `127.0.0.1` with no port, alike in both engines); a server's host name is looked up on a
  thread, the game going on; `GETPING` and `GETLOSS` answer the server
  connection's best lag of the last second in ms and its incoming packet
  loss in percent, as the original's do.
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
  ([housekeeping](#housekeeping-not-seen-directly)).
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
  `SendBinary` answers False; no mod can have counted on them. DXEngine's
  `SendText` and `SendBinary` answer False when nothing went.
- **The player's name in a net game**: a name the player never chose --
  none, or the game's stock `Player` -- logs in as `DXEnginePlayer`, a
  client's to its server and a listen server's host's, by choice, so
  DXEngine's players show as such (`Engine::SetMultiplayerName`). A name the
  player chose is kept; single player, the menus and the inis keep the
  game's.

As in the original:

- A root window per pawn, made at its first Possess and kept in it, as the
  original's `InitRootWindow` makes it; the engine draws, ticks and feeds
  the viewport's own player's: a client joining a listen server gets a root
  of its own and leaves the host's in place (`ServeConsole` with
  `JoinConsole`: the server's log names each root's pawn).
- On a dedicated server `GetPlayerPawn` answers None, as the original's
  with no client or viewport, and `AttachDecal` attaches none, as the
  original's with no client.
- In a URL a '/' is a character of a map's name or an option's value
  (`?Name=AC/DC` keeps its slash), '#' alone marking the portal, where the
  original's takes a '/' in the map's part for the portal's mark (bugs.md
  NET.13, with the URL's other departures); a relative
  travel that names a server's address goes there; a URL cleared is empty
  whole, its port the game's default again.
- Entry's actors find their regions in Entry's own model, with its zone
  actors and its LevelInfo, while a map is current, as the original's
  (`EntryConsole`: markers spawned into Entry from Liberty Island, as the
  game's server lists spawn their links there, and moved, alike in both
  engines). A level's LevelInfo is in its own zone, in no leaf and zone 0,
  wherever it stands, as the original's `SetActorZone` puts it, where
  DXEngine found it by its place (Entry's in zone 1, leaf 4).
- A client replicates its own player's pawn to the server each tick, before
  its packets go (`ULevel::TickNetClient`): the values the script classes'
  statements send from a client, none of the engine classes' native lists,
  which send only from the server. Deus Ex's `FrobTarget` goes so: the
  client finds what the player looks at, and the server's `ParseRightClick`
  takes it, so a weapon or an item is picked up on a net game only so
  (`PickServer` and `PickJoinConsole`, each engine as server and as client).
  Each side keeps what the other last got of each value, and a value goes
  when its property's compare finds it apart from that, a string's without
  case, as the original's `Identical` and its native lists' `appStricmp`
  take it: a name changed in case alone does not go (read: Engine.dll
  `0x103fead0`, `0x103616e0`).
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

Differs (bugs.md NET.11): a server's FAILCODE takes the place of the join's
failure text. Differs (bugs.md NET.12): a login's value ends at `)`. Differs
(bugs.md NET.14): a login with a FailCode and no Error, `SendText`'s answer,
a client travel's info, `open`'s one word. Differs (bugs.md NET.16): a
simulated proxy pawn's own path, its move along its velocity and its `Tick`
alone, runs only in a net game, where the original's runs whatever the net
mode; no stock actor is one outside a net game. Differs (bugs.md NET.17):
the native replication lists' floats compared by value, the original's by
their bits, so a change from 0.0 to -0.0 does not go.

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
