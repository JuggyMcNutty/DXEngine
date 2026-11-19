//=============================================================================
// WindowChurnConsole: windows made and destroyed on one map (bugs.md ENG.57),
// logged with "DXWINCHURN:". On the map the run starts on, 3 s in: Count
// (3000 unless DXCAP_CONFIG gives another) plain windows made on the root
// and each destroyed at once, as the menus, barks and
// conversations make and destroy theirs; then how many windows AllObjects
// finds, before the windows are made, after, and at 5 s and 7 s; then an
// exit. The original deletes a window as it is destroyed unless its
// lockCount holds it (dx-reverse-info extension-dll.md, Destroy): its count
// stays. DXEngine frees one at the next collection: past its backlog of
// destroyed actors and windows the log's "Collecting garbage" comes, and
// the count at 5 s is what it was before.
//=============================================================================
class WindowChurnConsole extends Console;

var config int Count;
var float RunTime;
var int Phase;

function int Windows()
{
	local Window O;
	local int n;

	foreach AllObjects(class'Window', O)
		n++;
	return n;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Window W;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0 && RunTime > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root == None)
			return;
		Log("DXWINCHURN: before, " $ Windows() $ " windows");
		for (i = 0; i < Count; i++)
		{
			W = Root.NewChild(class'Window');
			W.Destroy();
		}
		W = None;
		Log("DXWINCHURN: " $ Count $ " made and destroyed, " $ Windows() $ " windows");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		Log("DXWINCHURN: at 5 s, " $ Windows() $ " windows");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 7.0)
	{
		Log("DXWINCHURN: at 7 s, " $ Windows() $ " windows");
		Phase = 3;
		P.ConsoleCommand("exit");
	}
}

defaultproperties
{
	Count=3000
}
