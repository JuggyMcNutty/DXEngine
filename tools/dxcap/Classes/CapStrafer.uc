//=============================================================================
// CapStrafer: StrafeConsole's pawn, which strafes 200 units along X from
// where it is, looking along Y, 2 s after it is spawned; its MoveTimer,
// DesiredSpeed and physics logged with "DXSTRAFE:" at each of its first
// three ticks after the strafe starts, and the time the strafe took as it
// ends. StrafeConsole names each (Who) and gives one physics to land on.
//=============================================================================
class CapStrafer extends Pawn;

var string Who;
var int Logged;
var bool bStrafing;
var float Started;

auto state Probing
{
Begin:
	Sleep(2.0);
	Log("DXSTRAFE: " $ Who $ " strafing 200 along X, physics " $ Physics $ ", MaxDesiredSpeed " $ MaxDesiredSpeed $ " GroundSpeed " $ GroundSpeed);
	Started = Level.TimeSeconds;
	bStrafing = True;
	StrafeTo(Location + vect(200, 0, 0), Location + vect(0, 200, 0));
	Log("DXSTRAFE: " $ Who $ " strafe over after " $ (Level.TimeSeconds - Started) $ " s, physics " $ Physics);
	bStrafing = False;
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bStrafing && Logged < 3)
	{
		Logged++;
		Log("DXSTRAFE: " $ Who $ " tick " $ Logged $ ": MoveTimer " $ MoveTimer $ " DesiredSpeed " $ DesiredSpeed $ " physics " $ Physics $ " (delta " $ Delta $ ")");
	}
}

defaultproperties
{
	bHidden=True
	CollisionRadius=20.000000
	CollisionHeight=40.000000
	GroundSpeed=320.000000
	MaxDesiredSpeed=1.000000
}
