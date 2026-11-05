//=============================================================================
// ComputerTreeConsole: the InfoLink's computer window and its children, the
// same in both engines (docs/DEVELOPMENT.md, scripted runs; bugs.md EXT.24):
// the original's Init (XComputerWindow::Init, Extension.dll 0x1000d530)
// gives it a plain window that fills it, shown, its textWindow, and a hidden
// edit field, its inputWindow, and the root's player as its player. From the
// menu map, 3 s in, the HUD is shown and its info link window made, as an
// InfoLink makes it (CreateInfoLinkWindow), and given a line of text. A
// second and a half later the computer window is logged ("DXCOMPUTER:"):
// its place and size, its textWindow, inputWindow and player, where its
// typing has reached, and each child bottom to top -- class, place, size,
// shown or hidden and which of the two it is. Then an exit.
//=============================================================================
class ComputerTreeConsole extends Console;

var float RunTime;
var int Step;
var HUDInfoLinkDisplay Link;

function string NameOf(Object O)
{
	if (O == None)
		return "None";
	return string(O.Class.Name);
}

function LogComputer(PlayerPawn P, ComputerWindow W)
{
	local Window C;
	local string S;
	local int n;

	Log("DXCOMPUTER: the InfoLink's computer window " $ W.Class.Name $ " at " $ W.x $ "," $ W.y $ " size " $ W.width $ "x" $ W.height
		$ ", textWindow " $ NameOf(W.textWindow) $ ", inputWindow " $ NameOf(W.inputWindow) $ ", player is the player " $ (W.player == P));
	Log("DXCOMPUTER: typed to column " $ W.textX $ " of row " $ W.textY);
	for (C = W.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		S = "DXCOMPUTER:   child " $ C.Class.Name $ " at " $ C.x $ "," $ C.y $ " size " $ C.width $ "x" $ C.height;
		if (C.bIsVisible)
			S = S $ " shown";
		else
			S = S $ " hidden";
		if (C == W.textWindow)
			S = S $ ", its textWindow";
		if (C == W.inputWindow)
			S = S $ ", its inputWindow";
		Log(S);
		n++;
	}
	Log("DXCOMPUTER: " $ n $ " children");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None || Root.hud == None)
		return;

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		Root.ShowHud(True);
		Link = Root.hud.infolink;
		if (Link == None)
			Link = Root.hud.CreateInfoLinkWindow();
		Link.Show();
		Link.DisplayText("ComputerTreeConsole");
		Log("DXCOMPUTER: info link made, its text printed");
		break;
	case 1:
		if (RunTime < 4.5)
			return;
		LogComputer(P, Link.winText);
		break;
	case 2:
		if (RunTime < 5.0)
			return;
		Log("DXCOMPUTER: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
