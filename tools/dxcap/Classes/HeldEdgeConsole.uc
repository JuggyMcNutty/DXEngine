//=============================================================================
// HeldEdgeConsole (bugs.md D.71): what a pawn holds as its
// weapon triangle leaves the view.
//
// Render.dll's DrawLodMesh (0x10b0ea80) clears GMeshHadWeaponTriangle and
// sets it, with GWeaponCoords, only when the outcodes of the mesh's special
// vertices, the weapon triangle's corners, ANDed together are 0 (initial
// mask 60, the four sides 4, 8, 16 and 32: tables 0x10b2abbc-0x10b2abc8):
// a triangle wholly beyond one side of the view holds nothing, and
// DrawActorSprite (0x10b25080) draws the weapon or the selected item only
// with it set. DXEngine draws what is held wherever the triangle lies
// (Render/VisibleMesh.cpp:98; the outcode test only for wire, :136-151).
//
// On Liberty Island, the player out of the world at the start's eye, the HUD
// hidden, every other pawn gone, conversations and datalinks stopped, 3 s
// in: a UNATCOTroop Ahead units (160) ahead of the eye, turned to face the
// view's right (its gun pointing right), ordered to stand, its weapon drawn
// (SetupWeapon(true, true)). Then 16 views, the start's yaw turned right by
// 0, 2, ... 30 degrees, the troop crossing to the left edge and out: each
// held 1 s, then a marked shot; "DXHELDEDGE:" lines log each view, the
// troop's Weapon and its place.
//
// Compare the shots' left 320 columns (shots.py --region 0,0,320,768 at
// DXCAP_WINDOW=1024x768, the original with DXCAP_RENDERER=D3D). Expected:
// in the views where the troop's right hand is past the left edge and the
// gun's barrel is not, the original shows no gun, DXEngine the barrel.
//=============================================================================
class HeldEdgeConsole extends Console;

var config float Ahead;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var vector Eye;
var rotator Look, StartLook;
var Object Stopped;
var ScriptedPawn Troop;
var int View;

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

function Hush(PlayerPawn P, Actor Keep)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == P || A == Keep)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger'))
			A.Destroy();
	}
}

function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0,0,0);
}

function SetView(int N)
{
	Look = StartLook;
	Look.Yaw += N * 364;
	Log("DXHELDEDGE: view " $ N $ " turned " $ (N * 2) $ " degrees: troop " $ Troop.Name $ " at " $ Troop.Location
	    $ " weapon " $ Troop.Weapon $ " state " $ Troop.GetStateName());
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Spot;
	local rotator R;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
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
	Silence(P);

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXHELDEDGE: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			Eye = P.Location + vect(0,0,1) * P.EyeHeight;
			StartLook = P.Rotation;
			StartLook.Pitch = 0;
			StartLook.Roll = 0;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		if (Ahead == 0)
			Ahead = 160;
		Spot = Eye + vector(StartLook) * Ahead;
		Spot.Z = P.Location.Z;
		R = StartLook;
		R.Yaw += 16384;
		Troop = P.Spawn(class'UNATCOTroop',,, Spot, R);
		if (Troop == None)
			Troop = P.Spawn(class'UNATCOTroop',,, Spot + vect(0,0,20), R);
		if (Troop == None)
		{
			Log("DXHELDEDGE: no room for the troop at " $ Spot $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		// Its inventory is made by StartUp's Begin (InitializePawn), which
		// orders given at once pass over; and Standing's BeginState puts the
		// weapon away (SetupWeapon(false)) unless the pawn keeps it drawn.
		Troop.InitializePawn();
		Troop.bKeepWeaponDrawn = True;
		Troop.SetOrders('Standing', '', True);
		Troop.SetupWeapon(true, true);
		Hush(P, Troop);
		DeusExPlayer(P).ShowHud(False);
		Look = StartLook;
		Hold(P);
		Log("DXHELDEDGE: troop " $ Troop.Name $ " at " $ Troop.Location $ " facing " $ Troop.Rotation);
		StepTime = 0;
		Phase = 1;
		return;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	Hold(P);
	// 2 s for the troop to stand and draw its gun, then the views
	if (Phase == 1 && StepTime > 2.0)
	{
		View = 0;
		SetView(View);
		Hold(P);
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		MarkShot = View;
		bMarking = true;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.8)
	{
		Log("DXCAP: shot " $ View);
		P.ConsoleCommand("shot");
		bMarking = false;
		View++;
		if (View >= 16)
		{
			Log("DXHELDEDGE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		SetView(View);
		Hold(P);
		StepTime = 0;
		Phase = 2;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's
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
