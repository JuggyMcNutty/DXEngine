//=============================================================================
// AdminConsole: who is an admin (bugs.md NET.23), logged with "DXADMIN:" --
// the original's SpawnPlayActor makes a player an admin when its level has
// no net driver, or with -alladmin on the command line (Engine.dll
// 0x103971ad). From the menu map, 2 s in, the player's bAdmin and its
// replication info's, then "admin DxAdminMark", which only an admin's
// PlayerPawn.Admin runs: this console's DxAdminMark logs that it ran. 3 s in,
// a listen server on DXMP_Cathedral (CapDeathMatch), whose host logs in with
// a net driver; 3 s after it serves, the same lines; then an exit. Run
// without flags, then with DXCAP_FLAGS=-alladmin. The original's: in single
// player an admin, the command run; the host none, the command not run;
// with -alladmin an admin both times.
//=============================================================================
class AdminConsole extends Console;

var float RunTime, ServeTime;
var int Phase;

exec function DxAdminMark()
{
	Log("DXADMIN: the admin command ran, net mode " $ Viewport.Actor.Level.NetMode);
}

function Try(string Where)
{
	local PlayerPawn P;

	P = Viewport.Actor;
	Log("DXADMIN: " $ Where $ ": the player's bAdmin " $ P.bAdmin $ ", its replication info's " $ (P.PlayerReplicationInfo != None && P.PlayerReplicationInfo.bAdmin) $ ", net mode " $ P.Level.NetMode);
	Log("DXADMIN: " $ Where $ ": admin DxAdminMark");
	P.ConsoleCommand("admin DxAdminMark");
	Log("DXADMIN: " $ Where $ ": done");
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
		Try("single player");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXADMIN: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 2;
	}
	else if (Phase == 2 && P.Level.NetMode != NM_Standalone)
	{
		ServeTime += Delta;
		if (ServeTime > 3.0)
		{
			Try("listen server");
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		Log("DXADMIN: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}

	if (Phase < 9 && RunTime > 60.0)
	{
		Log("DXADMIN: out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
