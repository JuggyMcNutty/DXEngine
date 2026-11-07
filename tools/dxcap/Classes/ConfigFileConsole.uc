//=============================================================================
// ConfigFileConsole: which file a config section is read from and written
// to: from the menu map, the player's SaveConfig, then an exit. The
// original's SaveConfig writes every key into its object's class's config
// file, a base class's globalconfig one under the base's section (Core.dll
// 0x10151540), and its LoadConfig reads them so (0x10150d60): the player's
// class is config(User) and Pawn, which declares bNeverSwitchOnPickup, the
// system ini's, so [Engine.Pawn] goes into the run's user ini. Logged:
// "DXCONFIGFILE: saved". Compared: the run's two inis after it, which of
// them has [Engine.Pawn] (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class ConfigFileConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Viewport.Actor.SaveConfig();
	Log("DXCONFIGFILE: saved, bNeverSwitchOnPickup " $ Viewport.Actor.bNeverSwitchOnPickup);
	Viewport.Actor.ConsoleCommand("exit");
}
