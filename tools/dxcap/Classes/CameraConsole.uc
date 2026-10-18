//=============================================================================
// CameraConsole: the game's viewport windows (docs/NATIVES.md, viewport
// windows) -- a security computer's camera feed with the player in its view,
// its pan and its zoom, the targeting augmentation's zoom window and the spy
// drone's. Liberty Island is opened from wherever the run starts. 2 s into
// the level a security camera, tagged DXCamCamera, is placed up to 240 units
// out along the clearest of eight lines from the player's eyes, 40 units
// over them, facing the player, still (no swing, no alarm), and beside the
// player a security computer whose first view it is; the player logs in as
// the login screen does. The feed's window is logged (enabled, field of
// view, origin) with the player's last render time, and shot. Then the left
// arrow and the plus key go to the screen's PanCamera, as a player's keys
// do: the camera's desired turn and the window's field of view are logged,
// and 3 s later the feed is shot again. The terminal closed, the player is
// put before Paul Denton on the dock, looking at him, and takes the
// targeting augmentation at its top level: the HUD's zoom window is logged
// and shot. Then the spy drone:
// its window logged and shot. Each shot is marked for the original's grabber
// (the HUD's hit display, over the mark, hidden); then an exit. Each line is
// logged with "DXCAM:" in front.
//=============================================================================
class CameraConsole extends Console;

var float MapTime;
var int Phase;
var float NextTime;
var Object Stopped;
var SecurityCamera Cam;
var ComputerSecurity Comp;
var Pawn Target;
var Augmentation Aug;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

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

// A conversation's camera would take the view.
function Silence(DeusExPlayer DXP)
{
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXCAM: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

function string Place(vector V)
{
	return int(V.X) $ "," $ int(V.Y) $ "," $ int(V.Z);
}

function string Turn(rotator R)
{
	return R.Pitch $ "," $ R.Yaw $ "," $ R.Roll;
}

// A viewport window's state, as its fields hold it.
function LogWindow(string What, ViewportWindow W)
{
	if (W == None)
	{
		Log("DXCAM: " $ What $ " window: none");
		return;
	}
	Log("DXCAM: " $ What $ " window: enabled " $ W.bEnableViewport $ " fov " $ int(W.fov * 10) / 10.0
		$ " origin " $ W.originActor $ " watch " $ W.watchActor $ " eye level " $ W.bUseEyeHeight
		$ " watch eye level " $ W.bWatchEyeHeight $ " view rotation " $ W.bUseViewRotation
		$ " at " $ Place(W.location) $ " size " $ int(W.width) $ "x" $ int(W.height));
}

// How long ago the renderer last drew the player: only a window from
// elsewhere draws it, the main view being its own eyes.
function LogPlayerDrawn(PlayerPawn P, string When)
{
	Log("DXCAM: " $ When $ ": the player drawn " $ int((P.Level.TimeSeconds - P.LastRenderTime) * 100) / 100.0 $ " s ago");
}

// The camera, facing the player, and the computer whose first view it is.
function bool SetUp(DeusExPlayer P)
{
	local vector Eye, Dir, Best, Loc;
	local rotator R;
	local float Len, BestLen;
	local int i;

	Eye = P.Location + vect(0,0,1) * P.BaseEyeHeight;
	for (i = 0; i < 8; i++)
	{
		R.Yaw = i * 8192;
		Dir = vector(R);
		for (Len = 240; Len >= 100; Len -= 20)
			if (P.FastTrace(Eye + Dir * Len + vect(0,0,40), Eye))
				break;
		if (Len > BestLen)
		{
			BestLen = Len;
			Best = Dir;
		}
	}
	Loc = Eye + Best * BestLen + vect(0,0,40);
	Cam = P.Spawn(class'SecurityCamera', , 'DXCamCamera', Loc, rotator(P.Location - Loc));
	if (Cam == None)
	{
		Log("DXCAM: no room for the camera at " $ Place(Loc));
		return false;
	}
	Cam.bSwing = False;
	Cam.bNoAlarm = True;
	Log("DXCAM: camera " $ Cam.Name $ " at " $ Place(Cam.Location) $ ", " $ int(VSize(Cam.Location - Eye)) $ " units from the eyes, turned "
		$ Turn(Cam.Rotation) $ "; the player at " $ Place(P.Location));

	R = rotator(-Best);
	for (i = 0; i < 4 && Comp == None; i++)
	{
		Comp = P.Spawn(class'ComputerSecurity', , , P.Location + 64 * vector(R));
		R.Yaw += 16384;
	}
	if (Comp == None)
	{
		Log("DXCAM: no room for the computer");
		return false;
	}
	Comp.userList[0].userName = "JCD";
	Comp.userList[0].password = "BIONICMAN";
	Comp.Views[0].cameraTag = 'DXCamCamera';
	Comp.Views[0].titleString = "DXCam";
	Comp.bLockedOut = false;
	Log("DXCAM: computer " $ Comp.Name $ " at " $ Place(Comp.Location));
	return true;
}

function NetworkTerminal Terminal(DeusExPlayer P)
{
	return NetworkTerminal(DeusExRootWindow(P.rootWindow).GetTopWindow());
}

function LogIn(DeusExPlayer P)
{
	local NetworkTerminal Term;
	local ComputerScreenLogin Login;

	Term = Terminal(P);
	if (Term != None)
		Login = ComputerScreenLogin(Term.winComputer);
	if (Login == None)
	{
		Log("DXCAM: no login screen: " $ Term);
		return;
	}
	Login.editUserName.SetText(Comp.GetUserName(0));
	Login.editPassword.SetText(Comp.GetPassword(0));
	Log("DXCAM: logging in as " $ Comp.GetUserName(0));
	Login.ProcessLogin();
}

function ComputerScreenSecurity Screen(DeusExPlayer P)
{
	local NetworkTerminal Term;

	Term = Terminal(P);
	if (Term == None)
		return None;
	return ComputerScreenSecurity(Term.winComputer);
}

// Paul Denton, who stands on the dock: the player put up to 300 units from
// him along the clearest of eight lines out from his eyes, looking at him.
function Pawn FacePaul(DeusExPlayer P)
{
	local ScriptedPawn S, Paul;
	local vector Center, Dir, Best, Eye;
	local rotator R;
	local float Len, BestLen;
	local int i;

	foreach P.AllActors(class'ScriptedPawn', S)
		if (S.BindName == "PaulDenton")
			Paul = S;
	if (Paul == None)
		return None;
	Center = Paul.Location + vect(0,0,1) * Paul.BaseEyeHeight;
	for (i = 0; i < 8; i++)
	{
		R.Yaw = i * 8192;
		Dir = vector(R);
		for (Len = 300; Len >= 100; Len -= 20)
			if (P.FastTrace(Center + Dir * Len, Center))
				break;
		if (Len > BestLen)
		{
			BestLen = Len;
			Best = Dir;
		}
	}
	P.SetLocation(Paul.Location + Best * BestLen);
	Eye = P.Location + vect(0,0,1) * P.BaseEyeHeight;
	P.ViewRotation = rotator(Paul.Location + vect(0,0,1) * Paul.BaseEyeHeight * 0.5 - Eye);
	P.SetRotation(P.ViewRotation);
	return Paul;
}

function Mark(int N)
{
	MarkShot = N;
	bMarking = true;
}

function Shoot(PlayerPawn P, string What)
{
	Log("DXCAM: shot " $ MarkShot $ ": " $ What);
	P.ConsoleCommand("shot");
	bMarking = false;
}

function HudForShots(DeusExPlayer DXP)
{
	DXP.bHitDisplayVisible = false;
	if (DeusExRootWindow(DXP.rootWindow) != None)
		DeusExRootWindow(DXP.rootWindow).ShowHud(true);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ComputerScreenSecurity Sec;
	local AugmentationDisplayWindow Display;
	local float T;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None)
		Silence(DXP);
	T = P.Level.TimeSeconds;

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXCAM: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = -1;
			}
		}
		else if (T > 2.0 && DXP != None)
		{
			if (!SetUp(DXP))
			{
				P.ConsoleCommand("exit");
				Phase = 99;
				return;
			}
			HudForShots(DXP);
			NextTime = T + 1.0;
			Phase = 1;
		}
		return;
	}
	if (Phase == -1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0)
		{
			MapTime = 0;
			Phase = 0;
		}
		return;
	}
	if (DXP == None || T < NextTime)
		return;
	Display = DeusExRootWindow(DXP.rootWindow).hud.augDisplay;

	switch (Phase)
	{
	case 1:
		DXP.InvokeComputerScreen(Comp, 0, T);
		NextTime = T + 1.0;
		Phase++;
		break;
	case 2:
		LogIn(DXP);
		NextTime = T + 2.0;
		Phase++;
		break;
	case 3:
		Sec = Screen(DXP);
		if (Sec == None)
		{
			Log("DXCAM: no security screen: " $ Terminal(DXP));
			NextTime = T + 1.0;
			Phase = 8;
		}
		else
		{
			Log("DXCAM: the feed's camera " $ Sec.winCameras[0].camera $ ", the selected " $ Sec.selectedCamera.Name $ ", pan step " $ Sec.panMod);
			LogWindow("the feed's", Sec.winCameras[0].winCamera);
			LogPlayerDrawn(P, "the feed up");
			Mark(0);
			NextTime = T + 0.6;
			Phase++;
		}
		break;
	case 4:
		LogPlayerDrawn(P, "at the first shot");
		Shoot(P, "the feed, the player in view");
		NextTime = T + 0.5;
		Phase++;
		break;
	case 5:
		Sec = Screen(DXP);
		if (Sec != None)
		{
			Log("DXCAM: before the keys: camera turned " $ Turn(Cam.Rotation) $ ", desired " $ Turn(Cam.DesiredRotation));
			Sec.PanCamera(IK_Left);
			Log("DXCAM: left: desired " $ Turn(Cam.DesiredRotation));
			Sec.PanCamera(IK_Equals);
			Log("DXCAM: plus: the feed's field of view " $ int(Sec.winCameras[0].winCamera.fov * 10) / 10.0);
		}
		NextTime = T + 3.0;
		Phase++;
		break;
	case 6:
		Log("DXCAM: 3 s on: camera turned " $ Turn(Cam.Rotation));
		Mark(1);
		NextTime = T + 0.6;
		Phase++;
		break;
	case 7:
		LogPlayerDrawn(P, "at the second shot");
		Shoot(P, "the feed, turned and zoomed");
		NextTime = T + 0.5;
		Phase++;
		break;
	case 8:
		if (Terminal(DXP) != None)
			Terminal(DXP).CloseScreen("EXIT");
		HudForShots(DXP);
		NextTime = T + 1.0;
		Phase++;
		break;
	case 9:
		Target = FacePaul(DXP);
		if (Target == None)
			Log("DXCAM: no Paul Denton");
		else
			Log("DXCAM: looking at " $ Target.Name $ " at " $ Place(Target.Location) $ " from " $ Place(P.Location) $ ", "
				$ int(VSize(Target.Location - P.Location)) $ " units, turned " $ Turn(P.ViewRotation));
		Aug = DXP.AugmentationSystem.GivePlayerAugmentation(class'AugTarget');
		if (Aug != None)
		{
			Aug.CurrentLevel = 3;
			Aug.Activate();
		}
		NextTime = T + 2.0;
		Phase++;
		break;
	case 10:
		Log("DXCAM: targeting active " $ Display.bTargetActive $ " level " $ Display.targetLevel $ " target " $ Display.lastTarget);
		LogWindow("the targeting zoom's", Display.winZoom);
		Mark(2);
		NextTime = T + 0.6;
		Phase++;
		break;
	case 11:
		Shoot(P, "the targeting zoom");
		if (Aug != None)
			Aug.Deactivate();
		NextTime = T + 1.0;
		Phase++;
		break;
	case 12:
		Aug = DXP.AugmentationSystem.GivePlayerAugmentation(class'AugDrone');
		if (Aug != None)
			Aug.Activate();
		NextTime = T + 2.0;
		Phase++;
		break;
	case 13:
		Log("DXCAM: drone active " $ DXP.bSpyDroneActive $ " drone " $ DXP.aDrone);
		LogWindow("the drone's", Display.winDrone);
		Mark(3);
		NextTime = T + 0.6;
		Phase++;
		break;
	case 14:
		Shoot(P, "the drone's window");
		NextTime = T + 1.0;
		Phase++;
		break;
	case 15:
		Log("DXCAM: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
		break;
	}
}
