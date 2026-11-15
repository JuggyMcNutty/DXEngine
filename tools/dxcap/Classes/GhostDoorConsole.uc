//=============================================================================
// GhostDoorConsole: an actor that collides with actors but not the world
// moved through a closed door (bugs.md PHYS.41). On TargetMap
// ([DXCapture.GhostDoorConsole], UNATCO HQ by default), 2 s in, the
// DeusExMover nearest the player that turns and does not slide, closed and
// not locked, its leaf's angle round the hinge found as DoorConsole finds
// it. A CapGhostToucher spawned 32 units out along the leaf and 40 to one
// side of it, then moved 80 units across the leaf (Move) and back: the
// original's move checks the actors with no level, meets the door's brush
// and, the door not blocking what does not collide with the world, touches
// it. Each touch and untouch, where each move ends, then an exit; each line
// logged with "DXGHOST:" in front.
//=============================================================================
class GhostDoorConsole extends Console;

var config string TargetMap;

var string CurrentMap;
var float MapTime;
var int Step;
var DeusExMover Door;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOHQ";
	return TargetMap;
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

function vector Out(float Degrees, float Radius)
{
	local vector V;

	V.X = Radius * Cos(Degrees * Pi / 180);
	V.Y = Radius * Sin(Degrees * Pi / 180);
	return V;
}

// The angle of the door's leaf round its hinge, or -1, as DoorConsole's
function float LeafAngle(PlayerPawn P)
{
	local int i, h;
	local float Heights[5];
	local vector Centre, HitLoc, HitNorm;
	local Actor Hit;

	Heights[0] = 0;
	Heights[1] = 32;
	Heights[2] = -32;
	Heights[3] = 64;
	Heights[4] = -64;
	for (h = 0; h < 5; h++)
	{
		Centre = Door.Location + vect(0,0,1) * Heights[h];
		for (i = 0; i < 72; i++)
		{
			Hit = P.Trace(HitLoc, HitNorm, Centre + Out(i * 5 + 3, 32), Centre + Out(i * 5 - 3, 32), true);
			if (Hit == Door)
				return i * 5;
		}
	}
	return -1;
}

function bool PickDoor(PlayerPawn P)
{
	local DeusExMover D;
	local float Best;

	Best = 1000000;
	foreach P.AllActors(class'DeusExMover', D)
	{
		if (D.KeyRot[1].Yaw == 0 || D.KeyPos[1] != vect(0,0,0) || D.KeyNum != 0 || D.bLocked || D.bInterpolating)
			continue;
		if (VSize(D.Location - P.Location) < Best)
		{
			Best = VSize(D.Location - P.Location);
			Door = D;
		}
	}
	return Door != None;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Leaf;
	local CapGhostToucher G;

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

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXGHOST: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		Step = 1;
		if (!PickDoor(P))
		{
			Log("DXGHOST: no door");
			P.ConsoleCommand("exit");
			return;
		}
		Leaf = LeafAngle(P);
		Log("DXGHOST: door " $ Door.Name $ " at " $ Door.Location $ " leaf " $ Leaf);
		G = P.Spawn(class'CapGhostToucher',,, Door.Location + Out(Leaf, 32) + Out(Leaf + 90, 40));
		if (G == None)
		{
			Log("DXGHOST: no toucher");
			P.ConsoleCommand("exit");
			return;
		}
		Log("DXGHOST: toucher at " $ G.Location);
		G.Move(Out(Leaf + 90, -80));
		Log("DXGHOST: moved across to " $ G.Location);
		G.Move(Out(Leaf + 90, 80));
		Log("DXGHOST: moved back to " $ G.Location);
		Log("DXGHOST: done, exiting");
		P.ConsoleCommand("exit");
	}
}
