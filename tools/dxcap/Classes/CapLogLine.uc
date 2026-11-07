//=============================================================================
// CapLogLine: LogTimeoutConsole's text log, its script's Tick on. Armed, its
// first tick logs, with "DXLOGTIME:", whether the line is still there, which
// the log's own tick, before the script's, has counted down by then
// (XTextLogWindow::Tick, Extension.dll 0x10047c30).
//=============================================================================
class CapLogLine extends TextLogWindow;

var bool bArmed;
var int ArmedTicks;

event InitWindow()
{
	Super.InitWindow();
	bTickEnabled = True;
}

function string Report()
{
	return "the line there " $ (GetText() != "");
}

event Tick(float deltaSeconds)
{
	if (!bArmed)
		return;
	ArmedTicks++;
	if (ArmedTicks == 1)
		Log("DXLOGTIME: the log's first tick after the line: " $ Report());
}
