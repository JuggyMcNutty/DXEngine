//=============================================================================
// LoadHelper: a save's load asked for from an actor's tick, as a level's
// scripts ask for a travel, where a console's own ask comes from inside a
// draw (UConsole::PreRender). It asks on its first tick, and the load takes
// the level it is in.
//=============================================================================
class LoadHelper extends Info;

var int Slot;

event Tick(float Delta)
{
	local DeusExPlayer P;

	P = DeusExPlayer(Owner);
	Disable('Tick');
	if (P != None)
	{
		Log("DXSAVE: loading slot " $ Slot $ " at " $ Level.TimeSeconds);
		P.LoadGame(Slot);
	}
}
