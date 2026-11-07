//=============================================================================
// CapTouchProbe: PendingTouchConsole's actors (bugs.md NET.25) -- an actor
// of the package's own that collides with nothing, draws nothing and stays
// out of stasis, with no physics until the console gives it some. As the
// toucher it counts each PostTouch it is sent, and logs it with
// "DXPTOUCH:", with the sender's tag and physics.
//=============================================================================
class CapTouchProbe extends Actor;

var int PostTouches;

event PostTouch(Actor Other)
{
	PostTouches++;
	Log("DXPTOUCH: PostTouch from " $ Other.Tag $ ", its physics " $ Other.Physics);
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
