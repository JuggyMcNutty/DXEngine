//=============================================================================
// BindOrderConsole: which actor a conversation's speech
// binds when several actors share the name it names, logged with "DXBIND:".
//
// The original's DConversation::BindEvents (ConSys.dll 0x1000fec0) offers
// every Pawn in memory by object index, then every Decoration, then every
// PlayerPawn again; each match overwrites, so the last offered wins. DXEngine
// (Packages/ConSys/UConversation.cpp BindEvents) offers the level's actors in
// the level's order, once each.
//
// 1. 06_HongKong_WanChai_Underworld, 3 s in: RightHandHack (owner MaxChen),
//    whose speeches name RightHandMan, which TriadRedArrow5 and
//    TriadRedArrow6 both are. BindEvents with MaxChen as the invoker; each
//    speech's speaker and speakingTo logged, then the bound actors.
// 2. Same map: a decoration spawned, then a TriadRedArrow, both named
//    RightHandMan, and the bind again. The original offers the pawns first
//    and the decoration after them, so binds the decoration; DXEngine's
//    level order ends with the spawned pawn.
// 3. 08_NYC_Street, 3 s in: DL_Exit and DL_JockShipWarning (owner JCDenton),
//    whose speech names Jock, which BlackHelicopter0 and BlackHelicopter1
//    both are; the player the invoker. Then an exit.
// Both engines bind the same actors in all three, the spawned pawn of 2
// refused in both; only the order the bound actors are listed in differs
// (bugs.md, refuted). Run DXEngine's from the menu map, as the original's.
//=============================================================================
class BindOrderConsole extends Console;

var float MapTime;
var int Phase;
var string CurrentMap;
var Actor Bound[10];

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

function Conversation FindCon(name ConName)
{
	local Conversation C;
	foreach AllObjects(class'Conversation', C)
		if (C.conName == ConName)
			return C;
	return None;
}

function Actor FindByBindName(PlayerPawn P, string BN)
{
	local Actor A;
	foreach P.AllActors(class'Actor', A)
		if (A.BindName ~= BN)
			return A;
	return None;
}

function Dump(PlayerPawn P, name ConName, Actor Invoker, string Shared, string Step)
{
	local Conversation C;
	local ConEvent E;
	local ConEventSpeech S;
	local Actor A;
	local int i;

	C = FindCon(ConName);
	if (C == None)
	{
		Log("DXBIND: " $ Step $ " no conversation " $ ConName);
		return;
	}
	foreach P.AllActors(class'Actor', A)
		if (A.BindName ~= Shared)
			Log("DXBIND: " $ Step $ " candidate " $ A.Name $ " (" $ A.Class.Name $ ") at " $ A.Location);
	for (i = 0; i < 10; i++)
		Bound[i] = None;
	C.BindEvents(Bound, Invoker);
	for (E = C.eventList; E != None; E = E.nextEvent)
	{
		S = ConEventSpeech(E);
		if (S != None)
			Log("DXBIND: " $ Step $ " " $ ConName $ " speech " $ S.speakerName $ " -> " $ S.speaker.Name $ ", to " $ S.speakingToName $ " -> " $ S.speakingTo.Name);
	}
	for (i = 0; i < 10; i++)
		if (Bound[i] != None)
			Log("DXBIND: " $ Step $ " bound " $ i $ " " $ Bound[i].Name);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor MaxChen, D, Q;
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
		Log("DXCAP: opening 06_HongKong_WanChai_Underworld");
		P.ConsoleCommand("open 06_HongKong_WanChai_Underworld");
		Phase = 1;
	}
	else if (Phase == 1 && M ~= "06_HongKong_WanChai_Underworld" && MapTime > 3.0)
	{
		MaxChen = FindByBindName(P, "MaxChen");
		Log("DXBIND: invoker " $ MaxChen);
		Dump(P, 'RightHandHack', MaxChen, "RightHandMan", "map");

		// A decoration, then a pawn, both RightHandMan, by Max Chen.
		D = P.Spawn(class'DeusEx.Basketball',,, MaxChen.Location + vect(0, 0, 64));
		if (D != None)
		{
			D.SetCollision(false, false, false);
			D.BindName = "RightHandMan";
		}
		Q = P.Spawn(class'DeusEx.TriadRedArrow',,, MaxChen.Location + vect(80, 0, 0));
		if (Q != None)
			Q.BindName = "RightHandMan";
		Log("DXBIND: spawned " $ D $ " then " $ Q);
		Dump(P, 'RightHandHack', MaxChen, "RightHandMan", "spawned");

		Log("DXCAP: opening 08_NYC_Street");
		P.ConsoleCommand("open 08_NYC_Street");
		Phase = 2;
	}
	else if (Phase == 2 && M ~= "08_NYC_Street" && MapTime > 3.0)
	{
		Dump(P, 'DL_Exit', P, "Jock", "street");
		Dump(P, 'DL_JockShipWarning', P, "Jock", "street");
		Log("DXBIND: done");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
