//=============================================================================
// CarryConsole: what a map exit carries besides the inventory chain, and in
// what order. From wherever the run starts a new game is begun on Liberty
// Island as the main menu's New Game begins one (StartNewGame: the pistol,
// the prod and the medkit on the belt's 1 to 3, the key ring on its 0). 6 s
// in, two pickups are spawned beside the player, neither given, and each is
// named by one of the player's travel properties alone: binoculars by
// DeusExPlayer's ClientinHandPending, soy food by Pawn's SelectedItem --
// two that nothing in a standalone game changes while nothing is in hand
// or being put there. What the player carries is logged with "DXCARRY:",
// item by item (its class, state, belt flag and slot, bTravel), with the
// two properties and the belt's ten slots; then the island is left for
// UNATCO HQ as a map exit leaves it (ClientTravel, TRAVEL_Relative, with
// the items). 6 s into UNATCO HQ the same is logged and the run exits.
//
// The arrival gives the travelled items their TravelPreAccept last to
// first, in the order the export wrote them, with the HUD's belt already
// made (the login's Possess makes the root window): an item on the chain
// goes back to its own slot, taking it from whatever holds it, and one off
// the chain joins it and goes to the belt's first free slot
// (DeusExPlayer.AddInventory). The original's export walks a class's own
// travel properties before its super's (Engine.dll 0x10391740), Actor's
// Inventory last, so the two pickups take theirs after every chain item:
// the soy food slot 4, the binoculars slot 5. Were the chain's head, the
// medkit, to take its own after them, the binoculars would take its slot 3
// and lose it to the medkit: off the belt, bInObjectBelt False, beltPos -1.
//
// With Untravelled (DXCAP_CONFIG "Untravelled=True") the binoculars and the
// prod have their bTravel cleared before the exit. The original's export
// writes an actor only with bTravel set, and walks nothing past one it
// does not write: the binoculars stay behind, ClientinHandPending arriving
// as the new player's own, None; the chain ends at the medkit, the prod,
// its ammo, the pistol and its ammo staying behind, and the key ring comes
// by the player's KeyRing alone.
//=============================================================================
class CarryConsole extends Console;

var config bool Untravelled;
var int Step;
var float MapTime;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function string StateOf(Actor A)
{
	if (A == None)
		return "None";
	return A.Class.Name $ " " $ A.GetStateName();
}

// What the player carries, item by item, the two properties that name the
// pickups and the key ring, then the belt's slots.
function Carried(DeusExPlayer P, string Label)
{
	local Inventory Inv;
	local DeusExRootWindow Root;
	local int Count, i;

	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
	{
		Count++;
		Log("DXCARRY: " $ Label $ " carries " $ StateOf(Inv) $ " belt " $ Inv.bInObjectBelt $ " " $ Inv.beltPos
			$ " bTravel " $ Inv.bTravel);
		if (Count > 64)
			break;
	}
	Log("DXCARRY: " $ Label $ " carries " $ Count $ " items");
	Log("DXCARRY: " $ Label $ " ClientinHandPending " $ StateOf(P.ClientinHandPending) $ ", SelectedItem "
		$ StateOf(P.SelectedItem) $ ", KeyRing " $ StateOf(P.KeyRing) $ ", inHand " $ StateOf(P.inHand));

	Root = DeusExRootWindow(P.rootWindow);
	if (Root == None || Root.hud == None || Root.hud.belt == None)
	{
		Log("DXCARRY: " $ Label $ " no belt");
		return;
	}
	for (i = 0; i < 10; i++)
		Log("DXCARRY: " $ Label $ " belt slot " $ i $ " " $ StateOf(Root.hud.belt.objects[i].GetItem()));
}

// The two pickups, spawned beside the player and named by its travel
// properties alone; with Untravelled, they and the prod lose bTravel.
function SetUp(DeusExPlayer P)
{
	local Inventory Glasses, Food, Inv;

	Glasses = P.Spawn(class'Binoculars', , 'DXCarryGlasses', P.Location + 48 * vector(P.Rotation));
	Food = P.Spawn(class'SoyFood', , 'DXCarryFood', P.Location + 48 * vector(P.Rotation) + vect(0,0,32));
	if (Glasses == None || Food == None)
		Log("DXCARRY: no room for the pickups: binoculars " $ (Glasses != None) $ ", soy food " $ (Food != None));
	P.ClientinHandPending = Glasses;
	P.SelectedItem = Food;
	if (Untravelled)
	{
		if (Glasses != None)
			Glasses.bTravel = False;
		for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
		{
			if (Inv.IsA('WeaponProd'))
				Inv.bTravel = False;
		}
		Log("DXCARRY: the binoculars' and the prod's bTravel cleared");
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXCARRY: in " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXCARRY: StartNewGame 01_NYC_UNATCOIsland");
		P.ConsoleCommand("StartNewGame 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 6.0)
	{
		SetUp(DXP);
		Carried(DXP, "island");
		Log("DXCARRY: travelling to 01_NYC_UNATCOHQ with the items");
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, True);
		Step = 2;
	}
	else if (Step == 2 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 6.0)
	{
		Carried(DXP, "HQ");
		Log("DXCARRY: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
