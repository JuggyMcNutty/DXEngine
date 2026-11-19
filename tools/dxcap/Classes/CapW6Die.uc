//=============================================================================
// CapW6Die: NewChildDieConsole's window (bugs.md EXT.45). It destroys itself
// at the place its class's default Die names -- 1 in its InitWindow, 2 in
// its first VisibilityChanged(True), 3 in its first ConfigurationChanged --
// and logs each event it gets with "DXDIE:" in front.
//=============================================================================
class CapW6Die extends Window;

var int Die;
var int Configs;
var bool bGone;

function Kill(string Where)
{
	if (bGone)
		return;
	bGone = True;
	Log("DXDIE: " $ Die $ " destroys itself in " $ Where);
	Destroy();
	Log("DXDIE: " $ Die $ " back from its Destroy in " $ Where);
}

event InitWindow()
{
	Super.InitWindow();
	Log("DXDIE: " $ Die $ " InitWindow");
	if (Die == 1)
		Kill("InitWindow");
}

event VisibilityChanged(bool bNewVisibility)
{
	Log("DXDIE: " $ Die $ " VisibilityChanged " $ bNewVisibility);
	if (Die == 2 && bNewVisibility)
		Kill("VisibilityChanged");
}

event ConfigurationChanged()
{
	Configs++;
	Log("DXDIE: " $ Die $ " ConfigurationChanged #" $ Configs);
	if (Die == 3)
		Kill("ConfigurationChanged");
}

event DestroyWindow()
{
	Log("DXDIE: " $ Die $ " DestroyWindow");
}
