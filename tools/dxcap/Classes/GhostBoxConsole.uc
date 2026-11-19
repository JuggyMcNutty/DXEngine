//=============================================================================
// GhostBoxConsole (bugs.md AI.24): GetBoundingBox of an actor already
// destroyed puts it back in the BSP's actor lists, which nothing takes it out
// of again (NActor::GetBoundingBox -> UActor::UpdateBspInfo, no bDeleteMe
// test; UActor::Destroy unlinked it before). On Liberty Island, 3 s in: a
// CrateUnbreakableSmall spawned 120 units before the player, in view;
// 4 s in destroyed, then its GetBoundingBox asked (as a mod's script may ask
// of a reference it still holds); 4 to 6 s a screenshot each second (look
// for the crate still drawn after its destroy); 6 s in "obj garbage", then
// 2 s more of frames (under the sanitizer build: a heap-use-after-free in
// VisibleFrame's node walk when the crate was alone in its node, or the
// collector's log of an eliminated object kept through a MarkConst); 8.5 s
// exit. Each line starts "DXGHOST:". The original: GetBoundingBox reads the
// primitive's box and files nothing (engine-dll.md has no BSP actor lists).
//=============================================================================
class GhostBoxConsole extends Console;

var float MapTime;
var int Phase;
var Actor Box;

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Lo, Hi, At;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0 && MapTime > 3.0)
	{
		At = P.Location + vector(P.Rotation) * 120;
		Box = P.Spawn(class'DeusEx.CrateUnbreakableSmall',,, At);
		Log("DXGHOST: spawned" @ Box @ "at" @ At);
		Phase = 1;
	}
	else if (Phase == 1 && MapTime > 4.0)
	{
		if (Box != None)
		{
			Box.Destroy();
			Log("DXGHOST: destroyed, bDeleteMe" @ Box.bDeleteMe);
			Log("DXGHOST: GetBoundingBox" @ Box.GetBoundingBox(Lo, Hi) @ Lo @ Hi);
		}
		P.ConsoleCommand("shot");
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 5.0)
	{
		P.ConsoleCommand("shot");
		Phase = 3;
	}
	else if (Phase == 3 && MapTime > 6.0)
	{
		P.ConsoleCommand("shot");
		Box = None;
		Log("DXGHOST: obj garbage");
		P.ConsoleCommand("obj garbage");
		Phase = 4;
	}
	else if (Phase == 4 && MapTime > 8.5)
	{
		Log("DXGHOST: exit");
		Phase = 9;
		P.ConsoleCommand("exit");
	}
}
