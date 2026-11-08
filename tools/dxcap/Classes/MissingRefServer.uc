//=============================================================================
// MissingRefServer: one of a dedicated server's ServerActors, behind
// tools/dxcap/netrelay.py (DXCAP_PORT=7792 tools/dxcap.sh dedicated <engine>
// MissingRefServer 'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 60;
// netrelay.py --log <file>; any join console the client, JoinConsole).
// bugs.md NET.31. 3 s after a player is in it spawns a CapMissingRef whose
// Texture is Moon.Moonscape7, loaded here, a package the map has not, and
// logs it; it changes nothing more and exits 10 s after. Read with
// netrelay.py's log: the actor channel opened after the "placed" line, its
// bunches each second -- one at its open in the original's, where a
// DXEngine server that sends the reference again each update sends one
// each tick. Each line starts "DXMISSING:".
//=============================================================================
class MissingRefServer extends Info;

var float InTime, PlacedAt;
var int Step;
var CapMissingRef Placed;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;

	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None && Placed == None)
		return;
	InTime += Delta;
	if (Step == 0 && InTime > 3.0)
	{
		Placed = Spawn(class'CapMissingRef',,, Joined.Location + vect(0,0,200));
		Placed.Texture = Texture(DynamicLoadObject("Moon.Moonscape7", class'Texture'));
		PlacedAt = InTime;
		Log("DXMISSING: server placed " $ Placed $ " with Texture " $ Placed.Texture);
		Step = 1;
	}
	else if (Step == 1 && InTime - PlacedAt > 10.0)
	{
		Log("DXMISSING: server exiting");
		ConsoleCommand("exit");
		Step = 2;
	}
}
