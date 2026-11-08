//=============================================================================
// CapWallWalker (bugs.md PHYS.31): WallHitConsole's walker, a
// CapReacher that takes every wall hit (MinHitWall far over any dot, as
// ScriptedPawn's 1e13) and logs each HitWall with the tick it came in,
// "DXWALLHIT:".
//=============================================================================
class CapWallWalker extends CapReacher;

var int Ticks;
var bool bWatching;

event Tick(float DeltaTime)
{
	if (bWatching)
		Ticks++;
}

event HitWall(vector HitNormal, actor Wall)
{
	if (bWatching)
		Log("DXWALLHIT: tick " $ Ticks $ " HitWall " $ Wall $ " normal " $ HitNormal $ " at " $ Location);
}

defaultproperties
{
	MinHitWall=10000000000000.0
}
