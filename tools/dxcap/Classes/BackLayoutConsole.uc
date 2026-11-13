//=============================================================================
// BackLayoutConsole (bugs.md EXT.37): a window's place as a background is set
// on it, at once and on the next frame. From the menu map, the main menu
// shown, a plain Window put on its top window, centred both ways
// (SetWindowAlignments), its size never set: logged as made and on the next
// frame. Then its background Texture'Solid', logged at once and on the next
// frame; the same texture again, logged at once; then None, logged at once
// and on the next frame. Then an exit. Each line starts "DXBACKLAYOUT:",
// the texture's size among them.
//
// The original's SetBackground lays the window out again when the
// background changes (XWindow::SetBackground, Extension.dll 0x1004d5e0, its
// AskParentForReconfigure): no parent of the menu's takes the request, so
// the window places itself by its alignments at its preferred size
// (ResizeChild 0x1004eab0), the background's on a side it gave no size of
// its own (QueryPreferredSize 0x1004e790) -- at once the texture's size,
// centred in the menu, the same texture again changing nothing, None
// keeping that size, its own now. DXEngine before a fix (bugs.md EXT.37)
// asked no layout: the window stayed 10 by 10 where it was. Compared: every
// line.
//=============================================================================
class BackLayoutConsole extends Console;

var float RunTime;
var int Step;
var Window Holder;
var Window Plain;

function LogPlace(string Label)
{
	Log("DXBACKLAYOUT: " $ Label $ ": at " $ Plain.x $ "," $ Plain.y $ " size " $ Plain.width $ "x" $ Plain.height
		$ " in " $ Holder.width $ "x" $ Holder.height);
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
		Holder = Root.GetTopWindow();
		Plain = Holder.NewChild(class'Window');
		Plain.SetWindowAlignments(HALIGN_Center, VALIGN_Center);
		Log("DXBACKLAYOUT: holder " $ Holder.Class $ ", Solid " $ Texture'Solid'.USize $ "x" $ Texture'Solid'.VSize);
		LogPlace("as made");
		break;
	case 2:
		LogPlace("the next frame");
		Plain.SetBackground(Texture'Solid');
		LogPlace("Solid set, at once");
		break;
	case 3:
		LogPlace("the next frame");
		Plain.SetBackground(Texture'Solid');
		LogPlace("Solid again, at once");
		Plain.SetBackground(None);
		LogPlace("None, at once");
		break;
	case 4:
		LogPlace("the next frame");
		break;
	case 5:
		if (RunTime < 6.0)
			return;
		Log("DXBACKLAYOUT: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
