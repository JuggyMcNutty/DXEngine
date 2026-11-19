//=============================================================================
// CommandConsole: console commands given by DXCAP_CONFIG, each asked
// through the player's ConsoleCommand and logged with "DXCOMMAND:" in front
// with what it answers, in both engines. On the map the run starts on: at
// 3 s "Cmd1 Arg1", at 5 s "Cmd2 Arg2", at 7 s "Cmd3 Arg3" (a command with
// no word is passed over, an Arg may be left out), a shot at 9 s, and an
// exit at Seconds (10). DXCAP_CONFIG="Cmd1=lodbias Arg1=nan".
//=============================================================================
class CommandConsole extends Console;

var config string Cmd1, Arg1, Cmd2, Arg2, Cmd3, Arg3;
var config float Seconds;

var float RunTime;
var int Phase;

function Ask(PlayerPawn P, string Cmd, string Arg)
{
	local string Line;

	if (Cmd == "")
		return;
	Line = Cmd;
	if (Arg != "")
		Line = Line $ " " $ Arg;
	Log("DXCOMMAND: " $ Line);
	Log("DXCOMMAND: answers '" $ P.ConsoleCommand(Line) $ "'");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Limit;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Limit = Seconds;
	if (Limit <= 0.0)
		Limit = 10.0;
	if (Phase == 0 && RunTime > 3.0)
	{
		Ask(P, Cmd1, Arg1);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		Ask(P, Cmd2, Arg2);
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 7.0)
	{
		Ask(P, Cmd3, Arg3);
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 9.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > Limit)
	{
		Log("DXCOMMAND: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
