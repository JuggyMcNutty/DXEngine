//=============================================================================
// ParabolicConsole: Actor.ParabolicTrace as the NPCs ask it -- Liberty
// Island opened from wherever the run starts; 2 s into the level's time,
// from the player's eyes, throws traced at 400, 700 and 1000 units a second
// along the player's facing and 30 degrees up, with the game's gravity, a
// 2-unit box and 5 s at most, bouncing and not (the elasticity 0.5): each
// throw's landing and time logged, then the player turned a quarter turn
// four times and the throws traced again; an exit. The original's trace
// bounces on and lands where it comes to rest; DXEngine's once restarted a
// bounced flight at the world's origin when the slide after the bounce met
// nothing (bugs.md E4a.10). Each line starts "DXPARA:".
//=============================================================================
class ParabolicConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var int Turns;

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

function Throws(PlayerPawn P)
{
	local vector Start, Dir, Landing;
	local rotator R;
	local float Speed, T;
	local int i, b;

	Start = P.Location + vect(0,0,1) * P.BaseEyeHeight;
	R = P.Rotation;
	R.Pitch = 5461; // 30 degrees up
	Dir = Vector(R);
	for (i = 0; i < 3; i++)
	{
		Speed = 400 + 300 * i;
		for (b = 0; b < 2; b++)
		{
			T = P.ParabolicTrace(Landing, Dir * Speed, Start, true, vect(2,2,2), 5.0, 0.5, b == 1, 60, 0.01);
			Log("DXPARA: yaw " $ P.Rotation.Yaw $ " speed " $ Speed $ " bounce " $ (b == 1) $ " lands at " $ Landing $ " after " $ T);
		}
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;

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
			Log("DXPARA: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Throws(P);
		Turns = 0;
		Step = 1;
	}
	else if (Step == 1)
	{
		if (Turns < 4)
		{
			R = P.Rotation;
			R.Yaw += 16384;
			P.SetRotation(R);
			P.ViewRotation = R;
			Throws(P);
			Turns++;
		}
		else
		{
			Log("DXPARA: exiting");
			P.ConsoleCommand("exit");
			Step = 2;
		}
	}
}
