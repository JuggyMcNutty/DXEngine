//=============================================================================
// NameFirstConsole: a name's first spelling where a map holds it for the
// editor alone (bugs.md VM.25): from the menu map, First opened (by
// default 02_NYC_BatteryPark, whose editor's brushes are grouped
// BArricade, a name for neither the client nor the server), 3 s there,
// then Second (06_HongKong_WanChai_Garage, whose DeusExMover1's Tag and
// LogicTrigger0's Event are Barricade), and 3 s in each actor whose Tag or
// Event is Probe in any case (barricade) logged with "DXNAMEFIRST:", its
// name, Tag and Event as string() gives them, then an exit. The console
// holds no such name itself. The original's linker makes no name of an
// editor-only entry (Core.dll 0x10160543), so the name's first spelling is
// Second's. Other pairs (DXCAP_CONFIG): "First=14_OceanLab_UC
// Second=02_NYC_BatteryPark Probe=walkway"; "First=14_OceanLab_Lab
// Second=09_NYC_Dockyard Probe=supplyroom". Compared: every line
// (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class NameFirstConsole extends Console;

var config string First, Second, Probe;
var float LevelTime, LastSeconds;
var string LastLevel;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (string(P.Level) != LastLevel || P.Level.TimeSeconds < LastSeconds)
		LevelTime = 0;
	LastLevel = string(P.Level);
	LastSeconds = P.Level.TimeSeconds;
	LevelTime += Delta;
	if (LevelTime < 3.0)
		return;
	LevelTime = -1000;
	if (Step == 0)
		P.ConsoleCommand("open " $ First);
	else if (Step == 1)
		P.ConsoleCommand("open " $ Second);
	else
	{
		foreach P.AllActors(class'Actor', A)
		{
			if (string(A.Tag) ~= Probe || string(A.Event) ~= Probe)
				Log("DXNAMEFIRST: " $ string(A.Name) $ " Tag " $ string(A.Tag) $ " Event " $ string(A.Event));
		}
		Log("DXNAMEFIRST: done on " $ P.Level.GetLocalURL() $ ", exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}

defaultproperties
{
	First="02_NYC_BatteryPark"
	Second="06_HongKong_WanChai_Garage"
	Probe="barricade"
}
