//=============================================================================
// CapReacher: a plain pawn of the package's own that the reach consoles ask
// (LiftReachConsole, JumpLandConsole, PathnodeConsole): hidden, with no AI
// of its own, the size of a man, a walker that cannot jump, swim or fly
// unless the console that spawns it says so.
//=============================================================================
class CapReacher extends Pawn;

defaultproperties
{
	bHidden=True
	bStasis=False
	CollisionRadius=20.000000
	CollisionHeight=40.000000
	BaseEyeHeight=32.000000
	EyeHeight=32.000000
	GroundSpeed=320.000000
	AirSpeed=320.000000
	WaterSpeed=200.000000
	JumpZ=300.000000
	MaxStepHeight=25.000000
	bCanWalk=True
	bCanJump=False
	bCanSwim=False
	bCanFly=False
	bCollideActors=True
	bCollideWorld=True
	bBlockActors=True
	bBlockPlayers=True
}
