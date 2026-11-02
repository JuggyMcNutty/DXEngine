//=============================================================================
// CapPrecisionProbe: PrecisionConsole's actor, whose own Tick, in the level's
// tick, works out 7 *= 3e9 once and logs it with "DXPRECISION:".
//=============================================================================
class CapPrecisionProbe extends Info;

var bool bDone;

event Tick(float DeltaTime)
{
	local int I;

	if (bDone)
		return;
	bDone = true;
	I = 7;
	I *= 3000000000.0;
	Log("DXPRECISION: in the level's tick, 7 *= 3e9 -> " $ I);
}
