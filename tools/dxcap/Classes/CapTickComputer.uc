//=============================================================================
// CapTickComputer: TickOrderConsole's computer window, its script's Tick on, a
// timer due in its first tick and a CapTickChild under it. The window's own
// tick passes over its first and clears bFirstTick then
// (XComputerWindow::Tick, Extension.dll 0x1000f7b0); its script's Tick, the
// timer and the child each log, with "DXTICK:", whether bFirstTick is still
// set in that first tick. ComputerStart, which the window's own second tick
// sends, logs how many of its script's ticks have run by then.
//=============================================================================
class CapTickComputer extends ComputerWindow;

var CapTickChild Child;
var int Ticks;
var bool bStarted;

event InitWindow()
{
	Super.InitWindow();
	bTickEnabled = True;
	Child = CapTickChild(NewChild(class'CapTickChild'));
	Child.Watched = Self;
	Child.Label = "computer tick 1";
	Child.bArmed = True;
	AddTimer(0, False, 0, 'TimerProbe');
}

function string Report()
{
	return "bFirstTick " $ bFirstTick;
}

event Tick(float deltaSeconds)
{
	Ticks++;
	if (Ticks == 1)
		Log("DXTICK: computer tick 1: script, " $ Report());
}

function TimerProbe(int timerID, int invocations, int clientData)
{
	Log("DXTICK: computer tick 1: timer, " $ Report());
}

event bool ComputerStart()
{
	if (!bStarted)
		Log("DXTICK: computer ComputerStart after " $ Ticks $ " script ticks");
	bStarted = True;
	return False;
}
