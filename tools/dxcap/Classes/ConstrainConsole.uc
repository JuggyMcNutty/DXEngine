//=============================================================================
// ConstrainConsole: what GC.DrawActor's bConstrain does, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the UI). On Liberty
// Island, the player out of the world and held, the HUD hidden,
// conversations and datalinks stopped; 3 s in, a crate
// (CrateUnbreakableSmall, hidden, so the scene draws none) 96 units before
// the eye, and on the root a CapActorWindow over its top right quarter that
// draws the crate, constrained to the window: a shot; then not constrained:
// a shot; then an exit. The original narrows the scene's frame to the window
// for the draw (Extension.dll XGC::DrawActor). Each shot is marked for the
// original's grabber; each line is logged with "DXCONSTRAIN:" in front.
//=============================================================================
class ConstrainConsole extends Console;

var float MapTime;
var string CurrentMap;
var int Phase;
var int MarkShot;
var bool bMarking;
var bool bOut;
var DeusExDecoration Crate;
var CapActorWindow Drawer;
var vector Eye;
var rotator Look;
var Object Stopped;

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

function Hold(PlayerPawn P)
{
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0,0,0);
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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

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
	P.ReducedDamageType = 'All';
	Silence(P);

	if (Phase == 0)
	{
		// The original starts in its menu map
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCONSTRAIN: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root == None)
			return;
		Eye = P.Location + vect(0,0,1) * P.EyeHeight;
		Look = P.ViewRotation;
		Look.Pitch = 0;
		Crate = P.Spawn(class'CrateUnbreakableSmall',,, Eye + vector(Look) * 96);
		if (Crate == None)
		{
			Log("DXCONSTRAIN: no crate spawned, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Crate.SetCollision(false, false, false);
		Crate.bCollideWorld = false;
		Crate.SetPhysics(PHYS_None);
		Crate.bHidden = true;
		DeusExPlayer(P).ShowHud(False);
		Hold(P);
		Drawer = CapActorWindow(Root.NewChild(class'CapActorWindow'));
		Drawer.SetPos(Root.width / 2, 0);
		Drawer.SetSize(Root.width / 2, Root.height / 2);
		Drawer.Target = Crate;
		Drawer.bConstrain = true;
		Drawer.Show();
		Log("DXCONSTRAIN: crate at " $ Crate.Location $ ", the root " $ Root.width $ "x" $ Root.height
			$ ", the window at " $ Drawer.x $ "," $ Drawer.y $ " " $ Drawer.width $ "x" $ Drawer.height);
		Phase = 1;
		MapTime = 0;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && MapTime > 1.0)
	{
		MarkShot = 0;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 1.8)
	{
		Log("DXCONSTRAIN: shot 0, constrained");
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Drawer.bConstrain = false;
		Phase = 3;
	}
	else if (Phase == 3 && MapTime > 2.8)
	{
		MarkShot = 1;
		bMarking = true;
		Phase = 4;
	}
	else if (Phase == 4 && MapTime > 3.6)
	{
		Log("DXCONSTRAIN: shot 1, not constrained");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 5;
	}
	else if (Phase == 5 && MapTime > 4.6)
	{
		Log("DXCONSTRAIN: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's: a
// magenta block, then the shot's number in eight blocks
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
