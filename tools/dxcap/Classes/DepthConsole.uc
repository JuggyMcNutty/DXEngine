//=============================================================================
// DepthConsole: an expression 100 operators deep, past the 64 a long
// expression reaches, which the package's load reads as the original's does
// (its serializer and VM take any depth; DXEngine's up to 1,024). The sum of
// a hundred ones, 100, logged with "DXDEPTH:" from the menu map, then an
// exit (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class DepthConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local int a, n;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	a = 1;
	n = a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a
		+ a + a + a + a + a + a + a + a + a + a;
	Log("DXDEPTH: sum " $ n);
	bDone = true;
	Viewport.Actor.ConsoleCommand("exit");
}
