//=============================================================================
// PainConsole: a pawn's pain and speech timers, the same in both engines
// (docs/DEVELOPMENT.md, scripted runs). At Liberty Island, 6 s in (the menu
// map's fly-by sets the level's time dilation every tick), each held to 60
// frames a second (DXCAP_FPS=60): a CapPainPawn 100 units ahead of the
// player and the level's time slowed to a half, so a tick is some 0.0083 s;
// a frame on, its PainTime set to 0.0088 s and its SpeechTime to 0.0085 s,
// each due to be left under a thousandth of a second after its next tick;
// its ticks and the timers' firing logged ("DXPAIN:"); then the level's time
// back and an exit.
//=============================================================================
class PainConsole extends Console;

var float RunTime;
var float MapTime;
var int Step;
var int Frames;
var CapPainPawn Watched;

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector X, Y, Z;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	MapTime += Delta;

	// The original starts in its menu map.
	if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
	{
		if (Step == 0 && RunTime > 3.0)
		{
			Log("DXPAIN: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			RunTime = -1000.0;
		}
		MapTime = 0;
		return;
	}

	if (Step == 0 && MapTime > 6.0)
	{
		GetAxes(P.Rotation, X, Y, Z);
		Watched = P.Spawn(class'CapPainPawn',,, P.Location + X * 100);
		if (Watched == None)
		{
			Log("DXPAIN: no room for the pawn");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		P.Level.TimeDilation = 0.5;
		Step = 1;
	}
	else if (Step == 1)
	{
		Watched.PainTime = 0.0088;
		Watched.SpeechTime = 0.0085;
		Watched.bWatching = True;
		Log("DXPAIN: " $ Watched.Name $ ", time dilation " $ P.Level.TimeDilation $ ", pain time " $ Watched.PainTime
			$ ", speech time " $ Watched.SpeechTime);
		Step = 2;
	}
	else if (Step == 2 && ++Frames >= 60)
	{
		P.Level.TimeDilation = 1.0;
		Log("DXPAIN: done after " $ Watched.Ticks $ " ticks, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
	}
}
