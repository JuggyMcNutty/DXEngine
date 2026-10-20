//=============================================================================
// PointerConsole: when the root window draws the pointer, the same in both
// engines (docs/DEVELOPMENT.md, scripted runs). At Liberty Island's start,
// level and facing the start's way, the HUD hidden, the pointer put in the
// middle of the root: five shots 2 s apart, each marked for the original's
// grabber -- with no modal window up; a modal window made hidden; it shown;
// it hidden again; it destroyed -- then exit. Each step is logged with
// "DXPOINTER:" in front.
//=============================================================================
class PointerConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var ModalWindow Modal;

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

// What the next shot shows, set up as its mark goes up.
function Step(DeusExRootWindow Root, int Shot)
{
	if (Shot == 1)
	{
		Modal = ModalWindow(Root.NewChild(class'ModalWindow', False));
		Log("DXPOINTER: a modal window made hidden: visible " $ Modal.IsVisible());
	}
	else if (Shot == 2)
	{
		Modal.Show();
		Log("DXPOINTER: shown: visible " $ Modal.IsVisible());
	}
	else if (Shot == 3)
	{
		Modal.Hide();
		Log("DXPOINTER: hidden again: visible " $ Modal.IsVisible());
	}
	else if (Shot == 4)
	{
		Modal.Destroy();
		Modal = None;
		Log("DXPOINTER: destroyed");
	}
	else
		Log("DXPOINTER: no modal window");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXPOINTER: opening 01_NYC_UNATCOIsland");
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
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			Root = DeusExRootWindow(DXP.rootWindow);
			// The original's HUD draws over the grabber's mark.
			if (Root != None)
			{
				Root.ShowHud(False);
				Root.SetCursorPos(Root.width / 2, Root.height / 2);
				Log("DXPOINTER: the root " $ Root.width $ "x" $ Root.height $ ", the pointer put at its middle");
			}
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// A shot every 2 s from 2 s on, the mark shown for the 0.6 s before
		// each, the step set up as it goes up.
		Root = DeusExRootWindow(DXP.rootWindow);
		if (StepTime > 2.0 && ShotsTaken < 5)
		{
			if (!bMarking)
			{
				Step(Root, ShotsTaken);
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 1.4;
			}
			else
			{
				Log("DXPOINTER: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 5 && StepTime > 2.0)
		{
			Log("DXPOINTER: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
