//=============================================================================
// RandomLabelBelowConsole: a random label event's label for a cycleIndex a
// script set below 0, logged with "DXRANDBELOW:" (bugs.md DX.7).
//
// The original's GetRandomLabel (ConSys.dll 0x1000a270), cycling, takes the
// label at cycleIndex modulo the count, which C's remainder leaves below 0
// for an index below 0: it copies a string from before its label array,
// whatever the allocator left there -- nothing when those bytes are zero,
// else stray text, or a fault. DXEngine's gives no label. The original's run
// may end in its crash dialog; that is its result.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, BoatWaitingForBriefing's
// random label event of three labels or more, cycling alone, from cycleIndex
// -1 and then -2: each label's length and first 32 characters, and
// cycleIndex after. The event's own values put back, then an exit. Run
// DXEngine's from the menu map, as the original's.
//=============================================================================
class RandomLabelBelowConsole extends Console;

var float MapTime;
var int Phase;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function Pick(ConEventRandomLabel R, int Index)
{
	local string L;

	R.cycleIndex = Index;
	Log("DXRANDBELOW: from cycleIndex " $ Index);
	L = R.GetRandomLabel();
	Log("DXRANDBELOW: label of " $ Len(L) $ " characters [" $ Left(L, 32) $ "], cycleIndex " $ R.cycleIndex);
}

function Probe()
{
	local Conversation C;
	local ConEvent E;
	local ConEventRandomLabel R, Best;
	local bool bEvents, bOnce, bRandom, bCycled;
	local int Index;

	foreach AllObjects(class'Conversation', C)
	{
		for (E = C.eventList; E != None; E = E.nextEvent)
		{
			R = ConEventRandomLabel(E);
			if (Best == None && R != None && R.GetLabelCount() >= 3 && C.conName == 'BoatWaitingForBriefing')
				Best = R;
		}
	}
	if (Best == None)
	{
		Log("DXRANDBELOW: no random label event of three labels in BoatWaitingForBriefing");
		return;
	}
	Log("DXRANDBELOW: " $ Best.GetLabelCount() $ " labels");
	bEvents = Best.bCycleEvents;
	bOnce = Best.bCycleOnce;
	bRandom = Best.bCycleRandom;
	Index = Best.cycleIndex;
	bCycled = Best.bLabelsCycled;
	Best.bCycleEvents = True;
	Best.bCycleOnce = False;
	Best.bCycleRandom = False;
	Best.bLabelsCycled = False;
	Pick(Best, -1);
	Pick(Best, -2);
	Best.bCycleEvents = bEvents;
	Best.bCycleOnce = bOnce;
	Best.bCycleRandom = bRandom;
	Best.cycleIndex = Index;
	Best.bLabelsCycled = bCycled;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0 && MapTime > 3.0)
	{
		Log("DXCAP: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Phase = 1;
	}
	else if (Phase == 1 && M ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
	{
		Probe();
		Log("DXRANDBELOW: done");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
