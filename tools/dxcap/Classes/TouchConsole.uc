//=============================================================================
// TouchConsole: the touch slots, logged with "DXTOUCH:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a CapTrigger (radius 150) spawned
// 400 units over the player, four small crates spawned 360 units aside of
// it, held still (no physics), and each put within its radius by
// SetLocation, which touches (a spawn does not), each touch logged with the
// trigger's four slots; 1 s later the player is put at the trigger's
// centre, clear of the crates, and what the trigger logs says whether it
// can still be touched; 1 s later the crates' and the trigger's Touching
// are logged and the run exits. What is compared between the engines:
// every line, exactly -- the original frees the full slots of their
// touchers with no physics for the player, and fills the last free slot
// first.
//=============================================================================
class TouchConsole extends Console;

var float RunTime;
var int Phase;
var CapTrigger T;
var Actor Crates[4];
var vector Spot;

function string Slots(Actor A)
{
	local string S;
	local int i;

	for (i = 0; i < 4; i++)
	{
		if (i > 0)
			S = S $ ",";
		if (A.Touching[i] == None)
			S = S $ "-";
		else
			S = S $ A.Touching[i].Class.Name;
	}
	return S;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int i;
	local vector Off;

	Super.Tick(Delta);
	if (Phase == 3 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Spot = P.Location + vect(0, 0, 400);
		T = P.Spawn(class'CapTrigger',,, Spot);
		if (T == None)
		{
			Log("DXTOUCH: no trigger spawned");
			Phase = 3;
			P.ConsoleCommand("exit");
			return;
		}
		T.SetPhysics(PHYS_None);
		Log("DXTOUCH: trigger spawned, slots " $ T.Slots());
		for (i = 0; i < 4; i++)
		{
			Off = vect(0, 0, 0);
			if (i == 0) Off.X = 90;
			if (i == 1) Off.X = -90;
			if (i == 2) Off.Y = 90;
			if (i == 3) Off.Y = -90;
			Crates[i] = P.Spawn(class'CrateUnbreakableSmall',,, Spot + Off * 4);
			if (Crates[i] == None)
				Log("DXTOUCH: crate " $ i $ " not spawned");
			else
			{
				Crates[i].SetPhysics(PHYS_None);
				Log("DXTOUCH: crate " $ i $ " spawned aside, physics " $ Crates[i].Physics $ ", the trigger's slots " $ T.Slots());
				Log("DXTOUCH: crate " $ i $ " put in, at " $ Crates[i].SetLocation(Spot + Off) $ ", its slots " $ Slots(Crates[i]) $ ", the trigger's " $ T.Slots());
			}
		}
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		P.SetPhysics(PHYS_None);
		P.Velocity = vect(0, 0, 0);
		P.SetLocation(Spot);
		Log("DXTOUCH: player put at the trigger, at " $ (P.Location == Spot) $ ", the trigger's slots " $ T.Slots() $ ", the player's " $ Slots(P));
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.0)
	{
		for (i = 0; i < 4; i++)
			if (Crates[i] != None)
				Log("DXTOUCH: crate " $ i $ " slots " $ Slots(Crates[i]));
		Log("DXTOUCH: the trigger's slots " $ T.Slots() $ ", the player's " $ Slots(P));
		Log("DXTOUCH: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
