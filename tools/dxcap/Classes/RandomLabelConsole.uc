//=============================================================================
// RandomLabelConsole: a conversation's random label event as its cycling
// flags pick, logged with "DXRANDLABEL:".
//
// The original's DConEventRandomLabel::execGetRandomLabel (ConSys.dll
// 0x1000a270) sets bLabelsCycled when cycleIndex has reached the label
// count; picks at random when not cycling, or with bCycleRandom once the
// labels have cycled; else with bCycleOnce stays on the last label once
// cycleIndex is on it, never counting past it; else takes the label at
// cycleIndex modulo the count and counts on.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, the random label event of
// three labels or more in BoatWaitingForBriefing (the engines' loaded
// conversations differ: the original's have AlexJacobson's barks too), its
// labels logged. Then four rounds from
// cycleIndex 0, bLabelsCycled False: cycling alone, with bCycleOnce, with
// bCycleRandom, with both, each the label count and two calls more, each
// call's label, cycleIndex and bLabelsCycled logged -- but for the label of
// a random pick (bCycleRandom's round past its cycle), which differs between
// the engines' random numbers. The event's own values put back, then an
// exit. Run DXEngine's from the menu map, as the original's.
//=============================================================================
class RandomLabelConsole extends Console;

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

function Round(ConEventRandomLabel R, string Which, bool bOnce, bool bRandom)
{
	local int i, n;
	local string L;

	n = R.GetLabelCount();
	R.bCycleEvents = True;
	R.bCycleOnce = bOnce;
	R.bCycleRandom = bRandom;
	R.cycleIndex = 0;
	R.bLabelsCycled = False;
	for (i = 0; i < n + 2; i++)
	{
		L = R.GetRandomLabel();
		if (bRandom && !bOnce && i >= n)
			L = "(random)";
		Log("DXRANDLABEL: " $ Which $ " call " $ i $ ": " $ L $ ", cycleIndex " $ R.cycleIndex $ ", cycled " $ R.bLabelsCycled);
	}
}

function Probe()
{
	local Conversation C, BestCon;
	local ConEvent E;
	local ConEventRandomLabel R, Best;
	local bool bEvents, bOnce, bRandom, bCycled;
	local int i, Index;

	foreach AllObjects(class'Conversation', C)
	{
		for (E = C.eventList; E != None; E = E.nextEvent)
		{
			R = ConEventRandomLabel(E);
			if (Best == None && R != None && R.GetLabelCount() >= 3 && C.conName == 'BoatWaitingForBriefing')
			{
				BestCon = C;
				Best = R;
			}
		}
	}
	if (Best == None)
	{
		Log("DXRANDLABEL: no random label event of three labels in BoatWaitingForBriefing");
		return;
	}
	Log("DXRANDLABEL: " $ BestCon.conName $ "'s event, " $ Best.GetLabelCount() $ " labels, cycling " $ Best.bCycleEvents $ ", once " $ Best.bCycleOnce $ ", random " $ Best.bCycleRandom);
	for (i = 0; i < Best.GetLabelCount(); i++)
		Log("DXRANDLABEL: label " $ i $ " " $ Best.GetLabel(i));

	bEvents = Best.bCycleEvents;
	bOnce = Best.bCycleOnce;
	bRandom = Best.bCycleRandom;
	Index = Best.cycleIndex;
	bCycled = Best.bLabelsCycled;
	Round(Best, "cycling", False, False);
	Round(Best, "once", True, False);
	Round(Best, "random", False, True);
	Round(Best, "once and random", True, True);
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
		Log("DXRANDLABEL: done");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
