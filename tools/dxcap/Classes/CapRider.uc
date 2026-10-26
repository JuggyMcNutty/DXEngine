//=============================================================================
// CapRider: PathConsole's actor, which follows a path of InterpolationPoints
// and logs with "DXPATH:" each InterpolateEnd and, once armed, its next
// ticks: the tick's length, its place, rotation, alpha, target and
// velocity. It reaches the end of the path as a pawn does: the point with
// bEndOfPath stops it.
//=============================================================================
class CapRider extends Actor;

var int TicksLeft;

event Tick(float DeltaTime)
{
	if (TicksLeft > 0)
	{
		TicksLeft--;
		Log("DXPATH: a tick of " $ DeltaTime $ " s: at " $ Location $ ", rotation " $ Rotation $ ", alpha " $ PhysAlpha $ ", target " $ Target $ ", velocity " $ Velocity $ ", " $ GetEnum(enum'EPhysics', Physics));
	}
}

event InterpolateEnd(Actor Other)
{
	Log("DXPATH: InterpolateEnd " $ Other $ ", alpha " $ PhysAlpha $ ", at " $ Location $ ", rotation " $ Rotation);
	if (InterpolationPoint(Other) != None && InterpolationPoint(Other).bEndOfPath)
	{
		bInterpolating = False;
		SetPhysics(PHYS_None);
	}
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=False
	Physics=PHYS_None
}
