//=============================================================================
// ExecFileConsole: the console's exec, its file the first token past the
// word, by ParseToken into 64 characters (Engine.dll 0x10374241; bugs.md
// ENG.21): from the menu map, exec of a quoted name with a blank in it, of
// that name unquoted, of a name of 70 characters, and with none and with an
// empty quoted one, each marked "DXEXECF: <command>" before it is run, then
// an exit. In the game's System folder beforehand: "ENG21.txt b.txt",
// "ENG21.txt" and the long name's first 63 characters, ENG21_, 53 x's and
// .txt, each holding the line getping. Each file run logs "Execing <file>",
// in both engines.
//=============================================================================
class ExecFileConsole extends Console;

var float RunTime;
var bool bDone;

function Run(string Command)
{
	Log("DXEXECF: " $ Command);
	Viewport.Actor.ConsoleCommand(Command);
}

event Tick(float Delta)
{
	local string Long;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Long = "ENG21_";
	for (i = 0; i < 53; i++)
		Long = Long $ "x";
	Long = Long $ ".txtzzzzzzz";
	Run("exec \"ENG21.txt b.txt\"");
	Run("exec ENG21.txt b.txt");
	Run("exec " $ Long);
	Run("exec");
	Run("exec \"\"");
	Log("DXEXECF: done");
	Viewport.Actor.ConsoleCommand("exit");
}
