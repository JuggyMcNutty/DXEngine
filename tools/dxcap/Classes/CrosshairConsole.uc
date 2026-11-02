//=============================================================================
// CrosshairConsole: where the HUD draws the crosshair and the accuracy
// marks, each line starting "DXCROSS:" (docs/DEVELOPMENT.md, scripted runs).
// On Liberty Island, opened from wherever the run starts, 6 s in: the view
// level, a large crate 200 units ahead at the eye's height, still, the
// target the HUD draws the accuracy marks round (a pawn, a decoration or a
// breakable mover in the weapon's range), a pistol given to the player and
// put in hand, and the HUD's hit display, compass and message log hidden, as
// they draw over the grabber's mark. Logged: the root's size, the HUD's
// place and size, the crosshair window's and the augmentation display's, its
// last target and the weapon's accuracy. Then three shots 2.6 s apart, each
// marked for the original's grabber: the HUD shown, hidden, shown again;
// then an exit. What is compared: the HUD's own pixels in the view's middle,
// the third shot less the second (tools/dxcap/hud.py), column by column and
// row by row; the first comes before the weapon's accuracy settles (0.3,
// where it is 0.1 by the second), so its marks lie where each run's
// accuracy puts them.
//=============================================================================
class CrosshairConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Actor Crate;
var DeusExWeapon Pistol;

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

function string Place(Window W)
{
	if (W == None)
		return "None";
	return W.x $ "," $ W.y $ " " $ W.width $ "x" $ W.height $ " visible " $ W.IsVisible();
}

// What the next shot shows, set up as its mark goes up.
function Step(DeusExRootWindow Root, int Shot)
{
	if (Shot == 1)
	{
		Root.ShowHud(False);
		Log("DXCROSS: the HUD hidden");
	}
	else if (Shot == 2)
	{
		Root.ShowHud(True);
		Log("DXCROSS: the HUD shown");
	}
	else
		Log("DXCROSS: the HUD as it is");
}

// The windows the mark lies under, kept hidden.
function HideCorner(DeusExRootWindow Root)
{
	if (Root.hud.hit != None && Root.hud.hit.IsVisible())
		Root.hud.hit.Hide();
	if (Root.hud.compass != None && Root.hud.compass.IsVisible())
		Root.hud.compass.Hide();
	if (Root.hud.msgLog != None && Root.hud.msgLog.IsVisible())
		Root.hud.msgLog.Hide();
}

function LogWindows(DeusExRootWindow Root)
{
	Log("DXCROSS: root " $ Root.width $ "x" $ Root.height $ ", hud " $ Place(Root.hud) $ ", cross "
		$ Place(Root.hud.cross) $ ", augDisplay " $ Place(Root.hud.augDisplay));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local rotator R, Facing;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None)
		Root = DeusExRootWindow(DXP.rootWindow);
	if (Phase >= 2 && Root != None && Root.hud != None)
		HideCorner(Root);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXCROSS: opening 01_NYC_UNATCOIsland");
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
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None && Root != None)
		{
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			Facing.Yaw = R.Yaw;
			Crate = P.Spawn(class'CrateUnbreakableLarge',,, P.Location + 200 * vector(Facing) + vect(0, 0, 1) * P.BaseEyeHeight, Facing);
			if (Crate != None)
				Crate.SetPhysics(PHYS_None);
			Pistol = DXP.Spawn(class'WeaponPistol', DXP);
			if (Pistol != None)
			{
				Pistol.GiveTo(DXP);
				Pistol.GiveAmmo(DXP);
				DXP.PutInHand(Pistol);
			}
			Log("DXCROSS: the player at " $ P.Location $ " yaw " $ R.Yaw $ ", crate " $ Crate $ ", pistol " $ Pistol);
			HideCorner(Root);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// A shot every 2.6 s from 3.6 s on, the mark shown for the 0.6 s before
		// each, the step set up as it goes up.
		if (StepTime > 3.0 && ShotsTaken < 3)
		{
			if (!bMarking)
			{
				Step(Root, ShotsTaken);
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				LogWindows(Root);
				Log("DXCROSS: weapon " $ DXP.Weapon $ " accuracy " $ Pistol.currentAccuracy $ ", last target "
					$ Root.hud.augDisplay.lastTarget);
				Log("DXCROSS: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
			}
		}
		else if (ShotsTaken >= 3 && StepTime > 3.0)
		{
			Log("DXCROSS: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
