//=============================================================================
// PadModeConsole: the pad as the console's KeyEvent sees it, for the pad's
// DXController mode (Settings.json's Gamepad.DXController) without the mod's
// packages, beside tools/dxcap/timelines/pad-mode.txt's virtual pad. Every
// KeyEvent of a pad's key (Joy1 to Joy16, JoyX, JoyY, JoyZ, JoyR, JoyU, JoyV,
// the d-pad's four, which this class's stale enum names UnknownF0 to
// UnknownF3, and UnknownD8 to UnknownF5 for a map's own keys) logged with
// "DXPADMODE:", its name, its number, the action and the value, and the
// seconds since the run began; each run of JoyU's axis events from the first
// to its 0, its ticks, the view's yaw turned and the JoyU values summed, so
// the yaw a tick for a unit of JoyU (the axis's binding's arithmetic, its
// stock "Axis aturn speed=5.9"); the mode's four console words sent, each
// answer logged: GamepadGetInfo, GamepadSampleCurve Left 5, Right 5 and
// Right 300, GamepadGetRawMag at rest at ReadTime (12) and again at HeldTime
// (31), and GamepadReload at ReloadTime (32) and from A's first press (Joy1),
// as the mod's settings send it from the press that changes a value: A held
// gives one press and one release, not one a tick. An exit after Seconds (55).
//=============================================================================
class PadModeConsole extends Console;

var config float Seconds, ReadTime, HeldTime, ReloadTime;
var float RunTime;
var bool bRead, bHeld, bReloaded, bExited, bPressReloaded;
var bool bTurning;
var int TurnTicks, LastYaw;
var float TurnYaw, TurnSum, TurnStart, LastJoyU;

function bool IsPadKey(byte K)
{
	return (K >= 200 && K <= 215) || (K >= 224 && K <= 227) || K == 232 || K == 233 || (K >= 240 && K <= 243);
}

function Ask(string Command)
{
	Log("DXPADMODE: '" $ Command $ "' gave '" $ Viewport.Actor.ConsoleCommand(Command) $ "' at " $ RunTime);
}

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	local byte K;

	K = Key;
	if (IsPadKey(K) || (K >= 216 && K <= 245 && Action == IST_Axis))
		Log("DXPADMODE: key " $ GetEnum(enum'EInputKey', Key) $ " (" $ K $ ") " $ GetEnum(enum'EInputAction', Action) $ " " $ Delta $ " at " $ RunTime);
	if (K == 232 && Action == IST_Axis)
		LastJoyU = Delta;
	else if (K == 232 && Action == IST_Release)
		LastJoyU = 0;
	if (K == 200 && Action == IST_Press && !bPressReloaded)
	{
		bPressReloaded = true;
		Ask("GamepadReload");
	}
	return Super.KeyEvent(Key, Action, Delta);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int Yaw, Step;
	local float Limit, Ratio;

	Super.Tick(Delta);
	RunTime += Delta;
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	// The view's turn while JoyU's events come, each tick's step taken the
	// short way round
	Yaw = P.ViewRotation.Yaw & 65535;
	if (LastJoyU != 0)
	{
		if (!bTurning)
		{
			bTurning = true;
			TurnTicks = 0;
			TurnYaw = 0;
			TurnSum = 0;
			TurnStart = RunTime;
		}
		else
		{
			Step = (Yaw - LastYaw) & 65535;
			if (Step > 32768)
				Step -= 65536;
			TurnYaw += Step;
		}
		TurnTicks++;
		TurnSum += LastJoyU;
	}
	else if (bTurning)
	{
		bTurning = false;
		Ratio = 0;
		if (TurnSum != 0)
			Ratio = TurnYaw / TurnSum;
		Log("DXPADMODE: turn of " $ TurnTicks $ " ticks over " $ (RunTime - TurnStart) $ " s: yaw " $ TurnYaw $ ", JoyU summed " $ TurnSum $ ", yaw a tick a JoyU unit " $ Ratio);
	}
	LastYaw = Yaw;

	if (!bRead && RunTime > ReadTime)
	{
		bRead = true;
		Ask("GamepadGetInfo");
		Ask("GamepadSampleCurve Left 5");
		Ask("GamepadSampleCurve Right 5");
		Ask("GamepadSampleCurve Right 300");
		Ask("GamepadGetRawMag");
	}
	if (!bHeld && RunTime > HeldTime)
	{
		bHeld = true;
		Ask("GamepadGetRawMag");
		Ask("GamepadGetInfo");
	}
	if (!bReloaded && RunTime > ReloadTime)
	{
		bReloaded = true;
		Ask("GamepadReload");
	}

	Limit = Seconds;
	if (Limit <= 0)
		Limit = 55.0;
	if (!bExited && RunTime > Limit)
	{
		Log("DXPADMODE: exiting at " $ RunTime);
		P.ConsoleCommand("exit");
		bExited = true;
	}
}

defaultproperties
{
	ReadTime=12.0
	HeldTime=31.0
	ReloadTime=32.0
	Seconds=55.0
}
