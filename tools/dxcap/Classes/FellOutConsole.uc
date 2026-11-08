//=============================================================================
// FellOutConsole (bugs.md PHYS.35): what leaves the world, logged with
// "DXFELL:" -- Liberty Island opened from wherever the run starts; 2 s in,
// six actors spawned over the player, each then taken 30,000 units down,
// into the solid under the level, where it is in no zone (SetLocation with
// bCollideWorld and bCollideWhenPlacing off, so that no room is looked for;
// bCollideWorld put back after), and given its physics there: a CapReacher walking, one
// falling and one flying (bCanFly); a stock decoration (DeusEx.Basketball)
// and a stock pickup (DeusEx.Credits), each falling; a CapMarker falling.
// Each one's name, zone and physics logged as it is taken out; then, at the
// three ticks after, whether each is still there and its physics.
//
// The original (Engine.dll): physWalking and physFalling in no zone send
// FellOutOfWorld, the server logging "%s fell out of the world!" first, the
// actor's name (physWalking's 0x103ca5ae; physFalling's for a pawn, a
// decoration or an inventory item alone, 0x103d0b60); physFlying logs "%s
// flew out of the world!" for a flyer that collides with the world, on a
// client too, and destroys it unless it is a player (0x103d309d). Expected in
// each engine's own log, by the names this console logs: the walker's, the
// faller's, the basketball's and the credits' "fell out" lines, the flyer's
// "flew out" line, none for the marker.
//=============================================================================
class FellOutConsole extends Console;

var float RunTime, MapTime;
var int Phase, After;
var string CurrentMap;
var Actor Out[6];
var vector Below;

function Say(string S)
{
	Log("DXFELL: " $ S);
}

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

// Into the solid under the level, in no zone
function Take(int i, Actor A, bool bWorld)
{
	local vector Far;

	Out[i] = A;
	if (A == None)
	{
		Say(i $ ": none spawned");
		return;
	}
	A.bCollideWorld = false;
	A.bCollideWhenPlacing = false;
	Far = Below;
	Far.X += 300 * i;
	if (!A.SetLocation(Far))
		Say(i $ ": " $ A.Name $ " would not go out to " $ Far);
	A.bCollideWorld = bWorld;
}

function Report(int i)
{
	local Actor A;

	A = Out[i];
	if (A != None)
		Say(i $ ": " $ A.Name $ " (" $ A.Class $ ") at " $ A.Location $ " zone " $ A.Region.ZoneNumber $ " physics " $ GetEnum(enum'EPhysics', A.Physics));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapReacher R;
	local vector Over;
	local int i;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	if (Phase == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Say("opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = -1;
		}
		return;
	}
	if (Phase == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Phase = 0;
	if (Phase == 0 && P.Level.TimeSeconds > 2.0)
	{
		Over = P.Location + vect(0, 0, 200);
		Below = P.Location - vect(0, 0, 30000);
		Take(0, P.Spawn(class'CapReacher',,, Over), true);
		Take(1, P.Spawn(class'CapReacher',,, Over + vect(60, 0, 0)), true);
		R = P.Spawn(class'CapReacher',,, Over + vect(120, 0, 0));
		if (R != None)
			R.bCanFly = true;
		Take(2, R, true);
		Take(3, P.Spawn(class'DeusEx.Basketball',,, Over + vect(-60, 0, 0)), true);
		Take(4, P.Spawn(class'DeusEx.Credits',,, Over + vect(-120, 0, 0)), true);
		Take(5, P.Spawn(class'CapMarker',,, Over + vect(0, 60, 0)), false);
		if (Out[0] != None)
			Out[0].SetPhysics(PHYS_Walking);
		if (Out[1] != None)
			Out[1].SetPhysics(PHYS_Falling);
		if (Out[2] != None)
			Out[2].SetPhysics(PHYS_Flying);
		for (i = 3; i < 6; i++)
		{
			if (Out[i] != None)
				Out[i].SetPhysics(PHYS_Falling);
		}
		for (i = 0; i < 6; i++)
			Report(i);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		After++;
		for (i = 0; i < 6; i++)
		{
			if (Out[i] == None || Out[i].bDeleteMe)
				Say("tick " $ After $ " " $ i $ ": gone");
			else
				Say("tick " $ After $ " " $ i $ ": " $ Out[i].Name $ " zone " $ Out[i].Region.ZoneNumber $ " physics " $ GetEnum(enum'EPhysics', Out[i].Physics));
		}
		if (After >= 3)
		{
			Say("done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
