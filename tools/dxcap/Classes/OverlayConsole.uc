//=============================================================================
// OverlayConsole: what Canvas.DrawActor and Canvas.DrawClippedActor leave of
// their actor's bHidden, logged with "DXOVERLAY:" from wherever the run
// starts (docs/DEVELOPMENT.md, scripted runs; bugs.md E10a.25). 2 s in, from
// the console's PostRender: the player's pawn, visible, drawn with
// Canvas.DrawActor and its bHidden logged after; hidden, drawn and logged;
// visible again, drawn with Canvas.DrawClippedActor into a 128 by 128 frame
// and logged; its flag put back. Then an exit. The original's natives clear
// the flag for the draw and set it afterwards, whatever it was (Engine.dll
// UCanvas::execDrawActor, execDrawClippedActor), where GC.DrawActor puts it
// back; DXEngine once put it back for all three. With Wire
// (DXCAP_CONFIG="Wire=True"), instead: on Liberty Island, opened from
// wherever the run starts, 3 s in, a large crate 200 units ahead and a
// JCDentonMale 160 ahead holding a pistol, each still and hidden, drawn each
// frame by Canvas.DrawActor with WireFrame (with Solid too, without);
// shots marked 2 s and 4 s on, as CaptureConsole marks them; then an exit.
//=============================================================================
class OverlayConsole extends Console;

var config bool Wire;
var config bool Solid;

var float RunTime;
var int Phase;
var bool bProbe;
var string CurrentMap;
var float MapTime;
var int MarkShot;
var Actor Crate, Figure;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

event PostRender(canvas C)
{
	local PlayerPawn P;
	local bool bBefore;
	local int i;

	Super.PostRender(C);
	if (Wire)
	{
		if (Crate != None)
			C.DrawActor(Crate, !Solid);
		if (Figure != None)
			C.DrawActor(Figure, !Solid);
		if (MarkShot == 0)
			return;
		C.Style = 1;
		C.SetPos(0, 0);
		C.DrawColor.R = 255;
		C.DrawColor.G = 0;
		C.DrawColor.B = 255;
		C.DrawRect(Texture'Solid', 12, 12);
		for (i = 0; i < 8; i++)
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
			if ((MarkShot & (1 << i)) != 0)
			{
				C.DrawColor.R = 255;
				C.DrawColor.G = 255;
				C.DrawColor.B = 255;
			}
			C.SetPos(12 + 12 * i, 0);
			C.DrawRect(Texture'Solid', 12, 12);
		}
		return;
	}
	if (!bProbe || Viewport == None || Viewport.Actor == None)
		return;
	bProbe = false;
	P = Viewport.Actor;

	bBefore = P.bHidden;
	P.bHidden = false;
	C.DrawActor(P, false);
	Log("DXOVERLAY: DrawActor of the visible pawn: hidden after " $ P.bHidden);

	P.bHidden = true;
	C.DrawActor(P, false);
	Log("DXOVERLAY: DrawActor of the hidden pawn: hidden after " $ P.bHidden);

	P.bHidden = false;
	C.DrawClippedActor(P, false, 128, 128, 0, 0);
	Log("DXOVERLAY: DrawClippedActor of the visible pawn: hidden after " $ P.bHidden);

	P.bHidden = bBefore;
	Phase = 1;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Wire)
	{
		WireTick(P, Delta);
		return;
	}
	if (Phase == 0 && RunTime > 2.0)
		bProbe = true;
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXOVERLAY: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

function WireTick(PlayerPawn P, float Delta)
{
	local rotator Facing;
	local Pawn Holder;
	local Inventory Pistol;

	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0)
			return;
		Facing.Yaw = P.Rotation.Yaw;
		Crate = P.Spawn(class'CrateUnbreakableLarge',,, P.Location + 200 * vector(Facing) + vect(0, 0, 20), rot(0, 4000, 0));
		Facing.Yaw += 6000;
		Holder = P.Spawn(class'JCDentonMale',,, P.Location + 160 * vector(Facing), rot(0, 0, 0) + Facing + rot(0, 32768, 0));
		Figure = Holder;
		if (Crate != None)
		{
			Crate.SetPhysics(PHYS_None);
			Crate.bHidden = true;
		}
		if (Holder != None)
		{
			Holder.SetPhysics(PHYS_None);
			Holder.GotoState('');
			Holder.AnimSequence = 'Still';
			Holder.AnimFrame = 0;
			Holder.AnimRate = 0;
			Holder.bHidden = true;
			Pistol = Holder.Spawn(class'WeaponPistol');
			if (Pistol != None)
			{
				Pistol.GiveTo(Holder);
				Holder.Weapon = Weapon(Pistol);
				Pistol.bHidden = true;
			}
		}
		Log("DXOVERLAY: wire: crate " $ Crate $ " at " $ Crate.Location $ ", " $ Figure $ " at " $ Figure.Location $ " holding " $ Holder.Weapon);
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			MapTime = 0;
			Phase = 0;
		}
	}
	else if (Phase == 1 || Phase == 2)
	{
		if (MapTime > 3.0 + 2.0 * Phase - 0.6)
			MarkShot = Phase;
		if (MapTime > 3.0 + 2.0 * Phase)
		{
			Log("DXOVERLAY: wire: shot " $ Phase $ ", the figure's frame " $ Figure.AnimSequence $ " " $ Figure.AnimFrame);
			P.ConsoleCommand("shot");
			MarkShot = 0;
			Phase++;
		}
	}
	else if (Phase == 3)
	{
		Log("DXOVERLAY: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
