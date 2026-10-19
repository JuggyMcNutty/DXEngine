//=============================================================================
// DetailConsole: a detail texture's bands by depth (DXEngine's: the
// original draws none here). At Liberty Island's start, 6 s in, the player
// is lifted off the pier, no collision, no physics, looking straight down at
// the planks (DWoodFine_A's detail texture), so the whole view lies at one
// depth: the eye's height over the planks. At each of Heights (10 to 400 by
// default, either side of each band's bound: 380, 90, 21.3) a shot with the
// render device's DetailTextures on, then one with it off (set
// ini:Engine.Engine.GameRenderDevice DetailTextures), each 0.5 s after the
// change; then exit. The HUD is hidden. Each shot is logged in turn, its
// height and the setting, with "DXDETAIL:"; tools/dxcap/detail.py reads the
// pairs. Read at DXCAP_WINDOW=1024x768.
//=============================================================================
class DetailConsole extends Console;

var config string Heights;

var float MapTime, StepTime;
var int Phase, Index, Shots;
var bool bDetailOn;
var float PlanksZ, Height;
var vector Spot;
var rotator Down;
var string Pending;

function bool NextHeight()
{
	local int i;

	if (Pending == "")
		return False;
	i = InStr(Pending, ",");
	if (i < 0)
	{
		Height = float(Pending);
		Pending = "";
	}
	else
	{
		Height = float(Left(Pending, i));
		Pending = Mid(Pending, i + 1);
	}
	return True;
}

function Hold(PlayerPawn P)
{
	Spot.Z = PlanksZ + Height - P.EyeHeight;
	P.SetLocation(Spot);
	P.Velocity = vect(0, 0, 0);
	P.Acceleration = vect(0, 0, 0);
	P.ViewRotation = Down;
}

function SetDetail(PlayerPawn P, bool bOn)
{
	bDetailOn = bOn;
	if (bOn)
		P.ConsoleCommand("set ini:Engine.Engine.GameRenderDevice DetailTextures True");
	else
		P.ConsoleCommand("set ini:Engine.Engine.GameRenderDevice DetailTextures False");
}

function Shot(PlayerPawn P)
{
	P.ConsoleCommand("shot");
	Log("DXDETAIL: shot " $ Shots $ " height " $ Height $ " detail " $ bDetailOn
		$ " eye " $ (P.Location.Z + P.EyeHeight - PlanksZ));
	Shots++;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector HitLocation, HitNormal, TexHit, TexNormal;
	local Actor HitActor, A;
	local name TexName, TexGroup;
	local int Flags;
	local string Floor;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase >= 2)
		Hold(P);

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXDETAIL: opening 01_NYC_UNATCOIsland");
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
	else if (Phase == 1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None)
		{
			HitActor = P.Trace(HitLocation, HitNormal, P.Location - vect(0, 0, 400), P.Location, False);
			if (HitActor == None)
			{
				Log("DXDETAIL: no floor under the start; exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			PlanksZ = HitLocation.Z;
			Spot = P.Location;
			Down = P.ViewRotation;
			Down.Pitch = -16384;
			Down.Roll = 0;
			foreach P.TraceTexture(class'Actor', A, TexName, TexGroup, Flags, TexHit, TexNormal, P.Location - vect(0, 0, 400), P.Location)
			{
				Floor = string(TexName);
				break;
			}
			Log("DXDETAIL: planks at " $ PlanksZ $ " under " $ P.Location $ ", eye height " $ P.EyeHeight
				$ ", texture " $ Floor);
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			P.SetCollision(False, False, False);
			P.bCollideWorld = False;
			P.SetPhysics(PHYS_None);
			Pending = Heights;
			NextHeight();
			SetDetail(P, True);
			Hold(P);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 0.5)
	{
		// Detail on, settled: its shot, then off
		Shot(P);
		SetDetail(P, False);
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.5)
	{
		Shot(P);
		if (NextHeight())
		{
			SetDetail(P, True);
			Hold(P);
			StepTime = 0;
			Phase = 2;
		}
		else
		{
			SetDetail(P, True);
			Log("DXDETAIL: " $ Shots $ " shots; exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	Heights="10,15,20,21,22,25,30,40,50,60,70,80,89,91,100,110,130,160,200,250,300,350,379,381,400"
}
