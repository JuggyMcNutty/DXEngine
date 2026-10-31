//=============================================================================
// LoginConsole: a computer's login screen with text in its fields, logged
// with "DXLOGIN:" and the console's clock (docs/DEVELOPMENT.md, scripted
// runs; bugs.md EXT.18).
//
// On Liberty Island, opened from wherever the run starts, 6 s in: the map's
// first personal computer's screen opened as the player opens it
// (InvokeComputerScreen), not logged in to. A second later "DENTON" is put
// in its user name field and "BIONICMAN" in its password field as typing
// puts them (InsertText: the insertion point after the text, clear of the
// first letter), the focus left on the user name field, where the screen
// puts it; each field's text and insertion point and the screen's tree of
// windows (WindowDump) are logged. A second later a marked shot, and
// another 1.5 s after it; then an exit. The screen is centred at half a unit
// (353 by 195), so each field's text starts half a unit past a whole one;
// at DXCAP_WINDOW=1280x960 (a multiplier of 2, both roots 640x480) that is
// a pixel. What is compared between the engines: each field's first
// letter, its left columns (shots.py --region).
//=============================================================================
class LoginConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var bool bTravelled;
var int ShotsTaken;
var Computers Comp;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
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

function ComputerScreenLogin LoginScreen(DeusExPlayer P)
{
	local NetworkTerminal Term;

	Term = NetworkTerminal(DeusExRootWindow(P.rootWindow).GetTopWindow());
	if (Term == None)
		return None;
	return ComputerScreenLogin(Term.winComputer);
}

// The map's first personal computer's screen, as the player opens it.
function OpenComputer(DeusExPlayer P)
{
	local ComputerPersonal Found;

	foreach P.AllActors(class'ComputerPersonal', Found)
	{
		Comp = Found;
		break;
	}
	if (Comp == None)
	{
		Log("DXLOGIN: no personal computer, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	Log("DXLOGIN: opening " $ Comp.Name $ " at " $ MapTime);
	Comp.bLockedOut = false;
	P.InvokeComputerScreen(Comp, 0, P.Level.TimeSeconds);
}

function TypeInFields(DeusExPlayer P)
{
	local ComputerScreenLogin Login;

	Login = LoginScreen(P);
	if (Login == None)
	{
		Log("DXLOGIN: no login screen, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	Login.editUserName.InsertText("DENTON");
	Login.editPassword.InsertText("BIONICMAN");
	Log("DXLOGIN: user name '" $ Login.editUserName.GetText() $ "' at " $ Login.editUserName.GetInsertionPoint()
		$ ", password '" $ Login.editPassword.GetText() $ "' at " $ Login.editPassword.GetInsertionPoint()
		$ ", user name focused " $ Login.editUserName.IsFocusWindow());
	class'WindowDump'.static.DumpWindow(P, DeusExRootWindow(P.rootWindow).GetTopWindow(), 0, "DXLOGIN: ");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	// Only the island needs Deus Ex's player; the menu map's travels.
	if (DXP == None && M ~= "01_NYC_UNATCOIsland")
		return;
	P.ReducedDamageType = 'All';

	// A shot's mark up for 0.8 s before it.
	if (bMarking)
	{
		if (StepTime > 0.8)
		{
			Log("DXLOGIN: shot " $ ShotsTaken $ " at " $ MapTime);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			StepTime = 0;
		}
		return;
	}

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0 && !bTravelled)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				bTravelled = true;
			}
		}
		else if (MapTime > 6.0)
		{
			OpenComputer(DXP);
			if (Phase == 0)
				Phase = 1;
			StepTime = 0;
		}
	}
	else if (Phase == 1 && StepTime > 1.0)
	{
		TypeInFields(DXP);
		if (Phase == 1)
			Phase = 2;
		StepTime = 0;
	}
	else if ((Phase == 2 && StepTime > 1.0) || (Phase == 3 && StepTime > 1.5))
	{
		MarkShot = ShotsTaken;
		bMarking = true;
		StepTime = 0;
		Phase++;
	}
	else if (Phase == 4 && StepTime > 1.0)
	{
		Log("DXLOGIN: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
