//=============================================================================
// CapTickChild: a window of the package's own under one of TickOrderConsole's
// probes (CapTickComputer, CapTickButton, CapTickLog), its script's Tick on:
// at its first tick once armed it logs, with "DXTICK:", what its parent's own
// tick has done by then, which the parent's children tick after.
//=============================================================================
class CapTickChild extends Window;

var Window Watched;
var string Label;
var bool bArmed;
var int ArmedTicks;

event InitWindow()
{
	Super.InitWindow();
	bTickEnabled = True;
}

function string Report()
{
	if (CapTickComputer(Watched) != None)
		return CapTickComputer(Watched).Report();
	if (CapTickButton(Watched) != None)
		return CapTickButton(Watched).Report();
	if (CapTickLog(Watched) != None)
		return CapTickLog(Watched).Report();
	return "no probe";
}

event Tick(float deltaSeconds)
{
	if (!bArmed)
		return;
	ArmedTicks++;
	if (ArmedTicks == 1)
		Log("DXTICK: " $ Label $ ": child, " $ Report());
}
