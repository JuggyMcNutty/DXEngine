//=============================================================================
// IniFlushConsole: settings saved in play, the run then killed before any
// exit (bugs.md ENG.27), logged with "DXFLUSH:". Run with DXCAP_KILL=1. From
// wherever the run starts, 3 s in: the player's Name changed in the user
// ini's [DefaultPlayer] by UpdateURL, which marks the file changed and
// saves nothing; then `set Engine CacheSizeMegs 7`, a SET, which ends with
// its class's SaveConfig; then the player's MouseSensitivity 3.75 saved
// (SaveConfig); then nothing until the run is killed. The original's
// SaveConfig ends with the config cache's Flush(0) (Core.dll 0x10151540),
// which writes every file marked changed (GOG DeusEx.exe 0x10915250,
// 0x10915760): after the kill the run's ini holds CacheSizeMegs=7 and its
// user ini MouseSensitivity=3.75 and, written by the SET's flush, the
// Name. Compared: the three keys in each run's ini-after.ini and
// userini-after.ini (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class IniFlushConsole extends Console;

var float RunTime;
var int Phase;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0 && RunTime > 3.0)
	{
		P.UpdateURL("Name", "DXFlushName", true);
		Log("DXFLUSH: Name set by UpdateURL");
		P.ConsoleCommand("set Engine CacheSizeMegs 7");
		Log("DXFLUSH: CacheSizeMegs set");
		P.MouseSensitivity = 3.75;
		P.SaveConfig();
		Log("DXFLUSH: MouseSensitivity saved; waiting for the kill");
		Phase = 1;
	}
}
