//=============================================================================
// CapSleeper: an actor whose state code sleeps Seconds over and over, each
// waking logged with the level's time and its LatentFloat (SleepConsole).
//=============================================================================
class CapSleeper extends Info;

var float Seconds;
var int Wakes;

state Sleeping
{
Begin:
	Sleep(Seconds);
	Wakes++;
	Log("DXSLEEP: " $ Name $ " woke at " $ Level.TimeSeconds $ ", LatentFloat " $ LatentFloat);
	goto 'Begin';
}
