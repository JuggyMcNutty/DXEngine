//=============================================================================
// PickDestConsole: AIPickRandomDestination with a
// multiplier under 1, its direction made fixed (both distributions 1, so
// RandomBiasedRotation gives the centre). The original's second try
// (Engine.dll 0x103c80f2..0x103c8168) asks AIDirectionReachable for the
// reached fraction as both its least and its most distance, so it finds the
// first step past it, as DXEngine's does.
//
// On Liberty Island, 1 s in, the first 12 ScriptedPawns by name, each put
// back at its home spot: for yaws 0, 16384, 32768 and 49152,
// AIDirectionReachable(Location, yaw, 0, 200, 800) (the first try's range at
// a multiplier of 0.5) and AIPickRandomDestination(100, 400, yaw, 1.0, 0,
// 1.0, 1, 0.5); each answer, its spot and its distance logged "DXPICK:".
// Each engine's pick the first step (the pawn's radius, 5..25) at or past
// half the first distance, all 48 alike (bugs.md AI.11).
//=============================================================================
class PickDestConsole extends Console;

var float MapTime;
var int Phase;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function Probe(ScriptedPawn S)
{
	local int i, yaw;
	local vector Best, Dest;
	local bool bFound, bPicked;

	S.SetLocation(S.HomeLoc);
	S.Velocity = vect(0,0,0);
	S.Acceleration = vect(0,0,0);
	for (i = 0; i < 4; i++)
	{
		yaw = i * 16384;
		bFound = S.AIDirectionReachable(S.Location, yaw, 0, 200, 800, Best);
		bPicked = S.AIPickRandomDestination(100, 400, yaw, 1.0, 0, 1.0, 1, 0.5, Dest);
		Log("DXPICK: " $ S.Name $ " yaw " $ yaw $ " radius " $ S.CollisionRadius
			$ " first " $ bFound $ " " $ V(Best) $ " dist " $ VSize(Best - S.Location)
			$ " picked " $ bPicked $ " " $ V(Dest) $ " dist " $ VSize(Dest - S.Location));
	}
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
			Probe(Picked[j]);
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
