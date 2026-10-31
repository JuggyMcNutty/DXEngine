//=============================================================================
// LootConsole: carcasses looted and the inventory grid after, logged with
// "DXLOOT:" and the console's clock (docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island: an isolated ScriptedPawn killed outright and its
// carcass searched (DeusExCarcass.Frob, the player's own frob), then, 30 s
// later, a second one. The grid is logged after each search and every 10 s
// between: the player's invSlots row by row, each item's place and size and
// its belt slot, and whether invSlots holds exactly the cells the items in
// the grid cover. A timeline (DXCAP_TIMELINE) drags items in the inventory
// screen between the searches; the grid must still match after.
//
// With Hover, between the searches, the console's own steps a second apart,
// the same in both engines: the inventory screen opened; the pointer put on
// the middle of the first grid item's button (the screen's winItems child
// whose client object it is, as the screen finds one), a marked shot; the
// pointer on the screen's corner, clear of the buttons, a marked shot; the
// screen closed. The pointer the root draws over an item is the one its
// CursorRequested gives.
//=============================================================================
class LootConsole extends Console;

var config bool Hover;
var int HoverStep;
var int ShotsTaken;
var Window Pointed;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;
var int Searches;
var ScriptedPawn Victim;
var vector DeathSpot;
var bool bTravelled;

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

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

// Hover's steps, a second apart, each shot's mark up for 0.6 s before it.
function HoverTick(DeusExPlayer DXP)
{
	local DeusExRootWindow root;
	local PersonaScreenInventory screen;
	local Inventory item;
	local Window w;

	if (bMarking)
	{
		if (StepTime > 0.6)
		{
			Log("DXLOOT: hover: shot " $ ShotsTaken);
			DXP.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			StepTime = 0;
		}
		return;
	}
	if (StepTime < 1.0)
		return;
	StepTime = 0;
	root = DeusExRootWindow(DXP.rootWindow);
	if (root != None)
		screen = PersonaScreenInventory(root.GetTopWindow());
	if (HoverStep == 0)
	{
		DXP.ShowInventoryWindow();
		Log("DXLOOT: hover: the inventory screen opened");
	}
	else if (HoverStep == 1)
	{
		for (item = DXP.Inventory; item != None; item = item.Inventory)
			if (item.invPosX >= 0 && item.invPosY >= 0)
				break;
		if (screen != None && screen.winItems != None && item != None)
			for (w = screen.winItems.GetTopChild(); w != None && Pointed == None; w = w.GetLowerSibling())
				if (PersonaInventoryItemButton(w) != None && w.GetClientObject() == item)
					Pointed = w;
		if (Pointed == None)
		{
			Log("DXLOOT: hover: no item's button to point at, exiting");
			DXP.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Pointed.SetCursorPos(Pointed.width / 2, Pointed.height / 2);
		Log("DXLOOT: hover: the pointer on " $ Pointed.Class.Name $ ", " $ item.Name $ "'s");
		MarkShot = ShotsTaken;
		bMarking = true;
	}
	else if (HoverStep == 2 && screen != None)
	{
		screen.SetCursorPos(2, 2);
		Log("DXLOOT: hover: the pointer on the screen's corner");
		MarkShot = ShotsTaken;
		bMarking = true;
	}
	else
	{
		if (root != None)
			root.ClearWindowStack();
		Log("DXLOOT: hover: the screen closed");
		Phase = 4;
	}
	HoverStep++;
}

// The most isolated scripted pawn at least 400 units from the player, as
// DeathConsole picks its victims: a human, so it leaves a carcass.
function ScriptedPawn FindVictim(PlayerPawn P)
{
	local ScriptedPawn A, B, Best;
	local float Near, BestNear;

	foreach P.AllActors(class'ScriptedPawn', A)
	{
		if (A.bImportant || A.Health <= 0 || A.bHidden || A.IsA('Robot') || A.IsA('Animal'))
			continue;
		Near = 100000;
		foreach P.AllActors(class'ScriptedPawn', B)
		{
			if (B == A || B.Health <= 0)
				continue;
			if (VSize(B.Location - A.Location) < Near)
				Near = VSize(B.Location - A.Location);
		}
		if (VSize(A.Location - P.Location) < 400)
			continue;
		if (Near > BestNear)
		{
			BestNear = Near;
			Best = A;
		}
	}
	return Best;
}

// The grid as the items say it is, against invSlots.
function LogGrid(DeusExPlayer P, string Label)
{
	local Inventory item;
	local int Want[30];
	local int x, y, i;
	local string Row;
	local bool bMatch;

	Log("DXLOOT: grid " $ Label $ " at " $ MapTime);
	for (item = P.Inventory; item != None; item = item.Inventory)
	{
		Log("DXLOOT:   " $ item.Name $ " pos " $ item.invPosX $ "," $ item.invPosY
		    $ " size " $ item.invSlotsX $ "x" $ item.invSlotsY
		    $ " belt " $ item.bInObjectBelt $ " " $ item.beltPos);
		if (item.invPosX < 0 || item.invPosY < 0)
			continue;
		for (y = item.invPosY; y < item.invPosY + item.invSlotsY; y++)
			for (x = item.invPosX; x < item.invPosX + item.invSlotsX; x++)
				if (x < P.maxInvCols && y < P.maxInvRows)
					Want[y * P.maxInvCols + x]++;
	}
	bMatch = true;
	for (y = 0; y < P.maxInvRows; y++)
	{
		Row = "";
		for (x = 0; x < P.maxInvCols; x++)
		{
			i = y * P.maxInvCols + x;
			Row = Row $ P.invSlots[i] $ Want[i] $ " ";
			if (P.invSlots[i] != Min(Want[i], 1) || Want[i] > 1)
				bMatch = false;
		}
		Log("DXLOOT:   row " $ y $ ": " $ Row);
	}
	Log("DXLOOT: grid " $ Label $ " matches the items: " $ bMatch);
}

// The victim killed outright; the player stood beside the spot.
function Kill(PlayerPawn P)
{
	local vector Dir;

	Dir = vector(Victim.Rotation);
	DeathSpot = Victim.Location;
	Log("DXLOOT: killing " $ Victim.Name $ " (" $ Victim.Class.Name $ ") at " $ DeathSpot);
	P.SetLocation(DeathSpot + Dir * 100);
	P.SetRotation(rotator(-Dir));
	P.ViewRotation = P.Rotation;
	Victim.TakeDamage(1000, P, DeathSpot + Dir * 10, -Dir * 1000, 'Shot');
}

// The carcass nearest the death spot, searched as the player's frob does.
function Search(DeusExPlayer P)
{
	local DeusExCarcass C, Best;

	foreach P.AllActors(class'DeusExCarcass', C)
		if (VSize(C.Location - DeathSpot) < 300 && (Best == None || VSize(C.Location - DeathSpot) < VSize(Best.Location - DeathSpot)))
			Best = C;
	if (Best == None)
	{
		Log("DXLOOT: no carcass near " $ DeathSpot);
		return;
	}
	Log("DXLOOT: searching " $ Best.Name);
	Best.Frob(P, None);
	Searches++;
	LogGrid(P, "after search " $ Searches);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	// Only the island needs Deus Ex's player; the menu map's travels.
	if (DXP == None && M ~= "01_NYC_UNATCOIsland")
		return;
	P.ReducedDamageType = 'All';

	if (Phase >= 2 && Phase < 9)
	{
		LogTime += Delta;
		if (LogTime >= 10.0)
		{
			LogTime = 0;
			LogGrid(DXP, "now");
		}
	}

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0 && !bTravelled)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				bTravelled = true;
			}
		}
		else if (MapTime > 6.0)
		{
			LogGrid(DXP, "at the start");
			Phase = 1;
		}
	}
	else if (Phase == 1 || Phase == 4)
	{
		if (Phase == 1 || MapTime > 40.0)
		{
			Victim = FindVictim(P);
			if (Victim == None)
			{
				Log("DXLOOT: no victim, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Kill(P);
			StepTime = 0;
			Phase++;
		}
	}
	else if (Phase == 2 || Phase == 5)
	{
		if (StepTime > 4.0)
		{
			Search(DXP);
			StepTime = 0;
			Phase++;
		}
	}
	else if (Phase == 3)
	{
		// The timeline's drags happen here, or Hover's steps.
		if (Hover)
			HoverTick(DXP);
		else
			Phase = 4;
	}
	else if (Phase == 6)
	{
		if (StepTime > 20.0)
		{
			LogGrid(DXP, "at the end");
			Log("DXLOOT: " $ Searches $ " searches, done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
