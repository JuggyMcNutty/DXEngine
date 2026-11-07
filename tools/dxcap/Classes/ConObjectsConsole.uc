//=============================================================================
// ConObjectsConsole: the objects the conversation natives make, as a script
// sees them, logged with "DXCONOBJ:" (bugs.md DX.7).
//
// The original's DConversationList::BindConversations (ConSys.dll
// 0x10010b70) makes each of an actor's ConListItems in the list's package,
// named after its class and a number (ConListItemN) and public, the actor's
// old ones deleted at once (PurgeBoundConversations 0x10010cf0), so gone to
// AllObjects; CreateConCamera (0x100106f0) and CreateFlagRef (0x10010600)
// make theirs in package DeusEx, named and public.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, Paul Denton's first
// ConListItem -- its path, name, ObjectFlags and Outer --, his items counted
// and every ConListItem (AllObjects); his ConBindEvents again, then both
// counts and his first item again; then two cameras and a flag reference
// made by his first conversation, each logged the same way. Then an exit.
// No old item is read after the second bind: the original's are freed. Run
// DXEngine's from the menu map, as the original's.
//=============================================================================
class ConObjectsConsole extends Console;

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

function string Describe(Object O)
{
	if (O == None)
		return "None";
	return string(O) $ ", name " $ O.Name $ ", flags " $ O.ObjectFlags $ ", outer " $ O.Outer;
}

function int CountOf(Actor A)
{
	local ConListItem Item;
	local int n;

	for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		n++;
	return n;
}

function int CountAll()
{
	local ConListItem Item;
	local int n;

	foreach AllObjects(class'ConListItem', Item)
		n++;
	return n;
}

function Probe(PlayerPawn P)
{
	local ScriptedPawn S, Paul;
	local Conversation C;

	foreach P.AllActors(class'ScriptedPawn', S)
		if (S.BindName == "PaulDenton")
			Paul = S;
	if (Paul == None || ConListItem(Paul.ConListItems) == None)
	{
		Log("DXCONOBJ: no Paul, or no list");
		return;
	}
	Log("DXCONOBJ: Paul's first item " $ Describe(Paul.ConListItems));
	Log("DXCONOBJ: Paul's items " $ CountOf(Paul) $ ", every item " $ CountAll());
	Paul.ConBindEvents();
	Log("DXCONOBJ: bound again: Paul's items " $ CountOf(Paul) $ ", every item " $ CountAll());
	Log("DXCONOBJ: Paul's first item " $ Describe(Paul.ConListItems));
	C = ConListItem(Paul.ConListItems).con;
	Log("DXCONOBJ: camera " $ Describe(C.CreateConCamera()));
	Log("DXCONOBJ: camera " $ Describe(C.CreateConCamera()));
	Log("DXCONOBJ: flag ref " $ Describe(C.CreateFlagRef('DXCapFlag', True)));
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
		Probe(P);
		Log("DXCONOBJ: done");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
