//=============================================================================
// SimFallServer: one of a dedicated server's ServerActors (tools/dxcap.sh
// dedicated <engine> SimFallServer 'DXMP_Cathedral?game=DXCapture.
// CapDeathMatch' 60; any join console the client, JoinConsole). bugs.md
// NET.31. 3 s after a player is in it spawns a CapSimFall at PHYS_Projectile
// with no velocity, so that its first bunch carries its Physics, then sets
// its Physics to PHYS_None and back every 2 s, each change logged; it exits
// 14 s after the spawn. The client's CapSimFall logs the Physics it has: in
// the original's game the first alone. Each line starts "DXSIMFALL:".
//=============================================================================
class SimFallServer extends Info;

var float InTime, Since;
var int Step, Toggles;
var CapSimFall Placed;

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
		Placed = Spawn(class'CapSimFall',,, Joined.Location + vect(0,0,200));
		Placed.SetPhysics(PHYS_Projectile);
		Placed.Velocity = vect(0,0,0);
		Log("DXSIMFALL: server placed " $ Placed $ ", physics " $ int(Placed.Physics));
		Step = 1;
		return;
	}
	if (Step != 1)
		return;
	Since += Delta;
	if (Since >= 2.0 && Toggles < 6)
	{
		Since = 0;
		Toggles++;
		if (Placed.Physics == PHYS_None)
			Placed.SetPhysics(PHYS_Projectile);
		else
			Placed.SetPhysics(PHYS_None);
		Placed.Velocity = vect(0,0,0);
		Log("DXSIMFALL: server physics " $ int(Placed.Physics));
	}
	else if (Toggles >= 6 && Since > 2.0)
	{
		Log("DXSIMFALL: server exiting");
		ConsoleCommand("exit");
		Step = 2;
	}
}
