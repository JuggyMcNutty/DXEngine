//=============================================================================
// CmdHelper: SaveWordsConsole's console command, run from an actor's tick
// as SaveHelper's save is -- the original ticks its console inside a draw,
// and a save paints its progress with a draw of its own, which the
// original's device stops on. Runs Command by its owner's ConsoleCommand
// once, logs what it gave, and expires.
//=============================================================================
class CmdHelper extends Info;

var string Command;

event Tick(float Delta)
{
	local PlayerPawn P;

	P = PlayerPawn(Owner);
	Disable('Tick');
	LifeSpan = 0.01;
	if (P != None)
		Log("DXSWORDS2: '" $ Command $ "' gave '" $ P.ConsoleCommand(Command) $ "' at " $ Level.TimeSeconds);
}
