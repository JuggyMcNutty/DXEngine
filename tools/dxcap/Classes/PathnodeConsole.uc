//=============================================================================
// PathnodeConsole: ReachablePathnodes, the list Deus Ex's projectile
// avoidance walks -- the original's GetPathnodeList (Engine.dll 0x103c6490),
// sorted by the C runtime's qsort -- and AIPickRandomDestination's pitch
// (0x103c8041) (bugs.md AI.13, AI.14).
//
// On Liberty Island, 1 s in, the first 12 ScriptedPawns by name, each put
// back at its home spot, standing still, its MoveTarget and nearest
// navigation point logged (a node the pawn overlaps is the start node when
// FromPoint is none):
//   1. from its nearest navigation point, with the pruned paths: that
//      node's paths up to the first empty one, sorted, equal distances in
//      the order the C runtime's qsort leaves them;
//   2. from itself (no FromPoint): with no start node, 32 of the nodes
//      within 1,000 units taken in the level's order, each one after them
//      in place of the nearest held one farther than it, then those it can
//      reach;
//   3. from the player, the same round the player;
// the count of nodes within 1,000 units of where each list is taken from,
// each list's entries in its order, node and distance, and the iterator's
// node once it is done (None in the original). Then a CapReacher that can
// swim, set at the first water zone's ZoneInfo by name, walking and then
// swimming, picks a destination along yaw 0 with a pitch of 8192, both
// distributions 1 so that the direction is the centre's: the original
// drops the pitch for a walker whatever its zone. Each line logged
// "DXNODES:"; then an exit. What is compared between the engines: every
// line.
//=============================================================================
class PathnodeConsole extends Console;

var float MapTime;
var int Phase;

function int Within(PlayerPawn P, vector X)
{
	local NavigationPoint N;
	local int Count;

	foreach P.AllActors(class'NavigationPoint', N)
	{
		if (VSize(N.Location - X) < 1000)
			Count++;
	}
	return Count;
}

function ListNodes(PlayerPawn P, ScriptedPawn S, string Label, Actor From, vector Round, bool bPruned)
{
	local NavigationPoint N;
	local float Dist;
	local int i;

	Log("DXNODES: " $ S.Name $ " " $ Label $ ": " $ Within(P, Round) $ " nodes within 1000");
	foreach S.ReachablePathnodes(class'NavigationPoint', N, From, Dist, bPruned)
	{
		Log("DXNODES: " $ S.Name $ " " $ Label $ " " $ i $ ": " $ N.Name $ " " $ Dist);
		i++;
	}
	Log("DXNODES: " $ S.Name $ " " $ Label $ ": " $ i $ " given, the node after " $ N);
}

function Probe(PlayerPawn P, ScriptedPawn S)
{
	local NavigationPoint N, Nearest;
	local float D, Best;

	S.SetLocation(S.HomeLoc);
	S.Velocity = vect(0,0,0);
	S.Acceleration = vect(0,0,0);
	Best = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		D = VSize(N.Location - S.Location);
		if (D < Best || (D == Best && Nearest != None && Caps(string(N.Name)) < Caps(string(Nearest.Name))))
		{
			Best = D;
			Nearest = N;
		}
	}
	Log("DXNODES: " $ S.Name $ " at " $ S.Location $ " MoveTarget " $ S.MoveTarget $ " nearest node " $ Nearest $ " " $ Best);
	if (Nearest != None)
		ListNodes(P, S, "from its node", Nearest, Nearest.Location, true);
	ListNodes(P, S, "from itself", None, S.Location, false);
	ListNodes(P, S, "from the player", P, P.Location, false);
}

function PitchProbe(PlayerPawn P)
{
	local ZoneInfo Z, Water;
	local CapReacher C;
	local vector Dest;
	local bool B;

	foreach P.AllActors(class'ZoneInfo', Z)
	{
		if (Z.bWaterZone && (Water == None || Caps(string(Z.Name)) < Caps(string(Water.Name))))
			Water = Z;
	}
	if (Water == None)
	{
		Log("DXNODES: pitch: no water zone");
		return;
	}
	C = P.Spawn(class'CapReacher',,, Water.Location);
	if (C == None)
	{
		Log("DXNODES: pitch: no pawn at " $ Water.Name);
		return;
	}
	C.bCanSwim = True;
	C.SetPhysics(PHYS_Walking);
	Log("DXNODES: pitch: at " $ Water.Name $ ", " $ C.Location $ ", zone " $ C.Region.Zone $ " water " $ C.Region.Zone.bWaterZone);
	B = C.AIPickRandomDestination(100, 400, 0, 1.0, 8192, 1.0, 1, 1.0, Dest);
	Log("DXNODES: pitch: walking, picked " $ B $ " at " $ Dest $ ", up " $ (Dest.Z - C.Location.Z));
	C.SetPhysics(PHYS_Swimming);
	B = C.AIPickRandomDestination(100, 400, 0, 1.0, 8192, 1.0, 1, 1.0, Dest);
	Log("DXNODES: pitch: swimming, picked " $ B $ " at " $ Dest $ ", up " $ (Dest.Z - C.Location.Z));
	C.Destroy();
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
		// The first 12 by name, sorted here so both engines ask the same pawns.
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
		PitchProbe(P);
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
