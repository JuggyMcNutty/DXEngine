//=============================================================================
// CapStartComputer: one of ComputerStartConsole's computer windows. Its
// ComputerStart logs, with "DXSTART:", which window it is, whether it is
// shown and whether it marked the game paused, and leaves it to the parents
// (False), as ComputerWindow's own does.
//=============================================================================
class CapStartComputer extends ComputerWindow;

var string Label;

event bool ComputerStart()
{
	Log("DXSTART: " $ Label $ " ComputerStart, shown " $ bIsVisible $ ", paused " $ bGamePaused);
	return False;
}
