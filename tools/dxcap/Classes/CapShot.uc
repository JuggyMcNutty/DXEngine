//=============================================================================
// CapShot: ShotConsole's projectile, which bounces off what it hits at 0.6
// of its speed and logs with "DXSHOT:" each wall hit and, once armed, its
// next ticks' lengths and its place and velocity after each. It collides
// with the world alone, draws nothing and touches nothing.
//=============================================================================
class CapShot extends Projectile;

var int TicksLeft;

event Tick(float DeltaTime)
{
	if (TicksLeft > 0)
	{
		TicksLeft--;
		Log("DXSHOT: a tick of " $ DeltaTime $ " s: at " $ Location $ ", velocity " $ Velocity $ ", physics " $ GetEnum(enum'EPhysics', Physics));
	}
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	Log("DXSHOT: HitWall at " $ Location $ ", normal " $ HitNormal $ ", " $ Wall.Name $ ", velocity " $ Velocity);
	Velocity = 0.6 * (Velocity - 2 * HitNormal * (Velocity dot HitNormal));
}

simulated function ProcessTouch(Actor Other, vector HitLocation)
{
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
}

defaultproperties
{
	Speed=400
	MaxSpeed=2000
	bBounce=True
	bHidden=True
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=True
	CollisionRadius=4
	CollisionHeight=4
	LifeSpan=0
	Physics=PHYS_None
}
