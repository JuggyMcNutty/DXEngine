//=============================================================================
// MouseLookConsole: the turn a mouse move gives against the move the input
// received, in both engines (bugs.md ENG.36). On Liberty Island, 6 s in, the
// player at rest and in play: every MouseX and MouseY axis event the
// console's KeyEvent sees is summed, and each tick that moved logs the sums
// since the line before, the tick's length and the player's ViewRotation,
// with "DXMOUSELOOK:" in front; at Seconds (25 by default) the totals -- the
// yaw turned over the MouseX units received, the pitch over MouseY's, each
// way's steps added whatever their sign, so a move there and back counts
// both -- then an exit. The moves come from tools/dxcap/timelines/mouse-look.txt on
// DXEngine (raw counts, as its window sends them), or from
// tools/dxcap/mouselook.py's uinput mouse in either engine while the game's
// window has the focus (DXCAP_FULLSCREEN=1 puts the pointer over it).
//
// The original's MouseX is the window's pointer move in pixels from its
// middle (WinDrv.dll 0x1110adbd-0x1110b04a), the desktop's acceleration in
// it; DXEngine's the raw count, by choice. So the run compares the turn a
// unit received, which neither touches: the original's AXIS adds speed x the
// move x 0.01 an event (UInput::Exec, Engine.dll 0x10393e10) and its read
// scales every input float by 20 over the tick (UInput::ReadInput
// 0x10394580): 1.2 units of aMouseX x the tick a unit with the stock
// MouseX=Axis aMouseX Speed=6.0, and PlayerInput turns 32 x the tick x
// aMouseX x MouseSensitivity x DesiredFOV x 0.01111 a tick, some 96 yaw a
// unit at the stock 3 and 75. The build before ENG.36's fix added speed x 16
// x the move / (150 x the tick): some 51 a unit.
//=============================================================================
class MouseLookConsole extends Console;

var config float Seconds;

var float MapTime;
var string CurrentMap;
var int Phase;
var float SumX, SumY, TotalX, TotalY;
var int LastYaw, LastPitch, Turned, Pitched;

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

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	if (Phase == 1 && Action == IST_Axis)
	{
		if (Key == IK_MouseX)
		{
			SumX += Delta;
			TotalX += Abs(Delta);
		}
		else if (Key == IK_MouseY)
		{
			SumY += Delta;
			TotalY += Abs(Delta);
		}
	}
	return Super.KeyEvent(Key, Action, Delta);
}

// A step of a rotator's component, the shorter way round
function int Step(int Now, int Before)
{
	local int D;

	D = Now - Before;
	while (D > 32768)
		D -= 65536;
	while (D < -32768)
		D += 65536;
	return D;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int DYaw, DPitch;
	local float Limit;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

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
		if (MapTime < 6.0)
			return;
		LastYaw = P.ViewRotation.Yaw;
		LastPitch = P.ViewRotation.Pitch;
		Log("DXMOUSELOOK: start, yaw " $ LastYaw $ " pitch " $ LastPitch $ ", MouseSensitivity " $ P.MouseSensitivity $ ", DesiredFOV " $ P.DesiredFOV $ ", bMaxMouseSmoothing " $ P.bMaxMouseSmoothing $ ", bInvertMouse " $ P.bInvertMouse);
		Log("DXMOUSELOOK: MouseX=" $ P.ConsoleCommand("keybinding MouseX") $ ", MouseY=" $ P.ConsoleCommand("keybinding MouseY"));
		Phase = 1;
		return;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (Phase == 1)
	{
		DYaw = Step(P.ViewRotation.Yaw, LastYaw);
		DPitch = Step(P.ViewRotation.Pitch, LastPitch);
		Turned += Abs(DYaw);
		Pitched += Abs(DPitch);
		if (SumX != 0 || SumY != 0 || DYaw != 0 || DPitch != 0)
			Log("DXMOUSELOOK: at " $ MapTime $ " tick " $ Delta $ ": MouseX " $ SumX $ " MouseY " $ SumY $ ", yaw " $ P.ViewRotation.Yaw $ " (" $ DYaw $ "), pitch " $ P.ViewRotation.Pitch $ " (" $ DPitch $ ")");
		SumX = 0;
		SumY = 0;
		LastYaw = P.ViewRotation.Yaw;
		LastPitch = P.ViewRotation.Pitch;

		Limit = Seconds;
		if (Limit <= 0)
			Limit = 25.0;
		if (MapTime > Limit)
		{
			Log("DXMOUSELOOK: totals: MouseX " $ TotalX $ " yaw " $ Turned $ " (" $ (Turned / FMax(TotalX, 1.0)) $ " a unit), MouseY " $ TotalY $ " pitch " $ Pitched $ " (" $ (Pitched / FMax(TotalY, 1.0)) $ " a unit)");
			Log("DXMOUSELOOK: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
		}
	}
}
