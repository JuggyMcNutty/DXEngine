//=============================================================================
// EditActorConsole: the original launcher's EDITACTOR (bugs.md WIN.38),
// from the menu map. From 2 s in, a second apart, each of Words by the
// player's ConsoleCommand, logged with "DXEDIT:" and what it gave: no
// CLASS=, a class not loaded ("Missing class"), a loaded class no actor of
// the level is ("Actor not found"), the player's own class and None (each
// a window of its own on the player, at no distance; nothing answered).
// Each second the player's DrawScale, Fatness, bHidden and AmbientGlow
// logged when they change, so an edit made in a window shows where it
// lands. The run held Seconds (60 by default), then an exit.
//=============================================================================
class EditActorConsole extends Console;

var config float Seconds;
var config string Words[8];
var string Props[4];
var string Last[4];
var float RunTime, NextLog, NextWord;
var int Word;
var bool bDone;

function LogProps()
{
	local int i;
	local string v;

	for (i = 0; i < 4; i++)
	{
		v = Viewport.Actor.GetPropertyText(Props[i]);
		if (v != Last[i] || RunTime < 1.5)
			Log("DXEDIT: " $ Props[i] $ " = '" $ v $ "' at " $ RunTime);
		Last[i] = v;
	}
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime >= NextLog)
	{
		LogProps();
		NextLog = RunTime + 1.0;
	}
	if (RunTime > 2.0 && Word < 8 && RunTime >= NextWord)
	{
		if (Words[Word] != "")
			Log("DXEDIT: '" $ Words[Word] $ "' gave '" $ Viewport.Actor.ConsoleCommand(Words[Word]) $ "' at " $ RunTime);
		Word++;
		NextWord = RunTime + 1.0;
	}
	if (!bDone && RunTime > 2.0 + Seconds)
	{
		Log("DXEDIT: exiting at " $ RunTime);
		bDone = true;
		Viewport.Actor.ConsoleCommand("exit");
	}
}

defaultproperties
{
	Seconds=60
	Words(0)="EDITACTOR"
	Words(1)="EDITACTOR CLASS=NoSuchClassAnywhere"
	Words(2)="EDITACTOR CLASS=Spectator"
	Words(3)="EDITACTOR CLASS=DeusExPlayer"
	Words(4)="EDITACTOR CLASS=None"
	Props(0)="DrawScale"
	Props(1)="Fatness"
	Props(2)="bHidden"
	Props(3)="AmbientGlow"
}
