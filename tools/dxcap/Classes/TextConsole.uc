//=============================================================================
// TextConsole: a property's text in and out -- SetPropertyText and
// GetPropertyText, the original's ImportText and ExportText (Core.dll;
// docs/DEVELOPMENT.md, scripted runs) -- logged with "DXTEXT:" from wherever
// the run starts, 2 s in: the player's Location read; a vector of the
// console's set whole and by one member; a Plane, which extends Vector; a
// struct of two strings and a static array set with a quoted string holding
// a comma, a bare one and one element by index, read back, then set by one
// member alone; a dynamic array of ints; a class by its path, by Class'..',
// by its bare name and one outside the metaclass; a texture by Type'Path',
// by Package.Name and by its bare name; a bool from on, 1 and False; an int
// from junk and from a number with a tail. Then an exit.
//=============================================================================
class TextConsole extends Console;

struct Pair
{
	var string Text;
	var string Key;
	var int N[2];
};

var vector V;
var plane Pl;
var Pair Entry;
var array<int> Numbers;
var class<Actor> Kind;
var Texture Pic;
var bool B;
var int I;
var float RunTime;
var int Phase;

function Probe(PlayerPawn P)
{
	local string Q;

	Q = Chr(34);
	Log("DXTEXT: the player's Location: '" $ P.GetPropertyText("Location") $ "'");
	SetPropertyText("V", "(X=1,Y=2.5,Z=-3)");
	Log("DXTEXT: V from (X=1,Y=2.5,Z=-3): '" $ GetPropertyText("V") $ "'");
	SetPropertyText("V", "(Z=9)");
	Log("DXTEXT: V from (Z=9): '" $ GetPropertyText("V") $ "'");
	SetPropertyText("Pl", "(X=1,Y=2,Z=3,W=4)");
	Log("DXTEXT: Pl, a Plane, from (X=1,Y=2,Z=3,W=4): '" $ GetPropertyText("Pl") $ "'");
	SetPropertyText("Entry", "(Text=" $ Q $ "Yes, load" $ Q $ ",Key=LOAD,N[1]=7)");
	Log("DXTEXT: Entry from a quoted Yes, load; Key LOAD; N[1] 7: '" $ GetPropertyText("Entry") $ "'");
	SetPropertyText("Entry", "(Key=NEW)");
	Log("DXTEXT: Entry from (Key=NEW): '" $ GetPropertyText("Entry") $ "'");
	SetPropertyText("Numbers", "(1,2,3)");
	Log("DXTEXT: Numbers from (1,2,3): '" $ GetPropertyText("Numbers") $ "'");
	SetPropertyText("Kind", "Engine.Light");
	Log("DXTEXT: Kind from Engine.Light: '" $ GetPropertyText("Kind") $ "'");
	SetPropertyText("Kind", "Class'Engine.Pawn'");
	Log("DXTEXT: Kind from Class'Engine.Pawn': '" $ GetPropertyText("Kind") $ "'");
	SetPropertyText("Kind", "Mover");
	Log("DXTEXT: Kind from Mover: '" $ GetPropertyText("Kind") $ "'");
	SetPropertyText("Kind", "Engine.Texture");
	Log("DXTEXT: Kind from Engine.Texture, no Actor: '" $ GetPropertyText("Kind") $ "'");
	SetPropertyText("Pic", "Texture'Engine.S_Actor'");
	Log("DXTEXT: Pic from Texture'Engine.S_Actor': '" $ GetPropertyText("Pic") $ "'");
	SetPropertyText("Pic", "None");
	SetPropertyText("Pic", "Engine.S_Light");
	Log("DXTEXT: Pic from Engine.S_Light: '" $ GetPropertyText("Pic") $ "'");
	SetPropertyText("Pic", "None");
	SetPropertyText("Pic", "S_Actor");
	Log("DXTEXT: Pic from the bare S_Actor: '" $ GetPropertyText("Pic") $ "'");
	SetPropertyText("B", "on");
	Log("DXTEXT: B from on: " $ B);
	SetPropertyText("B", "1");
	Log("DXTEXT: B from 1: " $ B);
	SetPropertyText("B", "False");
	Log("DXTEXT: B from False: " $ B);
	I = 5;
	SetPropertyText("I", "abc");
	Log("DXTEXT: I from abc: " $ I);
	SetPropertyText("I", "12x");
	Log("DXTEXT: I from 12x: " $ I);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		Probe(P);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXTEXT: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
