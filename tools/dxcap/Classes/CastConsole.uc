//=============================================================================
// CastConsole: a class told from another of its name, logged with "DXCAST:"
// from wherever the run starts, 2 s in (docs/DEVELOPMENT.md, scripted runs).
//
// DXCapTwin.u is this package again under another name (tools/dxcap.sh
// compile), so its CapMarker is a class of this package's CapMarker's name
// and not it. One of each is spawned 200 over the player, owned by it and
// based on it; then each one's cast to this package's CapMarker and its
// IsA('CapMarker'); the actors AllActors, BasedActors, ChildActors,
// RadiusActors, ZoneActors and CycleActors give of this package's CapMarker,
// and the objects AllObjects gives; ClassIsChildOf and a class cast of the
// twin's class to it; then both destroyed, and an exit. The original tells
// the classes by pointer (Core.dll's execDynamicCast 0x10130ed0 and
// execAllObjects 0x1013f1f0, Engine.dll's iterators, as IsA 0x101155f0)
// and IsA(name) by name (execIsA 0x1013ee60). What is compared between the
// engines: every line, exactly.
//=============================================================================
class CastConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXCAST: " $ S);
}

function Probes(PlayerPawn P)
{
	local class<Actor> TwinClass;
	local Actor Own, Twin;
	local CapMarker M;
	local ZoneInfo Z;
	local int NAll, NBased, NChild, NRadius, NZone, NCycle, NObjects, Index;

	TwinClass = class<Actor>(DynamicLoadObject("DXCapTwin.CapMarker", class'Class'));
	if (TwinClass == None)
	{
		Say("no DXCapTwin.CapMarker: tools/dxcap.sh compile");
		return;
	}
	Own = P.Spawn(class'CapMarker', P,, P.Location + vect(0, 0, 200));
	Twin = P.Spawn(TwinClass, P,, P.Location + vect(0, 0, 200));
	if (Own == None || Twin == None)
	{
		Say("spawned " $ Own $ " and " $ Twin);
		return;
	}
	Own.SetBase(P);
	Twin.SetBase(P);
	Say("classes " $ string(Own.Class) $ " " $ string(Twin.Class));
	Say("cast own " $ (CapMarker(Own) != None) $ ", twin " $ (CapMarker(Twin) != None));
	Say("IsA('CapMarker') own " $ Own.IsA('CapMarker') $ ", twin " $ Twin.IsA('CapMarker'));
	foreach P.AllActors(class'CapMarker', M)
		NAll++;
	foreach P.BasedActors(class'CapMarker', M)
		NBased++;
	foreach P.ChildActors(class'CapMarker', M)
		NChild++;
	foreach P.RadiusActors(class'CapMarker', M, 1000)
		NRadius++;
	Z = Own.Region.Zone;
	if (Z != None)
	{
		foreach Z.ZoneActors(class'CapMarker', M)
			NZone++;
	}
	foreach P.CycleActors(class'CapMarker', M, Index)
		NCycle++;
	foreach AllObjects(class'CapMarker', M)
		NObjects++;
	Say("AllActors " $ NAll $ ", BasedActors " $ NBased $ ", ChildActors " $ NChild $ ", RadiusActors " $ NRadius $ ", ZoneActors " $ NZone $ ", CycleActors " $ NCycle $ ", AllObjects " $ NObjects);
	Say("ClassIsChildOf " $ ClassIsChildOf(TwinClass, class'CapMarker') $ ", class cast " $ (class<CapMarker>(TwinClass) != None));
	Own.Destroy();
	Twin.Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes(Viewport.Actor);
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
