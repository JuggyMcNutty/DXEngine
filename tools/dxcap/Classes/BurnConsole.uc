//=============================================================================
// BurnConsole: a flammable decoration burning out -- Liberty Island opened
// from wherever the run starts; 2 s into the level's time a small crate
// (breakable, medium) spawned 160 units ahead of the player, flammable, 5 hit points
// and set alight (TakeDamage 'Flamed': the game's Burning state spawns eight
// Fires based on it and its timer takes a point a second), then each second
// the crates and the Fires in the level and how many stand on the crate,
// until the crate is gone or 25 s pass; an exit. The original destroys the
// crate some 6 s in with its Fires still based on it, each Fire's BaseChange
// basing it on the dying crate again, which DXEngine's Destroy once looped
// on until the runaway guard stopped the game (bugs.md E6.1). Each line
// starts "DXBURN:".
//=============================================================================
class BurnConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var float NextLog;
var float LitAt;

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

function SetAlight(PlayerPawn P)
{
	local DeusExDecoration Crate;
	local vector Spot;

	// A breakable crate: an unbreakable one is invincible and refuses the
	// flame before its state can change.
	Spot = P.Location + Vector(P.Rotation) * 160 + vect(0,0,20);
	Crate = P.Spawn(class'CrateBreakableMedGeneral',, 'BurnCrate', Spot, P.Rotation);
	if (Crate == None)
	{
		Log("DXBURN: no room for the crate at " $ Spot);
		return;
	}
	Crate.bFlammable = True;
	Crate.bInvincible = False;
	Crate.HitPoints = 5;
	Crate.TakeDamage(1, P, Crate.Location, vect(0,0,0), 'Flamed');
	LitAt = P.Level.TimeSeconds;
	Log("DXBURN: lit " $ Crate $ " at " $ Crate.Location $ " state " $ Crate.GetStateName()
		$ " hp " $ Crate.HitPoints $ " flammability " $ Crate.Flammability);
}

// The crates and the Fires still in the level (AllActors passes over the
// destroyed), and the Fires based on a crate.
function Census(PlayerPawn P)
{
	local DeusExDecoration C;
	local Fire F;
	local int Crates, Fires, Based;

	foreach P.AllActors(class'DeusExDecoration', C, 'BurnCrate')
		Crates++;
	foreach P.AllActors(class'Fire', F)
	{
		Fires++;
		if (F.Base != None && F.Base.Tag == 'BurnCrate')
			Based++;
	}
	Log("DXBURN: t=" $ (P.Level.TimeSeconds - LitAt) $ " crates " $ Crates $ " fires " $ Fires $ " based on a crate " $ Based);
	if (Crates == 0 && Step == 1)
	{
		Log("DXBURN: the crate is gone, the game goes on");
		Step = 2;
		NextLog = P.Level.TimeSeconds + 1.0;
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXBURN: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		SetAlight(P);
		Step = 1;
		NextLog = P.Level.TimeSeconds + 1.0;
	}
	else if (Step == 1 && P.Level.TimeSeconds >= NextLog)
	{
		Census(P);
		NextLog += 1.0;
		if (Step == 1 && P.Level.TimeSeconds - LitAt > 25.0)
		{
			Log("DXBURN: the crate is still there after 25 s");
			Step = 2;
			NextLog = P.Level.TimeSeconds + 1.0;
		}
	}
	else if (Step == 2 && P.Level.TimeSeconds >= NextLog)
	{
		Log("DXBURN: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
