//=============================================================================
// SecondWayConsole: FindPathToward of an actor no
// navigation point near it can reach, the search's "second way" (the
// original's findPathToward, Engine.dll 0x103db3f0, from 0x103db901: the
// lists gathered again, then definePathsFor and findEndPoint on the GOAL's
// list -- lea ecx,[ebp+var_144] at 0x103db938 and 0x103db94d -- and the
// search from the pawn list's first node), as DXEngine's does.
//
// On Liberty Island, 1 s in, the first 12 ScriptedPawns by name, each put
// back at its home spot (HomeLoc) and standing still: a CapMarker 300 units
// over the navigation point nearest 500 units from the pawn (none of its
// nodes walks within 15 units of it), then FindPathToward(marker), twice
// (once as found, once after a SetLocation back home); the answer, its place
// and the pawn's place after logged with "DXSECOND:".
// The two engines' routes are alike, all 24 (bugs.md AI.10).
//=============================================================================
class SecondWayConsole extends Console;

var float MapTime;
var int Phase;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function Probe(PlayerPawn P, ScriptedPawn S)
{
	local NavigationPoint N, Pick;
	local float D, Best;
	local CapMarker M;
	local Actor R;
	local int Try;
	local string Where;

	S.SetLocation(S.HomeLoc);
	S.Velocity = vect(0,0,0);
	S.Acceleration = vect(0,0,0);

	Best = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		D = Abs(VSize(N.Location - S.Location) - 500);
		if (D < Best)
		{
			Best = D;
			Pick = N;
		}
	}
	if (Pick == None)
		return;
	M = P.Spawn(class'CapMarker',,, Pick.Location + vect(0,0,300));
	if (M == None)
		return;
	for (Try = 0; Try < 2; Try++)
	{
		S.SetLocation(S.HomeLoc);
		R = S.FindPathToward(M);
		Where = "-";
		if (R != None)
			Where = V(R.Location);
		Log("DXSECOND: " $ S.Name $ " at " $ V(S.Location) $ " goal over " $ Pick.Name
			$ " route " $ R $ " " $ Where $ " movetarget " $ S.MoveTarget);
	}
	M.Destroy();
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local ScriptedPawn S, Picked[12], T;
	local int Count, i, j;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1 && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 1.0)
	{
		foreach P.AllActors(class'ScriptedPawn', S)
		{
			if (Count < 12)
				Picked[Count++] = S;
			else if (Caps(string(S.Name)) < Caps(string(Picked[11].Name)))
				Picked[11] = S;
			for (i = Count - 1; i > 0; i--)
			{
				if (Caps(string(Picked[i].Name)) < Caps(string(Picked[i - 1].Name)))
				{
					T = Picked[i];
					Picked[i] = Picked[i - 1];
					Picked[i - 1] = T;
				}
			}
		}
		for (j = 0; j < Count; j++)
			Probe(P, Picked[j]);
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
