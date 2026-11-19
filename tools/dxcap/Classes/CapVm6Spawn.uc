//=============================================================================
// CapVm6Spawn: Vm6Console's spawn chain. Each one's PostBeginPlay, run under
// the Spawn native that made it, spawns the next while default.Left lasts:
// a script call chain through a native at every level.
//=============================================================================
class CapVm6Spawn extends Actor;

var int Left;
var int Reached;

event PostBeginPlay()
{
	local CapVm6Spawn Child;

	default.Reached = default.Reached + 1;
	if (default.Left > 0)
	{
		default.Left = default.Left - 1;
		Child = Spawn(class'CapVm6Spawn');
	}
}

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
