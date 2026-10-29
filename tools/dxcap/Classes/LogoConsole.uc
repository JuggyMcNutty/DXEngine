//=============================================================================
// LogoConsole: a window's parent as its script reads it, logged with
// "DXLOGO:" (docs/DEVELOPMENT.md, scripted runs). From wherever the run
// starts, 2 s in: the cheats enabled and the ShowQuotesWindow cheat's
// QuotesWindow opened, whose ten SpinningLogoWindows bounce off their
// parent's size (winParent.width and height, read each tick). 1 s and 3 s
// later, for each logo: its winParent, that parent's size and the logo's
// place, which stays within it; then an exit.
//=============================================================================
class LogoConsole extends Console;

var float RunTime;
var int Phase;

function Report(string When)
{
	local SpinningLogoWindow W;
	local int N;

	foreach AllObjects(class'SpinningLogoWindow', W)
	{
		N++;
		if (W.winParent == None)
			Log("DXLOGO: " $ When $ " " $ W.Name $ " winParent None, at " $ int(W.x) $ "," $ int(W.y));
		else
			Log("DXLOGO: " $ When $ " " $ W.Name $ " winParent " $ W.winParent.Class.Name $ " "
				$ int(W.winParent.width) $ "x" $ int(W.winParent.height) $ ", inside "
				$ (W.x >= 0 && W.y >= 0 && W.x + W.width <= W.winParent.width && W.y + W.height <= W.winParent.height));
	}
	Log("DXLOGO: " $ When $ " " $ N $ " logos");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		P.ConsoleCommand("set DeusEx.JCDentonMale bCheatsEnabled True");
		P.ConsoleCommand("set DeusEx.JCDentonFemale bCheatsEnabled True");
		Log("DXLOGO: opening the quotes window");
		P.ConsoleCommand("showquoteswindow");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Report("1 s");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		Report("3 s");
		Log("DXLOGO: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
