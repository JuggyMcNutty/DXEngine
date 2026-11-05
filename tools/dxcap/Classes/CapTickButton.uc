//=============================================================================
// CapTickButton: TickOrderConsole's button, its script's Tick on and a
// CapTickChild under it. Armed, from the console's tick, outside the
// windows': a timer due in the next tick, then a press with no activate delay
// (PressButton), whose show ends at a timer of the button's own
// (XButtonWindow::Timer, Extension.dll 0x100080e0), then another timer. Its
// script's first two ticks after, both timers and the child log, with
// "DXTICK:", whether it still shows pressed. Its activations stop at it.
//=============================================================================
class CapTickButton extends ButtonWindow;

var CapTickChild Child;
var bool bArmed;
var int ArmedTicks;

event InitWindow()
{
	Super.InitWindow();
	bTickEnabled = True;
	Child = CapTickChild(NewChild(class'CapTickChild'));
	Child.Watched = Self;
	Child.Label = "button tick 1 after the press";
}

function string Report()
{
	return "pressed " $ bButtonPressed;
}

function Arm()
{
	AddTimer(0, False, 0, 'TimerBefore');
	SetActivateDelay(0);
	PressButton();
	AddTimer(0, False, 0, 'TimerAfter');
	bArmed = True;
	Child.bArmed = True;
	Log("DXTICK: button armed: " $ Report());
}

event Tick(float deltaSeconds)
{
	if (!bArmed)
		return;
	ArmedTicks++;
	if (ArmedTicks <= 2)
		Log("DXTICK: button tick " $ ArmedTicks $ " after the press: script, " $ Report());
}

function TimerBefore(int timerID, int invocations, int clientData)
{
	Log("DXTICK: button tick 1 after the press: timer added before the press, " $ Report());
}

function TimerAfter(int timerID, int invocations, int clientData)
{
	Log("DXTICK: button tick 1 after the press: timer added after the press, " $ Report());
}

event bool ButtonActivated(Window button)
{
	return True;
}
