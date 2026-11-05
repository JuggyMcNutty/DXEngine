//=============================================================================
// SpellingConsole: a name's spelling, logged with "DXSPELL:" from wherever
// the run starts, 2 s in (docs/DEVELOPMENT.md, scripted runs).
//
// Names made as the run goes, by the root window's StringToName, in another
// case than the packages' spelling of them: ANNADIES (DeusEx.u's Mission05
// spells it annadies), LOCATION (Core's and Engine's Location), and
// DxSpellOne, which no package holds, made again as DXSPELLONE: each one's
// text and whether it is the name the packages spell; then the flag
// COOLANTCUT set (Mission15's coolantcut) and the flag base's name for it,
// as the cheats' EditFlags window lists it; then an exit. The original's
// name table keeps the spelling a name was first made with (FName::FName,
// Core.dll 0x1014b950), and string(name) gives it (execNameToString
// 0x101320b0). What is compared between the engines: every line.
//=============================================================================
class SpellingConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXSPELL: " $ S);
}

function Probes(DeusExPlayer P)
{
	local Window Root;
	local name N;
	local int It;

	Root = P.rootWindow;
	if (Root == None)
	{
		Say("no root window");
		return;
	}
	N = Root.StringToName("ANNADIES");
	Say("ANNADIES as " $ string(N) $ ", the packages' annadies " $ (N == 'annadies'));
	N = Root.StringToName("LOCATION");
	Say("LOCATION as " $ string(N) $ ", the packages' Location " $ (N == 'Location'));
	N = Root.StringToName("DxSpellOne");
	Say("DxSpellOne as " $ string(N));
	N = Root.StringToName("DXSPELLONE");
	Say("DXSPELLONE as " $ string(N) $ ", DxSpellOne's " $ (N == Root.StringToName("DxSpellOne")));
	if (P.flagBase == None)
	{
		Say("no flag base");
		return;
	}
	P.flagBase.SetBool(Root.StringToName("COOLANTCUT"), True);
	It = P.flagBase.CreateIterator();
	while (P.flagBase.GetNextFlagName(It, N))
	{
		if (N == 'coolantcut')
			Say("the flag set as COOLANTCUT listed as " $ string(N));
	}
	P.flagBase.DestroyIterator(It);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes(DeusExPlayer(Viewport.Actor));
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
