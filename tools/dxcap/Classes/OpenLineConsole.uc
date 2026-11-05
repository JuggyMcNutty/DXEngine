//=============================================================================
// OpenLineConsole: what the console's open takes of its line (bugs.md
// NET.14): the original's UGameEngine::Exec (Engine.dll 0x1038a030) takes
// the rest of the line, past the word and its blanks, as the URL, and an
// open with nothing after it sets no travel but sends PreTravel all the same
// (SetClientTravel 0x1038fa10). From the menu map, 2 s in: a CapPreTravel
// spawned, which logs the PreTravel it is sent, then "open" alone; 2 s later
// whether it is still in the same level, then "open DX.dx?Name=Two Words", a
// URL with a blank in it: the original's FURL refuses a blank in an option,
// its URL the default one, Index.dx, which fails to load, so it stays, where
// DXEngine's URL takes the blank and travels (bugs.md NET.13). In the level
// that travel reaches, its URL and the player's name as its login took it,
// then an exit -- or, still in the first level 10 s on, that it never went.
// Each line starts "DXOPEN:".
//=============================================================================
class OpenLineConsole extends Console;

var float RunTime, StepTime;
var int Step;
var LevelInfo FirstLevel;

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Name;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Name = "";
	if (P.PlayerReplicationInfo != None)
		Name = P.PlayerReplicationInfo.PlayerName;

	switch (Step)
	{
	case 0:
		if (RunTime < 2.0)
			return;
		FirstLevel = P.Level;
		P.Spawn(class'CapPreTravel');
		Log("DXOPEN: at " $ P.Level.GetLocalURL() $ " as '" $ Name $ "', open alone");
		P.ConsoleCommand("open");
		StepTime = RunTime;
		break;
	case 1:
		if (RunTime < StepTime + 2.0)
			return;
		Log("DXOPEN: after open alone, the same level " $ (P.Level == FirstLevel));
		Log("DXOPEN: open DX.dx?Name=Two Words");
		P.ConsoleCommand("open DX.dx?Name=Two Words");
		StepTime = RunTime;
		break;
	case 2:
		if (P.Level == FirstLevel)
		{
			if (RunTime < StepTime + 10.0)
				return;
			Log("DXOPEN: still in the first level 10 s on, at " $ P.Level.GetLocalURL() $ " as '" $ Name $ "'");
		}
		else
		{
			if (RunTime < StepTime + 2.0)
				return;
			Log("DXOPEN: travelled, at " $ P.Level.GetLocalURL() $ " as '" $ Name $ "'");
		}
		Log("DXOPEN: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
