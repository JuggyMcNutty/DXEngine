//=============================================================================
// ConPackageConsole: ConBindEvents under the level's ConversationPackage
// set to what no stock map has, logged with "DXCONPKG:".
//
// The original's ConBindEvents (DeusEx.dll 0x10014320) loads
// <package>Text.ConList_MissionNN without a warning (LOAD_NoWarn),
// DeusExConversations, in any case, becoming DeusExCon: a package or a list
// that is not there binds nothing and keeps the actor's list, an empty
// package too, Text.ConList_MissionNN being none.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, Paul Denton's bound
// conversations counted, then with his list cleared each time, the level's
// ConversationPackage empty, NoSuchConPackage and deusexconversations, then
// as it was, each followed by his ConBindEvents and the count. Then an exit.
// Run DXEngine's from the menu map, as the original's.
//=============================================================================
class ConPackageConsole extends Console;

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

function int Count(Actor A)
{
	local ConListItem Item;
	local int n;

	for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		n++;
	return n;
}

function Bind(ScriptedPawn Paul, DeusExLevelInfo Info, string Package)
{
	Paul.ConListItems = None;
	Info.ConversationPackage = Package;
	Paul.ConBindEvents();
	Log("DXCONPKG: package [" $ Package $ "]: " $ Count(Paul) $ " bound");
}

function Probe(PlayerPawn P)
{
	local ScriptedPawn S, Paul;
	local DeusExLevelInfo Info;
	local string Old;

	foreach P.AllActors(class'ScriptedPawn', S)
		if (S.BindName == "PaulDenton")
			Paul = S;
	foreach P.AllActors(class'DeusExLevelInfo', Info)
		break;
	if (Paul == None || Info == None)
	{
		Log("DXCONPKG: no Paul or no DeusExLevelInfo");
		return;
	}
	Old = Info.ConversationPackage;
	Log("DXCONPKG: the level's package [" $ Old $ "], Paul's " $ Count(Paul) $ " bound");
	Bind(Paul, Info, "");
	Bind(Paul, Info, "NoSuchConPackage");
	Bind(Paul, Info, "deusexconversations");
	Bind(Paul, Info, Old);
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
		Log("DXCONPKG: done");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
