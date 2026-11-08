//=============================================================================
// FixedMapConsole (bugs.md VM.31, VM.32): a fixed array's and a map's text in
// and out, which no stock package can show -- none of the game's, Revision's
// or the mods' packages declares a FixedArrayProperty or a MapProperty --
// logged with "DXFIXMAP:" from the menu map, 2 s in, then an exit.
//
// UCC compiles Fixed as a static int array and Mapped as an int; fixedmap.py
// then retypes them in DXCapture.u: Fixed a FixedArrayProperty of three ints
// (ArrayDim 1, Inner an IntProperty, Count 3), Mapped a MapProperty of int to
// int. This class's code names neither but by its text, so its bytecode holds
// no reference to them.
//
// The original (Core.dll): GetPropertyText exports by the property's
// ExportText (0x1013e0b0 -> 0x10165440), a fixed array as "(a,b,c)"
// (0x10167f50) and a map as nothing (0x10168f20); SetPropertyText imports by
// ImportText (0x1013e320), a fixed array's zeroing the whole array, then
// taking Count elements each followed by one character, then ')' (0x10168050):
// "(4,5,6)" sets 4, 5, 6 and answers failure, which nobody reads; "(1,2)"
// sets 1, 2 and a zero; a map's takes nothing (0x10168f30). SET imports into
// each object and the defaults, then SaveConfig writes Fixed=(7,8,9) and
// removes the section's Mapped lines (0x10151540).
// DXEngine: "fixed array" and "map" (PrintValue), nothing imported, and
// SaveConfig writing Fixed=fixed array and Mapped=map.
//=============================================================================
class FixedMapConsole extends Console;

var config int Fixed[3];
var config int Mapped;
var float RunTime;
var int Phase;

function Show(PlayerPawn P, string Step)
{
	Log("DXFIXMAP: " $ Step $ ": Fixed '" $ GetPropertyText("Fixed") $ "', Mapped '" $ GetPropertyText("Mapped") $ "'");
}

function Probe(PlayerPawn P)
{
	Show(P, "as loaded");
	SetPropertyText("Fixed", "(4,5,6)");
	Show(P, "Fixed from (4,5,6)");
	SetPropertyText("Fixed", "(7,8,9,)");
	Show(P, "Fixed from (7,8,9,)");
	SetPropertyText("Fixed", "(1,2)");
	Show(P, "Fixed from (1,2)");
	SetPropertyText("Fixed", "1,2,3");
	Show(P, "Fixed from 1,2,3");
	SetPropertyText("Mapped", "(1,2)");
	Show(P, "Mapped from (1,2)");
	Log("DXFIXMAP: GET Fixed '" $ P.ConsoleCommand("get FixedMapConsole Fixed") $ "', GET Mapped '" $ P.ConsoleCommand("get FixedMapConsole Mapped") $ "'");
	// SET: every object of the class and its defaults, then the class's
	// config saved -- the game ini's [DXCapture.FixedMapConsole] section
	P.ConsoleCommand("set FixedMapConsole Fixed (3,2,1)");
	Show(P, "after SET Fixed (3,2,1)");
	Log("DXFIXMAP: GET Fixed after SET '" $ P.ConsoleCommand("get FixedMapConsole Fixed") $ "'");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Probe(P);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXFIXMAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
