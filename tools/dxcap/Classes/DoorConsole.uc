//=============================================================================
// DoorConsole: a hinged door closing on a crate. On TargetMap
// ([DXCapture.DoorConsole], UNATCO HQ by default), 2 s in, the DeusExMover
// nearest the player that turns and does not slide (its KeyRot[1]'s yaw not
// 0, its KeyPos[1] none), closed and not locked. Its leaf's angle round the
// hinge (the door's location) is found by short lines across the circle 32
// units out, every 5 degrees, at its height and 32 and 64 units above and
// below: the first that meets the door. The door opened (DoOpen), and its
// leaf found again once it stops; a breakable crate spawned 40 units out
// between the two angles and left to settle 1.5 s; then the door closed
// (DoClose): a mover crushes a decoration it encroaches on, whatever its
// MoverEncroachType. Each 0.1 s for 5 s the door's yaw, KeyNum and whether
// it moves, and the crate's place or that it is gone; then exit. Each line is
// logged with "DXDOOR:" in front.
//=============================================================================
class DoorConsole extends Console;

var config string TargetMap;

var string CurrentMap;
var float MapTime;
var float StepTime;
var float LogTime;
var int Step;
var DeusExMover Door;
var Actor Crate;
var float ClosedAngle, OpenAngle;

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

// The angle of the door's leaf round its hinge, or -1.
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

function LogState(string What)
{
	local string S;

	if (Crate == None || Crate.bDeleteMe)
		S = "gone";
	else
		S = int(Crate.Location.X) $ "," $ int(Crate.Location.Y) $ "," $ int(Crate.Location.Z);
	Log("DXDOOR: " $ What $ " t " $ int(StepTime * 10 + 0.5) $ " yaw " $ (Door.Rotation.Yaw & 65535) $ " key " $ Door.KeyNum $ " moving " $ Door.bInterpolating $ " crate " $ S);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Mid;
	local vector Spot;

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
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXDOOR: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		if (!PickDoor(P))
		{
			Log("DXDOOR: no door");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		ClosedAngle = LeafAngle(P);
		Log("DXDOOR: door " $ Door.Name $ " at " $ Door.Location $ " yaw " $ (Door.Rotation.Yaw & 65535) $ " turns " $ Door.KeyRot[1].Yaw
			$ " encroach " $ Door.MoverEncroachType $ " leaf " $ ClosedAngle);
		Door.DoOpen();
		StepTime = 0;
		Step = 1;
	}
	else if (Step == 1 && StepTime > 0.5 && !Door.bInterpolating)
	{
		OpenAngle = LeafAngle(P);
		Mid = OpenAngle - ClosedAngle;
		if (Mid > 180)
			Mid -= 360;
		if (Mid < -180)
			Mid += 360;
		Mid = ClosedAngle + Mid / 2;
		Spot = Door.Location + Out(Mid, 40);
		Crate = P.Spawn(class'CrateBreakableMedGeneral',,, Spot);
		Log("DXDOOR: open, leaf " $ OpenAngle $ "; crate at " $ Mid $ " degrees: " $ Crate);
		StepTime = 0;
		Step = 2;
	}
	else if (Step == 2 && StepTime > 1.5)
	{
		LogState("closing");
		Door.DoClose();
		StepTime = 0;
		LogTime = 0;
		Step = 3;
	}
	else if (Step == 3)
	{
		if (StepTime >= LogTime)
		{
			LogState("state");
			LogTime += 0.1;
		}
		if (StepTime > 5.0)
		{
			LogState("end");
			Log("DXDOOR: exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
	}
}
