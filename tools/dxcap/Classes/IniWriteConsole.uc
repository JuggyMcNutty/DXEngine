//=============================================================================
// IniWriteConsole: a setting changed and the inis written back, logged with
// "DXINIW:" (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: `set Engine CacheSizeMegs 5`, which
// saves the Engine class's config into the run's system ini, then an exit,
// which writes the inis back. For a hand run on a copy of an ini with a key
// above its first section header and a section given twice: the file after
// each of two runs, its lines and the keys' values, stays as it was (the
// original's config reader drops the stray key and keeps one section).
//=============================================================================
class IniWriteConsole extends Console;

var float RunTime;
var int Phase;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Log("DXINIW: set Engine CacheSizeMegs 5");
		Viewport.Actor.ConsoleCommand("set Engine CacheSizeMegs 5");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXINIW: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
