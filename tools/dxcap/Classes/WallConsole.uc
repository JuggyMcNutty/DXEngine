//=============================================================================
// WallConsole: the player's own weapon held against a wall -- TargetMap
// (UNATCO HQ when none is set: DXCAP_MISSION_MAP) opened from wherever the
// run starts; 6 s in, the assault rifle given as the game gives the
// starting pistol (a spawned one frobbed) and put in hand, the HUD hidden;
// then from where the player starts the nearest wall of the level's own in
// eight directions within 2000 units, and the player swept to it (Move: up
// to its collision radius and a unit off the hit, stopping at the first
// touch), level, facing it, and held there. Shots 4 s and 5 s later, marked
// for the original's grabber; the view and the weapon's place logged with
// each. Exits 2 s after. At 4:3 (DXCAP_WINDOW=1024x768) both engines' views
// are the same. Each line is logged with "DXWALL:".
//=============================================================================
class WallConsole extends Console;

var config string TargetMap;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var string CurrentMap;
var vector StandLoc;
var rotator StandRot;

var int MarkShot;
var bool bMarking;

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

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

// The nearest wall of the level's own, of eight directions from Start.
function bool FindWall(PlayerPawn P, vector Start)
{
	local int i;
	local rotator R;
	local vector HitLocation, HitNormal, Best, BestNormal;
	local actor HitActor;
	local float Dist, BestDist;

	BestDist = 100000;
	for (i = 0; i < 8; i++)
	{
		R.Pitch = 0;
		R.Yaw = i * 8192;
		R.Roll = 0;
		HitActor = P.Trace(HitLocation, HitNormal, Start + vector(R) * 2000, Start, false);
		Log("DXWALL: trace yaw " $ R.Yaw $ " hit " $ HitActor $ " at " $ HitLocation $ " normal " $ HitNormal);
		if (HitActor == P.Level && Abs(HitNormal.Z) < 0.2)
		{
			Dist = VSize(HitLocation - Start);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = HitLocation;
				BestNormal = HitNormal;
			}
		}
	}
	if (BestDist >= 100000)
		return false;
	BestNormal.Z = 0;
	BestNormal = Normal(BestNormal);
	StandLoc = Best + BestNormal * (P.CollisionRadius + 1);
	StandLoc.Z = Start.Z;
	StandRot = rotator(-BestNormal);
	StandRot.Pitch = 0;
	StandRot.Roll = 0;
	Log("DXWALL: wall at " $ Best $ " normal " $ BestNormal $ " distance " $ BestDist);
	return true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local Inventory Item;
	local DeusExWeapon W;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP == None)
		return;

	if (Phase == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXWALL: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Phase = -1;
		}
		return;
	}
	if (Phase == -1 && (CurrentMap ~= Target()))
		Phase = 0;

	if (Phase == 0 && MapTime > 6.0)
	{
		Item = P.Spawn(class'WeaponAssaultGun');
		Item.Frob(P, None);
		W = DeusExWeapon(P.FindInventoryType(class'WeaponAssaultGun'));
		if (W != Item)
			Item.Destroy();
		if (W == None)
		{
			Log("DXWALL: no rifle");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		if (!FindWall(P, P.Location))
		{
			Log("DXWALL: no wall");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		DXP.PutInHand(W);
		if (DeusExRootWindow(DXP.rootWindow) != None)
			DeusExRootWindow(DXP.rootWindow).ShowHud(False);
		// Swept to the wall (Move stops at the first touch), as walking into
		// it leaves the player, then held where it stopped.
		P.SetRotation(StandRot);
		P.Move(StandLoc - P.Location);
		StandLoc = P.Location;
		Log("DXWALL: against the wall at " $ StandLoc $ " facing " $ StandRot);
		StepTime = 0;
		Phase = 1;
	}
	if (Phase >= 1 && Phase < 9)
	{
		P.SetLocation(StandLoc);
		P.SetRotation(StandRot);
		P.ViewRotation = StandRot;
		P.Velocity = vect(0,0,0);
		P.SetPhysics(PHYS_None);
	}
	if (Phase == 1 && StepTime > 3.4 && ShotsTaken < 2)
	{
		if (!bMarking)
		{
			MarkShot = ShotsTaken;
			bMarking = true;
		}
		else if (StepTime > 4.0)
		{
			W = DeusExWeapon(P.Weapon);
			Log("DXWALL: shot " $ ShotsTaken $ " view at " $ P.Location $ " eye " $ P.EyeHeight $ " fov " $ P.FovAngle
				$ " weapon " $ W $ " at " $ W.Location $ " offset " $ W.PlayerViewOffset $ " scale " $ W.DrawScale);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			StepTime = 3.0;
		}
	}
	else if (Phase == 1 && ShotsTaken >= 2 && StepTime > 5.0)
	{
		Log("DXWALL: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
