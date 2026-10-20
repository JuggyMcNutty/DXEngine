//=============================================================================
// RollConsole: rolling (PHYS_Rolling) -- Liberty Island opened from wherever
// the run starts; 2 s into the level's time, four small crates spawned 120
// units from the player, ahead, behind and to each side; 1.5 s later, landed,
// each set rolling away from it at 400 units/s; every 0.1 s for 4 s, each
// crate's place, velocity, physics and base, logged with "DXROLL:"; then an
// exit.
//=============================================================================
class RollConsole extends Console;

var float MapTime, NextLog, StartTime;
var int Step;
var string CurrentMap;
var Decoration Crates[4];

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

function string BaseOf(Actor A)
{
	if (A.Base == None)
		return "None";
	if (A.Base == A.Level)
		return "Level";
	return string(A.Base.Class.Name);
}

function Spawn4(PlayerPawn P)
{
	local int i;
	local rotator R;
	local vector Dir;

	for (i = 0; i < 4; i++)
	{
		R = P.Rotation;
		R.Pitch = 0;
		R.Roll = 0;
		R.Yaw += i * 16384;
		Dir = vector(R);
		Crates[i] = P.Spawn(class'DeusEx.CrateUnbreakableSmall',,, P.Location + Dir * 120);
		if (Crates[i] == None)
		{
			Log("DXROLL: crate " $ i $ " no room");
			continue;
		}
	}
}

function Roll4(PlayerPawn P)
{
	local int i;
	local rotator R;
	local vector Dir;

	for (i = 0; i < 4; i++)
	{
		if (Crates[i] == None)
			continue;
		R = P.Rotation;
		R.Pitch = 0;
		R.Roll = 0;
		R.Yaw += i * 16384;
		Dir = vector(R);
		Crates[i].SetPhysics(PHYS_Rolling);
		Crates[i].Velocity = Dir * 400;
		Crates[i].Acceleration = vect(0,0,0);
		Log("DXROLL: crate " $ i $ " at " $ Crates[i].Location $ " heading " $ Dir $ " base " $ BaseOf(Crates[i]));
	}
}

function LogCrates(float T)
{
	local int i;

	for (i = 0; i < 4; i++)
	{
		if (Crates[i] != None)
			Log("DXROLL: t" $ int(T * 10 + 0.5) $ " crate " $ i $ " at " $ Crates[i].Location $ " vel " $ Crates[i].Velocity
				$ " physics " $ int(Crates[i].Physics) $ " base " $ BaseOf(Crates[i]));
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
			Log("DXROLL: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Spawn4(P);
		Step = 1;
	}
	else if (Step == 1 && P.Level.TimeSeconds > 3.5)
	{
		Roll4(P);
		StartTime = P.Level.TimeSeconds;
		NextLog = 0;
		Step = 2;
	}
	else if (Step == 2)
	{
		if (P.Level.TimeSeconds - StartTime >= NextLog)
		{
			LogCrates(NextLog);
			NextLog += 0.1;
		}
		if (P.Level.TimeSeconds - StartTime > 4.0)
		{
			Log("DXROLL: exiting");
			P.ConsoleCommand("exit");
			Step = 3;
		}
	}
}
