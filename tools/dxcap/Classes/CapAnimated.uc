//=============================================================================
// CapAnimated: PassesConsole's actor, a Hong Kong market tarp whose Blowing
// sequence loops at the rate the console sets; each AnimEnd and each Tick
// logged with "DXPASS:" and the level's time, a Tick with its length and the
// AnimFrame then; its ticks counted for the console.
//=============================================================================
class CapAnimated extends HKMarketTarp;

var int Ends, Ticks;

event AnimEnd()
{
	Ends++;
	Log("DXPASS: animend " $ Ends $ " at " $ Level.TimeSeconds);
}

function Tick(float deltaTime)
{
	Super.Tick(deltaTime);
	Ticks++;
	Log("DXPASS: tick " $ deltaTime $ " at " $ Level.TimeSeconds $ " frame " $ AnimFrame $ " ends " $ Ends);
}

defaultproperties
{
	bRandomize=False
}
