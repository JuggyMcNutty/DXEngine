//=============================================================================
// CapViewportWindow: a viewport window of the package's own for
// CameraConsole. With bTurn set, its CalcView turns the frame a quarter
// turn to the left and logs, once, what it was given. With bProject set, its
// PostDrawWindow logs, once, where a point beside its watch actor lands
// (ConvertVectorToCoordinates) in it and in its child, if it has one: 33
// units to the watch actor's right and 13 up, a watch actor 200 units ahead
// of the eye along x. The original has a viewport window's frame only while
// the window draws, and stops at an assertion if asked at any other time.
// Each line is logged with "DXCAM:" in front.
//=============================================================================
class CapViewportWindow extends ViewportWindow;

var bool bTurn;
var bool bTurnLogged;
var bool bProject;
var bool bProjected;
var Window Child;

event CalcView(actor originActor, actor watchActor, out vector frameLocation, out rotator frameRotation)
{
	if (!bTurn)
		return;
	if (!bTurnLogged)
	{
		bTurnLogged = true;
		Log("DXCAM: calc view: origin " $ originActor $ " watch " $ watchActor $ " at " $ int(frameLocation.X) $ ","
			$ int(frameLocation.Y) $ "," $ int(frameLocation.Z) $ " turned " $ frameRotation.Pitch $ "," $ frameRotation.Yaw
			$ "," $ frameRotation.Roll);
	}
	frameRotation.Yaw += 16384;
}

event PostDrawWindow(GC gc)
{
	local vector Point;
	local float X, Y;
	local bool bFront;

	if (!bProject || bProjected || watchActor == None)
		return;
	bProjected = true;
	Point = watchActor.Location + vect(0,33,13);
	bFront = ConvertVectorToCoordinates(Point, X, Y);
	Log("DXCAM: " $ Name $ " projects the point at " $ int(X) $ "," $ int(Y) $ ", in front " $ bFront);
	if (Child != None)
	{
		bFront = Child.ConvertVectorToCoordinates(Point, X, Y);
		Log("DXCAM: its child, at " $ int(Child.x) $ "," $ int(Child.y) $ ", at " $ int(X) $ "," $ int(Y) $ ", in front " $ bFront);
	}
}
