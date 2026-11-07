//=============================================================================
// JumpLandConsole: where a jumper's reach test brings its jump down. A walk
// that finds no floor jumps (FindBestJump, Engine.dll 0x103c2f50): the
// jump rises at the pawn's JumpZ (0x103c2f87), its fall is stepped by
// jumpLanding (0x103c2530) with three MoveActor test moves a step, held off
// 2 units and passing pawns and decorations that are not static, and a
// landing 350 or more below the start fails with the original's own line,
// "FAILING FBJ: nc ... from odS ... and dS ... and dz ... success 0": the
// distance gained, the distances before and after and the drop, which say
// where the jump came down (bugs.md PHYS.21).
//
// On Liberty Island, 1 s in, the player taken out of the world: a column
// near where the player started -- the first, by ring and yaw, whose floor
// is flat and dry from 0 to 256 units along X and with nothing of the level
// for 600 units over it, and no actor that blocks within 400 units. Then
// each trial a CapReacher that can jump, walking, set Drop units over a
// standing pawn's place at the column, asks PointReachable of a spot in the
// air Across units along X and 120 units under it: its walk steps out,
// finds no floor and jumps toward the spot, and the jump comes down on the
// floor under it unless something stands in its way there -- set on the
// floor first, with no physics, clear of the spot so as not to move it:
//   1. bare: 420 up, 64 along, nothing there;
//   2. large crate: a CrateUnbreakableLarge under the spot, a landing on its
//      top under 350 below the jump;
//   3. small crate: a CrateUnbreakableSmall there;
//   4. pawn: a CapReacher standing there, its head under 350 below;
//   5. across: 420 up, 200 along, the jump going across (its first step of
//      128 a walk, the rest the jump);
//   6. low: 200 up, 64 along, a landing under 350 below: no line.
// Each answer logged "DXJUMP:", the engine's FAILING FBJ lines among them;
// then an exit. What is compared between the engines: every DXJUMP line and
// every FAILING FBJ line. The original's falls pass through the crates and
// the pawn to the floor, held 2 units over it, so 1 to 5 each log a FAILING
// FBJ line, 2 to 4 the same as 1, and 6 none; each answers False.
//=============================================================================
class JumpLandConsole extends Console;

var float MapTime;
var int Phase;
var vector Start, Floor;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function Say(string S)
{
	Log("DXJUMP: " $ S);
}

// The level's floor under X, from the start's height down
function bool FloorAt(PlayerPawn P, vector X, out vector Spot)
{
	local vector From, To, HitLoc, HitNorm;
	local Actor Hit;

	From = X;
	From.Z = Start.Z + 64;
	To = X;
	To.Z = Start.Z - 1200;
	Hit = P.Trace(HitLoc, HitNorm, To, From, false);
	if (Hit != P.Level || HitNorm.Z < 0.9)
		return false;
	Spot = HitLoc;
	return true;
}

function bool Column(PlayerPawn P, vector C)
{
	local vector F, G, Low, High;
	local int i, j;
	local Actor A;
	local CapReacher Probe;
	local bool bWet;

	if (!FloorAt(P, C, F))
		return false;
	if (!FloorAt(P, F + vect(64, 0, 0), G) || Abs(G.Z - F.Z) > 2)
		return false;
	if (!FloorAt(P, F + vect(200, 0, 0), G) || Abs(G.Z - F.Z) > 2)
		return false;
	if (!FloorAt(P, F + vect(256, 0, 0), G) || Abs(G.Z - F.Z) > 2)
		return false;
	for (i = -1; i <= 8; i++)
	{
		for (j = -1; j <= 1; j++)
		{
			Low = F + vect(32, 0, 0) * i + vect(0, 32, 0) * j + vect(0, 0, 8);
			High = Low + vect(0, 0, 600);
			if (!P.FastTrace(High, Low))
				return false;
		}
	}
	foreach P.RadiusActors(class'Actor', A, 400, F + vect(100, 0, 300))
	{
		if (A != P && A.bCollideActors && (A.bBlockActors || A.bBlockPlayers))
			return false;
	}
	Probe = P.Spawn(class'CapReacher',,, F + vect(64, 0, 44));
	if (Probe == None)
		return false;
	bWet = Probe.Region.Zone.bWaterZone || Probe.FootRegion.Zone.bWaterZone;
	Probe.Destroy();
	if (bWet)
		return false;
	Floor = F;
	return true;
}

function bool FindColumn(PlayerPawn P)
{
	local int r, k;
	local rotator Dir;
	local vector C;

	for (r = 0; r < 4; r++)
	{
		for (k = 0; k < 8; k++)
		{
			Dir.Yaw = k * 8192;
			C = Start + vector(Dir) * (256 * r);
			if (Column(P, C))
			{
				Say("column at ring " $ r $ " yaw " $ Dir.Yaw $ ": floor " $ Floor);
				return true;
			}
			if (r == 0)
				break;
		}
	}
	return false;
}

function Trial(PlayerPawn P, string Label, float Drop, float Across, class<Actor> UnderClass)
{
	local CapReacher J;
	local Actor Under;
	local vector Dest;
	local bool B;

	if (UnderClass != None)
	{
		Under = P.Spawn(UnderClass,,, Floor + vect(64, 0, 0) + vect(0, 0, 1) * (UnderClass.default.CollisionHeight + 1));
		if (Under == None)
		{
			Say(Label $ ": " $ UnderClass $ " did not spawn");
			return;
		}
		Under.SetPhysics(PHYS_None);
		Say(Label $ ": " $ Under.Class $ " at " $ Under.Location $ " radius " $ Under.CollisionRadius $ " height " $ Under.CollisionHeight
			$ " static " $ Under.bStatic $ " blocks " $ Under.bBlockActors);
	}
	J = P.Spawn(class'CapReacher',,, Floor + vect(0, 0, 44) + vect(0, 0, 1) * Drop);
	if (J == None)
	{
		Say(Label $ ": the jumper did not spawn");
		if (Under != None)
			Under.Destroy();
		return;
	}
	J.bCanJump = True;
	J.SetPhysics(PHYS_Walking);
	Dest = Floor + vect(0, 0, 44) + vect(1, 0, 0) * Across + vect(0, 0, 1) * (Drop - 120);
	Say(Label $ ": jumper at " $ J.Location $ " JumpZ " $ J.JumpZ $ ", to " $ Dest);
	B = J.PointReachable(Dest);
	Say(Label $ ": reachable " $ B $ ", the jumper back at " $ J.Location);
	J.Destroy();
	if (Under != None)
		Under.Destroy();
}

function Probes(PlayerPawn P)
{
	Trial(P, "1 bare", 420, 64, None);
	Trial(P, "2 large crate", 420, 64, class'CrateUnbreakableLarge');
	Trial(P, "3 small crate", 420, 64, class'CrateUnbreakableSmall');
	Trial(P, "4 pawn", 420, 64, class'CapReacher');
	Trial(P, "5 across", 420, 200, None);
	Trial(P, "6 low", 200, 64, None);
}

event Tick(float Delta)
{
	local PlayerPawn P;

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
		Start = P.Location;
		P.ReducedDamageType = 'All';
		P.SetCollision(false, false, false);
		P.bCollideWorld = false;
		P.SetPhysics(PHYS_None);
		Say("the player started at " $ Start);
		if (FindColumn(P))
			Probes(P);
		else
			Say("no column found");
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
