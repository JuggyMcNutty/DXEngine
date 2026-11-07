//=============================================================================
// ComputerStartConsole: a computer window's ComputerStart and its pause
// flags, logged with "DXSTART:" (bugs.md EXT.26).
//
// The original's computer window is marked to start as it is shown
// (XComputerWindow::VisibilityChanged, Extension.dll 0x1000ee70); its next
// tick sends ComputerStart to it and to each parent until one takes it,
// paused or not, shown or not (Tick 0x1000f7b0). Unless it ignores the
// pause, its first tick in one sets bGamePaused and bPauseProcessing and goes
// no further, and the game going on clears both; bPauseProcessing alone
// stops its typing.
//
// From the menu map, 3 s in, three CapStartComputers made: A hidden in a
// CapStartParent, which takes ComputerStart; B shown on the root, 10 by 2
// cells of the InfoLink's font; C hidden on the root. At 3.5 s A shown and
// hidden again at once; at 4 s A's flags, then B's typing stopped by
// bPauseProcessing and "abc" printed; at 4.5 s where B has typed to, and its
// typing on again; at 5 s the game paused and C shown; at 5.5 s C's and B's
// pause flags, and the pause ended; at 6 s C's flags again and A shown again;
// at 6.5 s an exit. Each window's ComputerStart logs as it comes.
//=============================================================================
class ComputerStartConsole extends Console;

var float RunTime;
var int Step;
var CapStartParent Holder;
var CapStartComputer WinA, WinB, WinC;

function string Flags(CapStartComputer W)
{
	return "invoke " $ W.bInvokeComputerStart $ ", invoked " $ W.bComputerStartInvoked;
}

function string PauseFlags(CapStartComputer W)
{
	return "bGamePaused " $ W.bGamePaused $ ", bPauseProcessing " $ W.bPauseProcessing;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Color White;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		Holder = CapStartParent(Root.NewChild(class'CapStartParent'));
		Holder.SetPos(0, 0);
		Holder.SetSize(80, 30);
		WinA = CapStartComputer(Holder.NewChild(class'CapStartComputer', False));
		WinA.Label = "A";
		WinB = CapStartComputer(Root.NewChild(class'CapStartComputer'));
		WinB.Label = "B";
		WinB.SetPos(0, 40);
		White.R = 255;
		White.G = 255;
		White.B = 255;
		WinB.SetTextSize(10, 2);
		WinB.SetTextFont(Font'DeusExUI.FontFixedWidthSmall', 7, 10, White);
		WinC = CapStartComputer(Root.NewChild(class'CapStartComputer', False));
		WinC.Label = "C";
		WinC.SetPos(0, 70);
		Log("DXSTART: made A hidden in a parent, B shown, C hidden");
		Log("DXSTART: A: " $ Flags(WinA));
		break;
	case 1:
		if (RunTime < 3.5)
			return;
		WinA.Show();
		Log("DXSTART: A shown: " $ Flags(WinA));
		WinA.Hide();
		Log("DXSTART: A hidden again");
		break;
	case 2:
		if (RunTime < 4.0)
			return;
		Log("DXSTART: A: " $ Flags(WinA));
		WinB.bPauseProcessing = True;
		WinB.Print("abc", False);
		Log("DXSTART: B: processing paused, abc printed, column " $ WinB.textX);
		break;
	case 3:
		if (RunTime < 4.5)
			return;
		Log("DXSTART: B: column " $ WinB.textX);
		WinB.bPauseProcessing = False;
		Log("DXSTART: B: processing on");
		break;
	case 4:
		if (RunTime < 5.0)
			return;
		Log("DXSTART: B: column " $ WinB.textX);
		P.SetPause(True);
		WinC.Show();
		Log("DXSTART: paused " $ (P.Level.Pauser != "") $ ", C shown: " $ Flags(WinC));
		break;
	case 5:
		if (RunTime < 5.5)
			return;
		Log("DXSTART: C paused: " $ PauseFlags(WinC) $ ", " $ Flags(WinC));
		Log("DXSTART: B paused: " $ PauseFlags(WinB));
		P.SetPause(False);
		Log("DXSTART: unpaused " $ (P.Level.Pauser == ""));
		break;
	case 6:
		if (RunTime < 6.0)
			return;
		Log("DXSTART: C: " $ PauseFlags(WinC) $ ", " $ Flags(WinC));
		WinA.Show();
		Log("DXSTART: A shown again");
		break;
	case 7:
		if (RunTime < 6.5)
			return;
		Log("DXSTART: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
