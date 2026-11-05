//=============================================================================
// CapTickLog: TickOrderConsole's text log, its script's Tick on and a
// CapTickChild under it. Armed, from the console's tick, outside the
// windows': a timer due in the next tick, then a line with no time out, which
// the log's own next tick drops (XTextLogWindow::Tick, Extension.dll
// 0x10047c30). Its script's first tick after, the timer and the child log,
// with "DXTICK:", whether the line is still there.
//=============================================================================
class CapTickLog extends TextLogWindow;

var CapTickChild Child;
var bool bArmed;
var int ArmedTicks;

event InitWindow()
{
	Super.InitWindow();
	bTickEnabled = True;
	Child = CapTickChild(NewChild(class'CapTickChild'));
	Child.Watched = Self;
	Child.Label = "log tick 1 after the line";
}

function string Report()
{
	return "the line there " $ (GetText() != "");
}

function Arm()
{
	local Color C;

	C.R = 255;
	C.G = 255;
	C.B = 255;
	AddTimer(0, False, 0, 'TimerProbe');
	SetTextTimeout(0);
	AddLog("CapTickLog's line", C);
	bArmed = True;
	Child.bArmed = True;
	Log("DXTICK: log armed: " $ Report());
}

event Tick(float deltaSeconds)
{
	if (!bArmed)
		return;
	ArmedTicks++;
	if (ArmedTicks == 1)
		Log("DXTICK: log tick 1 after the line: script, " $ Report());
}

function TimerProbe(int timerID, int invocations, int clientData)
{
	Log("DXTICK: log tick 1 after the line: timer, " $ Report());
}
