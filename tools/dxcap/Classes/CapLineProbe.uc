//=============================================================================
// CapLineProbe: LineEdgeConsole's actor. Setup, called in the console's tick,
// sets its marks and works out its spots, points and destinations once; Run
// asks each check and logs it with "DXLINE:", called in the console's tick
// and once more from its own Tick, in the level's.
//=============================================================================
class CapLineProbe extends Info;

var vector Spot, Eye, Solid, SolidNear, SolidFar, Wall, WallNormal;
var bool bSolid, bWall, bReady, bDone;
var vector Ends[81];
var CapReacher Jumper;
var vector JumpDest[6];
var float JumpSpeed[6];

function Setup(PlayerPawn P)
{
	local vector HitLoc, HitNorm, Dir;
	local rotator R;
	local Actor Hit;
	local int i, k;

	Spot = P.Location;
	Eye = Spot;
	Eye.Z += P.BaseEyeHeight;

	// The marks: along the first yaw from the facing, 8192 apart, with 200
	// units free
	R.Yaw = P.Rotation.Yaw;
	for (k = 0; k < 8; k++)
	{
		Dir = vector(R);
		Hit = Trace(HitLoc, HitNorm, Spot + Dir * 400, Spot, false);
		if (Hit == None || VSize(HitLoc - Spot) > 200)
			break;
		R.Yaw += 8192;
	}
	Log("DXLINE: the spot " $ Spot $ ", the marks along yaw " $ R.Yaw);
	for (i = 1; i <= 3; i++)
		Spawn(class'CapLoopMark',,, Spot + Dir * (50 * i));

	// Inside solid: 8 units behind the floor's face
	Hit = Trace(HitLoc, HitNorm, Spot - vect(0,0,1000), Spot, false);
	Solid = HitLoc - HitNorm * 8;
	SolidNear = Solid + vect(0.005,0,0);
	SolidFar = Solid + vect(0.02,0,0);
	bSolid = Hit != None && !FastTrace(Solid, Solid + HitNorm * 16);
	Log("DXLINE: the floor at " $ HitLoc $ " normal " $ HitNorm $ ", 8 behind it in solid: " $ bSolid);
	if (bSolid)
		Spawn(class'CapLoopMark',,, Solid);

	// The wall along neither X nor Y
	R.Yaw = P.Rotation.Yaw;
	for (k = 0; k < 32; k++)
	{
		Dir = vector(R);
		Hit = Trace(HitLoc, HitNorm, Eye + Dir * 2000, Eye, false);
		if (Hit == Level && Abs(HitNorm.X) > 0.1 && Abs(HitNorm.Y) > 0.1 && Abs(HitNorm.Z) < 0.7)
		{
			bWall = true;
			break;
		}
		R.Yaw += 2048;
	}
	if (bWall)
	{
		// The trace's hit is half a unit short of the face along the line
		Wall = HitLoc + Dir * 0.5;
		WallNormal = HitNorm;
		for (i = 0; i < 81; i++)
			Ends[i] = Wall + WallNormal * ((40 - i) * 0.00005);
	}
	Log("DXLINE: the wall: " $ bWall $ " at " $ Wall $ " normal " $ WallNormal $ ", yaw " $ R.Yaw);

	// The jumper, out of the world, and its destinations
	Jumper = Spawn(class'CapReacher',,, Spot);
	if (Jumper != None)
	{
		Jumper.SetCollision(false, false, false);
		Jumper.bCollideWorld = false;
		Jumper.SetPhysics(PHYS_None);
		JumpDest[0] = Jumper.Location + vect(64,0,-120);
		JumpSpeed[0] = 300;
		JumpDest[1] = Jumper.Location + vect(200,30,-420);
		JumpSpeed[1] = 300;
		JumpDest[2] = Jumper.Location + vect(128,-64,40);
		JumpSpeed[2] = 421.5;
		JumpDest[3] = Jumper.Location + vect(300,77,100);
		JumpSpeed[3] = 421.5;
		JumpDest[4] = Jumper.Location + vect(37.5,0,-333);
		JumpSpeed[4] = 117.25;
		JumpDest[5] = Jumper.Location + vect(64,64,50);
		JumpSpeed[5] = 0;
	}
	else
		Log("DXLINE: the jumper did not spawn");
	bReady = true;
}

function MarksWithin(string Where, float Radius, string Label)
{
	local CapLoopMark A;
	local string S;

	foreach VisibleActors(class'CapLoopMark', A, Radius, Spot)
		S = S $ " " $ int(VSize(A.Location - Spot) + 0.5);
	Log("DXLINE: " $ Where $ ": VisibleActors within " $ Label $ ":" $ S);
}

// The marks VisibleActors finds from a spot with no limit, the radius 0
function string MarksFromList(float Radius, vector From)
{
	local CapLoopMark A;
	local string S;

	foreach VisibleActors(class'CapLoopMark', A, Radius, From)
		S = S $ " " $ int(VSize(A.Location - Spot) + 0.5);
	return S;
}

function string Bit(bool B)
{
	if (B)
		return "1";
	return "0";
}

function Run(string Where)
{
	local Actor A;
	local string S;
	local int i;
	local vector V;

	MarksWithin(Where, 120, "120");
	MarksWithin(Where, -120, "-120");
	MarksWithin(Where, 100, "100");
	MarksWithin(Where, 0, "0");
	MarksWithin(Where, Sqrt(-1.0), "a NaN");

	if (bSolid)
	{
		Log("DXLINE: " $ Where $ ": in solid, FastTrace to itself " $ FastTrace(Solid, Solid) $ ", to 0.005 along X "
			$ FastTrace(SolidNear, Solid) $ ", to 0.02 along X " $ FastTrace(SolidFar, Solid));
		S = "";
		S = MarksFromList(0, Solid);
		Log("DXLINE: " $ Where $ ": VisibleActors from the solid spot:" $ S);
	}

	if (bWall)
	{
		S = "";
		for (i = 0; i < 81; i++)
			S = S $ Bit(FastTrace(Ends[i], Eye));
		Log("DXLINE: " $ Where $ ": the eye to the wall's points " $ S);
		S = "";
		for (i = 0; i < 81; i++)
			S = S $ Bit(FastTrace(Eye, Ends[i]));
		Log("DXLINE: " $ Where $ ": the wall's points to the eye " $ S);
	}

	if (Jumper != None)
	{
		for (i = 0; i < 6; i++)
		{
			Jumper.Destination = JumpDest[i];
			V.X = 0;
			V.Y = 0;
			V.Z = JumpSpeed[i];
			Jumper.Velocity = V;
			Log("DXLINE: " $ Where $ ": jump " $ i $ " at " $ JumpSpeed[i] $ ": " $ Jumper.EAdjustJump());
		}
	}
}

event Tick(float DeltaTime)
{
	if (!bReady || bDone)
		return;
	bDone = true;
	Run("level's tick");
}
