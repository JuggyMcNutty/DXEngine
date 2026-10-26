//=============================================================================
// CapHopper: HopConsole's actor, which collides with the world alone, draws
// nothing, and logs with "DXHOP:" its landing's velocity and, once armed,
// the length of each of its next ticks.
//=============================================================================
class CapHopper extends Actor;

var int TicksLeft;

event Tick(float DeltaTime)
{
	if (TicksLeft > 0)
	{
		TicksLeft--;
		Log("DXHOP: a tick of " $ DeltaTime $ " s, physics " $ GetEnum(enum'EPhysics', Physics));
	}
}

event Landed(vector HitNormal)
{
	Log("DXHOP: landed with velocity " $ Velocity $ " at " $ Location $ ", normal " $ HitNormal);
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=True
	bBounce=False
	Mass=100
	CollisionRadius=10
	CollisionHeight=10
	Physics=PHYS_None
}
