//=============================================================================
// CapPainPawn: a pawn of the package's own, hidden, with no collision or
// physics, that logs its pain and speech timers' firing against its own
// ticks. PainConsole sets the timers.
//=============================================================================
class CapPainPawn extends Pawn;

var bool bWatching;
var int Ticks;

event Tick(float DeltaTime)
{
	if (!bWatching)
		return;
	Ticks++;
	if (Ticks <= 4)
		Log("DXPAIN: tick " $ Ticks $ ": pain time over 0 " $ (PainTime > 0) $ ", speech time over 0 " $ (SpeechTime > 0)
			$ ", a step of " $ int(DeltaTime * 10000 + 0.5) $ " ten-thousandths of a second");
}

event PainTimer()
{
	Log("DXPAIN: PainTimer in tick " $ Ticks);
}

event SpeechTimer()
{
	Log("DXPAIN: SpeechTimer in tick " $ Ticks);
}

defaultproperties
{
	bHidden=True
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
