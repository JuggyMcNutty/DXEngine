//=============================================================================
// LocalizeConsole: where a package's text and the objects a .int file
// registers come from, logged with "DXLOC:" from the menu map, then an exit.
// The run's paths name two folders of .int files of the harness's own first
// (tools/dxcap.sh writes them), as a mod's ini names its folders: the
// first by its absolute path, the second by one relative to the game's
// System folder. Logged: Localize of a key the game's DeusEx.int and the
// first folder's both give, of one only the game's gives, of a package
// whose .int is only in either folder, and of one with no .int at all, and
// by their characters' codes of a UTF-16 .int's and a Latin-1 one's; this
// class's localized Greeting, which only the first folder's DXCapture.int
// gives; the objects the folders' .int files register for this class, and
// the render devices the game's register (docs/DEVELOPMENT.md, scripted
// runs).
//=============================================================================
class LocalizeConsole extends Console;

var float RunTime;
var bool bDone;
var localized string Greeting;

function LogLocalized(string Section, string Key, string Package)
{
	Log("DXLOC: " $ Package $ "." $ Section $ "." $ Key $ " '" $ Localize(Section, Key, Package) $ "'");
}

function LogRegistry(string MetaClass)
{
	local string Entry, Description;
	local int i;

	for (i = 0; i < 32; i++)
	{
		Entry = "";
		Viewport.Actor.GetNextIntDesc(MetaClass, i, Entry, Description);
		if (Entry == "")
			break;
		Log("DXLOC: " $ MetaClass $ " " $ i $ " " $ Entry $ " '" $ Description $ "'");
	}
	Log("DXLOC: " $ MetaClass $ ", " $ i $ " registered");
}

// A key's text by its characters' codes, the log being 8-bit: a UTF-16 .int
// and a Latin-1 one
function LogCodes(string Section, string Key, string Package)
{
	local string T, R;
	local int i;

	T = Localize(Section, Key, Package);
	for (i = 0; i < Len(T); i++)
		R = R $ " " $ Asc(Mid(T, i, 1));
	Log("DXLOC: " $ Package $ "." $ Section $ "." $ Key $ " codes" $ R);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	LogLocalized("DeusExPlayer", "TooHeavyToLift", "DeusEx");
	LogLocalized("DeusExPlayer", "CannotLift", "DeusEx");
	LogLocalized("Strings", "Hello", "DXCapFirst");
	LogLocalized("Strings", "Hello", "DXCapSecond");
	LogLocalized("Strings", "Hello", "DXCapNone");
	LogCodes("Strings", "Hello", "DXCapWide");
	LogCodes("Strings", "Hello", "DXCapLatin");
	Log("DXLOC: Greeting '" $ Greeting $ "'");
	LogRegistry("DXCapture.LocalizeConsole");
	LogRegistry("Engine.RenderDevice");
	Log("DXLOC: done");
	Viewport.Actor.ConsoleCommand("exit");
}

defaultproperties
{
	Greeting="the class's own"
}
