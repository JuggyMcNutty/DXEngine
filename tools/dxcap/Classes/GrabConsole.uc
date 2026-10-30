//=============================================================================
// GrabConsole: the root window's grabs of the keyboard and the mouse, the
// counts a modal raises as it shows and lowers as it hides, logged in both
// engines (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the UI). On
// the menu map, 3 s in, a step every half second, each logging both counts
// and what the root's IsKeyboardGrabbed and IsMouseGrabbed answer: the
// stack cleared; the main menu invoked; a message box pushed over it, hidden,
// shown again; the menu under it hidden; the stack cleared; a modal made
// hidden, then shown and destroyed; two grabs of the keyboard by script and
// three ungrabs; then a grab of the mouse by script with no modal up, the
// pointer at 500, 380, and a shot; the mouse let go and a second shot. Each
// shot is marked for the original's grabber; each line is logged with
// "DXGRAB:" in front. (A modal in a plain window grabs as one under the root
// does, but destroying it stops the original on an assertion, so it is not
// made here.)
//=============================================================================
class GrabConsole extends Console;

var float RunTime;
var float NextStep;
var int Step;
var int MarkShot;
var bool bMarking;
var DeusExBaseWindow Menu;
var Window Box;
var Window Late;

function Note(DeusExRootWindow Root, string What)
{
	Log("DXGRAB: " $ What $ ": keyboard " $ Root.handleKeyboardRef $ ", mouse " $ Root.handleMouseRef
		$ ", grabbed " $ Root.IsKeyboardGrabbed() $ " and " $ Root.IsMouseGrabbed() $ " at " $ RunTime);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (RunTime < 3.0 || RunTime < NextStep || DeusExPlayer(P) == None)
		return;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;
	NextStep = RunTime + 0.5;

	switch (Step)
	{
	case 0:
		Note(Root, "the menu map's start");
		break;
	case 1:
		Root.ClearWindowStack();
		Note(Root, "the stack cleared");
		break;
	case 2:
		Root.InvokeMenu(class'MenuMain');
		Menu = Root.GetTopWindow();
		Note(Root, "the main menu invoked");
		break;
	case 3:
		Box = Root.MessageBox("Grab", "A second modal", 0, False, Root);
		Note(Root, "a message box over it");
		break;
	case 4:
		Box.Hide();
		Note(Root, "the box hidden");
		break;
	case 5:
		Box.Show();
		Note(Root, "the box shown again");
		break;
	case 6:
		Menu.Hide();
		Note(Root, "the menu under it hidden");
		break;
	case 7:
		Root.ClearWindowStack();
		Note(Root, "the stack cleared, the menu hidden");
		break;
	case 8:
		Late = Root.NewChild(class'ModalWindow', False);
		Note(Root, "a modal made hidden");
		break;
	case 9:
		Late.Show();
		Note(Root, "that modal shown");
		break;
	case 10:
		Late.Destroy();
		Note(Root, "that modal destroyed");
		break;
	case 11:
		Root.GrabKeyboardEvents();
		Root.GrabKeyboardEvents();
		Note(Root, "two grabs of the keyboard by script");
		break;
	case 12:
		Root.UngrabKeyboardEvents();
		Root.UngrabKeyboardEvents();
		Root.UngrabKeyboardEvents();
		Note(Root, "three ungrabs");
		break;
	case 13:
		Root.GrabMouseEvents();
		Root.ShowCursor(True);
		Root.SetCursorPos(500, 380);
		Note(Root, "a grab of the mouse by script, no modal up");
		MarkShot = 0;
		bMarking = true;
		break;
	case 14:
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		break;
	case 15:
		Root.UngrabMouseEvents();
		Note(Root, "the mouse let go");
		MarkShot = 1;
		bMarking = true;
		break;
	case 16:
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		break;
	default:
		Log("DXGRAB: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
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
