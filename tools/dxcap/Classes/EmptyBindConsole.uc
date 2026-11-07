//=============================================================================
// EmptyBindConsole: conversations with no owner, bound by an actor with no
// names, logged with "DXEMPTYBIND:" (bugs.md DX.8).
//
// The original's DConversationList::BindConversations (ConSys.dll
// 0x10010b70) compares a conversation's owner with the actor's names by
// appStricmp, an empty name as any other: a bark ("_Bark" in its name) is
// the actor's whose BarkBindName is its owner; any conversation, but a bark
// when the actor has a BarkBindName, the actor's whose BindName is its
// owner. The three ConBindEvents (DeusEx.dll 0x1000ea50, 0x10014320,
// 0x10019140), which ScriptedPawn's, DeusExPlayer's and DeusExDecoration's
// PostPostBeginPlay call, bind whatever the actor's names. No stock
// conversation has no owner, and no stock script clears one.
//
// From the menu map, 01_NYC_UNATCOIsland; 3 s in, the first plain
// conversation in Paul Denton's list and the first bark in his (else in the
// first pawn's list that has one) lose their owner; a CapBindDeco -- no
// BindName, no BarkBindName -- is spawned, its PostPostBeginPlay binding
// it, and its items are logged; then its names set empty and its
// ConBindEvents called; then its BarkBindName set to DXCapBark and its
// ConBindEvents again; then the two owners put back and its ConBindEvents
// again, each logged the same way. Then an exit. The original binds both
// twice, then the plain one alone, then none.
//=============================================================================
class EmptyBindConsole extends Console;

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

function bool IsBark(Conversation C)
{
	return InStr(string(C.conName), "_Bark") >= 0;
}

// The first conversation in the actor's list that is a bark, or that is not
function Conversation FirstOf(Actor A, bool bBark)
{
	local ConListItem Item;

	for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		if (Item.con != None && IsBark(Item.con) == bBark)
			return Item.con;
	return None;
}

// The actor's items in its list's order: how many, then each conversation
function string Items(Actor A)
{
	local ConListItem Item;
	local string S;
	local int n;

	for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
	{
		n++;
		if (Item.con == None)
			S = S $ " None";
		else
			S = S $ " " $ Item.con.conName;
	}
	return n $ " items:" $ S;
}

function Probe(PlayerPawn P)
{
	local ScriptedPawn S, Paul;
	local Conversation Plain, Bark;
	local string PlainOwner, BarkOwner;
	local CapBindDeco D;

	foreach P.AllActors(class'ScriptedPawn', S)
		if (S.BindName == "PaulDenton")
			Paul = S;
	if (Paul == None)
	{
		Log("DXEMPTYBIND: no Paul");
		return;
	}
	// Paul's first plain conversation; his first bark, else the first in
	// the first pawn's list that has one
	Plain = FirstOf(Paul, False);
	Bark = FirstOf(Paul, True);
	foreach P.AllActors(class'ScriptedPawn', S)
		if (Bark == None)
			Bark = FirstOf(S, True);
	if (Plain == None || Bark == None)
	{
		Log("DXEMPTYBIND: no plain conversation or no bark: Paul's " $ Items(Paul));
		return;
	}
	PlainOwner = Plain.conOwnerName;
	BarkOwner = Bark.conOwnerName;
	Log("DXEMPTYBIND: plain " $ Plain.conName $ " owner " $ PlainOwner $ ", bark " $ Bark.conName $ " owner " $ BarkOwner);

	Plain.conOwnerName = "";
	Bark.conOwnerName = "";
	D = P.Spawn(class'CapBindDeco',,, P.Location + vect(0, 0, 200));
	if (D == None)
	{
		Log("DXEMPTYBIND: no CapBindDeco spawned");
	}
	else
	{
		Log("DXEMPTYBIND: spawned, BindName '" $ D.BindName $ "' BarkBindName '" $ D.BarkBindName $ "': " $ Items(D));
		D.BindName = "";
		D.BarkBindName = "";
		D.ConBindEvents();
		Log("DXEMPTYBIND: no names, bound again: " $ Items(D));
		D.BarkBindName = "DXCapBark";
		D.ConBindEvents();
		Log("DXEMPTYBIND: BarkBindName DXCapBark, bound again: " $ Items(D));
		D.BarkBindName = "";
	}
	Plain.conOwnerName = PlainOwner;
	Bark.conOwnerName = BarkOwner;
	if (D != None)
	{
		D.ConBindEvents();
		Log("DXEMPTYBIND: owners back, bound again: " $ Items(D));
		D.Destroy();
	}
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
		Log("DXEMPTYBIND: done");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
