//=============================================================================
// NavReachConsole: the script's ActorReachable of a
// navigation point asked by a pawn standing on one. The original's
// execactorReachable (Engine.dll 0x103bc1a0) answers it from the path
// network when the pawn stands at a node (its MoveTarget first, then the
// level's list: within max(radius, 48) across and its height up or down):
// true for that node itself or one CanMoveTo reaches from it (a spec of the
// node's Paths or PrunedPaths, open to the pawn), else false unless flying;
// only a pawn at no node is walked (actorReachable). DXEngine's does the same.
//
// On Liberty Island, 1 s in, the first 12 ScriptedPawns by name: each put on
// its nearest NavigationPoint, standing still, then ActorReachable of the 8
// nearest other navigation points within 800 units, each logged with
// whether the pawn's node has a spec to it (describeSpec over its Paths; a
// pruned spec, which counts too, is not shown, so a node reached by one
// logs as unlinked). The two engines' 87 answers are alike (bugs.md AI.9).
//=============================================================================
class NavReachConsole extends Console;

var float MapTime;
var int Phase;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function bool Linked(NavigationPoint From, NavigationPoint To)
{
	local int i, Flags, Dist;
	local Actor S, E;
	for (i = 0; i < 16; i++)
	{
		if (From.Paths[i] < 0)
			break;
		From.describeSpec(From.Paths[i], S, E, Flags, Dist);
		if (E == To)
			return true;
	}
	return false;
}

function Probe(PlayerPawn P, ScriptedPawn S)
{
	local NavigationPoint N, Home, Near[8];
	local float D, Best, Dist[8];
	local int k, j;

	Best = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		D = VSize(N.Location - S.Location);
		if (D < Best)
		{
			Best = D;
			Home = N;
		}
	}
	if (Home == None)
		return;
	S.SetLocation(Home.Location);
	S.Velocity = vect(0,0,0);
	S.Acceleration = vect(0,0,0);
	Log("DXNAVREACH: " $ S.Name $ " on " $ Home.Name $ " at " $ V(S.Location) $ " movetarget " $ S.MoveTarget);

	for (k = 0; k < 8; k++)
		Dist[k] = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		if (N == Home)
			continue;
		D = VSize(N.Location - Home.Location);
		if (D > 800)
			continue;
		for (k = 0; k < 8; k++)
		{
			if (D < Dist[k])
			{
				for (j = 7; j > k; j--)
				{
					Dist[j] = Dist[j - 1];
					Near[j] = Near[j - 1];
				}
				Dist[k] = D;
				Near[k] = N;
				break;
			}
		}
	}
	for (k = 0; k < 8; k++)
		if (Near[k] != None)
			Log("DXNAVREACH: " $ S.Name $ " node " $ Near[k].Name $ " dist " $ int(Dist[k])
				$ " linked " $ Linked(Home, Near[k]) $ " reachable " $ S.ActorReachable(Near[k]));
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
