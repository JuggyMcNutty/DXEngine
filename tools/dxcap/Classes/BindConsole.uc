//=============================================================================
// BindConsole: what a key's binding does with the input's words and the
// commands it names (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, a
// key's binding). On Liberty Island, every quarter second from 3 s, the
// player's bExtra0, bExtra1 and bExtra2 are logged with "DXBIND:" in front.
// Its exec functions Mark, Shot and ShowPath log their call: the console's
// Shot beside the viewport's SHOT, its ShowPath beside the player's. With
// tools/dxcap/timelines/bind-keys.txt and a User.ini binding F to Toggle
// bExtra0, G to Pulse bExtra1, H to Button bExtra2|Mark 1|OnRelease Mark 2,
// J to Mark 3, K to Shot and L to ShowPath, DXEngine's keys press each (the
// original takes no scripted key). In both engines, the console's own
// TimeDemo by command: timedemo 1 at 4 s, timedemo 0 at 7 s, whose result
// the console's script logs. Then an exit at 20 s.
//=============================================================================
class BindConsole extends Console;

var float MapTime;
var float NextLog;
var string CurrentMap;
var int Phase;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

exec function Mark(int N)
{
	Log("DXBIND: mark " $ N $ " at " $ MapTime);
}

exec function Shot()
{
	Log("DXBIND: the console's Shot at " $ MapTime);
}

exec function ShowPath()
{
	Log("DXBIND: the console's ShowPath at " $ MapTime);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		NextLog = MapTime;
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (MapTime >= NextLog)
	{
		Log("DXBIND: at " $ NextLog $ " bExtra0 " $ P.bExtra0 $ ", bExtra1 " $ P.bExtra1 $ ", bExtra2 " $ P.bExtra2);
		NextLog += 0.25;
	}
	if (Phase == 1 && MapTime > 4.0)
	{
		Log("DXBIND: timedemo 1");
		P.ConsoleCommand("timedemo 1");
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 7.0)
	{
		Log("DXBIND: timedemo 0");
		P.ConsoleCommand("timedemo 0");
		Phase = 3;
	}
	else if (Phase == 3 && MapTime > 20.0)
	{
		Log("DXBIND: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
