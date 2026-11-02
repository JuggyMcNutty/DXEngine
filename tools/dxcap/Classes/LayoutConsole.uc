//=============================================================================
// LayoutConsole: where a window sits as it and its parent are laid out, at
// once and on the next frame. From the menu map, the main menu shown, a
// window of the package's own that places its first two children itself
// (CapLayoutWindow) is put on it at 10, 10, 200 by 100, and in it a centred
// window and one at 5, 5. Logged ("DXLAYOUT:") as made and on the next
// frame; the second moved to 7, 7, which its parent places again, logged at
// once and on the next frame; then the holder's layout made to leave the
// first out (bLeaveOutFirst) and the holder made 210 wide, the first left
// to place itself by its alignment, logged at once, on the next frame and a
// second on. Then an exit.
//=============================================================================
class LayoutConsole extends Console;

var float RunTime;
var int Step;
var CapLayoutWindow Holder;
var Window First;
var Window Second;

function LogPlace(string Label, Window W)
{
	Log("DXLAYOUT: " $ Label $ ": at " $ W.x $ "," $ W.y $ " size " $ W.width $ "x" $ W.height);
}

function LogAll(string When)
{
	Log("DXLAYOUT: " $ When);
	LogPlace("  holder", Holder);
	LogPlace("  first", First);
	LogPlace("  second", Second);
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

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Holder = CapLayoutWindow(Root.GetTopWindow().NewChild(class'CapLayoutWindow'));
		Holder.SetPos(10, 10);
		Holder.SetSize(200, 100);
		First = Holder.NewChild(class'Window');
		First.SetWindowAlignments(HALIGN_Center, VALIGN_Center);
		Second = Holder.NewChild(class'Window');
		Second.SetPos(5, 5);
		LogAll("as made");
		break;
	case 2:
		LogAll("the next frame");
		Second.SetPos(7, 7);
		LogAll("the second moved to 7, 7");
		break;
	case 3:
		LogAll("the next frame");
		Holder.bLeaveOutFirst = True;
		Holder.SetSize(210, 100);
		LogAll("the holder 210 wide, its layout leaving the first out");
		break;
	case 4:
		LogAll("the next frame");
		break;
	case 5:
		if (RunTime < 5.0)
			return;
		LogAll("a second on");
		break;
	case 6:
		if (RunTime < 6.0)
			return;
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
