#include "Precomp.h"
#include "DXControllerPad.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Utils/StrTools.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
	const char* const SettingsSection = "DXController.ControllerSettings";
	const char* const ButtonSection = "DXController.GamepadButtonMap";
	const char* const AxisSection = "DXController.GamepadAxisMap";

	// The buttons by SDL's names for them, GamepadButton's then
	// GamepadExtraButton's order, with the keys they send by default
	struct ButtonName { const char* Name; EInputKey Key; };
	const ButtonName ButtonTable[DXControllerPad::NumButtons] =
	{
		{ "a", IK_Joy1 }, { "b", IK_Joy2 }, { "x", IK_Joy3 }, { "y", IK_Joy4 },
		{ "back", IK_Joy7 }, { "guide", IK_None }, { "start", IK_Joy8 },
		{ "leftstick", IK_Joy9 }, { "rightstick", IK_Joy10 },
		{ "leftshoulder", IK_Joy5 }, { "rightshoulder", IK_Joy6 },
		{ "dpup", IK_JoyPovUp }, { "dpdown", IK_JoyPovDown }, { "dpleft", IK_JoyPovLeft }, { "dpright", IK_JoyPovRight },
		{ "misc1", IK_Joy15 },
		{ "paddle1", IK_Joy11 }, { "paddle2", IK_Joy12 }, { "paddle3", IK_Joy13 }, { "paddle4", IK_Joy14 },
		{ "touchpad", IK_Joy16 },
		{ "misc2", IK_None }, { "misc3", IK_None }, { "misc4", IK_None }, { "misc5", IK_None }, { "misc6", IK_None },
	};

	// The sticks' and triggers' keys, in the order the pad's axes are sent
	const EInputKey AxisKeys[6] = { IK_JoyX, IK_JoyY, IK_JoyU, IK_JoyV, IK_JoyZ, IK_JoyR };

	// A stick pushed this far on a pad not the active one makes it active
	const float SwitchTravel = 8000.0f / 32767.0f;

	bool IsAxisKey(EInputKey key)
	{
		return std::find(std::begin(AxisKeys), std::end(AxisKeys), key) != std::end(AxisKeys);
	}

	std::string KeyName(EInputKey key)
	{
		const char* name = Engine::keynames[(uint8_t)key];
		return name && *name ? name : std::to_string((int)key);
	}

	// A key by its name without IK_, in any case, as [Extension.InputExt]'s
	// bindings name it, or by its number, 0 to 255; None is IK_None
	bool ParseKey(const std::string& text, EInputKey& key)
	{
		if (text.empty())
			return false;
		if (std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
		{
			if (text.size() > 3 || std::atoi(text.c_str()) > 255)
				return false;
			key = (EInputKey)std::atoi(text.c_str());
			return true;
		}
		for (int i = 0; i < 256; i++)
		{
			if (Engine::keynames[i] && *Engine::keynames[i] && StrTools::equals_ignore_case(Engine::keynames[i], text))
			{
				key = (EInputKey)i;
				return true;
			}
		}
		return false;
	}

	std::string Trim(const std::string& text)
	{
		size_t start = text.find_first_not_of(" \t");
		if (start == std::string::npos)
			return {};
		size_t end = text.find_last_not_of(" \t");
		return text.substr(start, end - start + 1);
	}

	std::string Format(const char* format, double value)
	{
		char text[64];
		std::snprintf(text, sizeof(text), format, value);
		return text;
	}

	// A key of [DXController.ControllerSettings]: its value as the mod's
	// config class writes it, held within its range; one missing written
	// back with its default; one that does not read keeps its default
	class SettingsReader
	{
	public:
		SettingsReader(PackageManager* packages, bool& written) : Packages(packages), Written(written) {}

		void Int(const char* key, int& target, int low, int high)
		{
			std::string value;
			if (!Find(key, value, std::to_string(target)))
				return;
			char* end = nullptr;
			long number = std::strtol(value.c_str(), &end, 10);
			if (end != value.c_str())
				target = (int)std::clamp(number, (long)low, (long)high);
		}

		void Float(const char* key, float& target, float low, float high)
		{
			std::string value;
			if (!Find(key, value, Format("%f", target)))
				return;
			char* end = nullptr;
			double number = std::strtod(value.c_str(), &end);
			if (end != value.c_str() && std::isfinite(number))
				target = std::clamp((float)number, low, high);
		}

		void Bool(const char* key, bool& target)
		{
			std::string value;
			if (!Find(key, value, target ? "True" : "False"))
				return;
			if (StrTools::equals_ignore_case(value, "true") || value == "1")
				target = true;
			else if (StrTools::equals_ignore_case(value, "false") || value == "0")
				target = false;
		}

		void Curve(const char* key, StickCurve& target)
		{
			static const char* const names[] = { "Linear", "Power", "Expo", "Sigmoid" };
			std::string value;
			if (!Find(key, value, names[(int)target]))
				return;
			for (int i = 0; i < 4; i++)
			{
				if (StrTools::equals_ignore_case(value, names[i]))
					target = (StickCurve)i;
			}
		}

	private:
		bool Find(const char* key, std::string& value, const std::string& fallback)
		{
			if (Packages->FindIniValue("system", SettingsSection, key, value))
			{
				value = Trim(value);
				return true;
			}
			Packages->SetIniValue("system", SettingsSection, key, fallback);
			Written = true;
			return false;
		}

		PackageManager* Packages;
		bool& Written;
	};
}

void DXControllerPad::LoadIni(PackageManager* packages)
{
	bool written = false;
	LoadSettings(packages, written);
	LoadButtonMap(packages, written);
	LoadAxisMap(packages);
	// What was written back on disk at once, as the mod's own start writes
	// it, so that the file the player opens is whole
	if (written)
		packages->FlushIniFiles();
}

void DXControllerPad::LoadSettings(PackageManager* packages, bool& written)
{
	// The defaults: the left stick a power of 2, the right a sigmoid of
	// steepness 8 about 0.7 at strength 0.9 and scaled to 0.4, each the dead
	// zone 3500; the triggers' threshold 30 of 255
	Left = StickResponse();
	Left.Curve = StickCurve::Power;
	Right = StickResponse();
	Right.Curve = StickCurve::Sigmoid;
	Right.Scale = 0.4f;
	TriggerThreshold = 30;
	InvertLookY = false;

	SettingsReader read(packages, written);
	read.Int("StickDeadzoneLeft", Left.Deadzone, 0, 32766);
	read.Int("StickDeadzoneRight", Right.Deadzone, 0, 32766);
	read.Curve("StickCurveLeft", Left.Curve);
	read.Curve("StickCurveRight", Right.Curve);
	read.Float("StickCurvePowerLeft", Left.Power, 0.1f, 10.0f);
	read.Float("StickCurvePowerRight", Right.Power, 0.1f, 10.0f);
	read.Float("StickCurveExpoLeft", Left.Expo, 0.0f, 1.0f);
	read.Float("StickCurveExpoRight", Right.Expo, 0.0f, 1.0f);
	read.Float("StickCurveSigmoidSteepnessLeft", Left.Steepness, 1.0f, 12.0f);
	read.Float("StickCurveSigmoidSteepnessRight", Right.Steepness, 1.0f, 12.0f);
	read.Float("StickCurveSigmoidMidpointLeft", Left.Midpoint, 0.15f, 0.85f);
	read.Float("StickCurveSigmoidMidpointRight", Right.Midpoint, 0.15f, 0.85f);
	read.Float("StickCurveSigmoidStrengthLeft", Left.Strength, 0.0f, 1.0f);
	read.Float("StickCurveSigmoidStrengthRight", Right.Strength, 0.0f, 1.0f);
	read.Float("StickScaleRight", Right.Scale, 0.1f, 1.0f);
	read.Bool("InvertLookY", InvertLookY);
	read.Int("TriggerThreshold", TriggerThreshold, 0, 255);
}

// The defaults, then each line of the section, the later of two for one
// button winning; then every clash rejected and logged: a button sent to a
// stick's or a trigger's key, or two to one key, the one moved by a line
// going back to its default, or to None where its default is taken too
void DXControllerPad::LoadButtonMap(PackageManager* packages, bool& written)
{
	bool moved[NumButtons] = {};
	for (int i = 0; i < NumButtons; i++)
		ButtonKeys[i] = ButtonTable[i].Key;

	for (const auto& [name, value] : packages->GetIniKeyValues("system", ButtonSection))
	{
		int button = -1;
		for (int i = 0; i < NumButtons; i++)
		{
			if (StrTools::equals_ignore_case(name, ButtonTable[i].Name))
				button = i;
		}
		if (button < 0)
		{
			LogMessage(std::string("DXController: ") + ButtonSection + ": " + name + " is no button, passed over");
			continue;
		}
		EInputKey key;
		if (!ParseKey(Trim(value), key))
		{
			LogMessage(std::string("DXController: ") + ButtonSection + ": " + name + "=" + value + " names no key: its default stands");
			continue;
		}
		ButtonKeys[button] = key;
		moved[button] = key != ButtonTable[button].Key;
	}

	for (int i = 0; i < NumButtons; i++)
	{
		if (moved[i] && IsAxisKey(ButtonKeys[i]))
		{
			LogMessage(std::string("DXController: ") + ButtonSection + ": " + ButtonTable[i].Name + " on " + KeyName(ButtonKeys[i]) +
				", a stick's or a trigger's: refused, back to " + KeyName(ButtonTable[i].Key));
			ButtonKeys[i] = ButtonTable[i].Key;
			moved[i] = false;
		}
	}

	// Each pass settles one button; at most two passes a button
	for (int pass = 0; pass < 2 * NumButtons; pass++)
	{
		int loser = -1, other = -1;
		for (int i = 0; i < NumButtons && loser < 0; i++)
		{
			for (int j = i + 1; j < NumButtons && loser < 0; j++)
			{
				if (ButtonKeys[i] == IK_None || ButtonKeys[i] != ButtonKeys[j])
					continue;
				// The one a line moved there loses; of two moved, the later
				// in SDL's order
				loser = moved[j] ? j : moved[i] ? i : (ButtonKeys[j] != ButtonTable[j].Key ? j : i);
				other = loser == j ? i : j;
			}
		}
		if (loser < 0)
			break;
		EInputKey back = ButtonKeys[loser] != ButtonTable[loser].Key ? ButtonTable[loser].Key : IK_None;
		LogMessage(std::string("DXController: ") + ButtonSection + ": " + ButtonTable[loser].Name + " and " + ButtonTable[other].Name +
			" both on " + KeyName(ButtonKeys[loser]) + ": " + ButtonTable[loser].Name + " refused, on " + KeyName(back));
		ButtonKeys[loser] = back;
		moved[loser] = false;
	}

	// Every button's name written back with its default where the section
	// lacks it
	for (int i = 0; i < NumButtons; i++)
	{
		std::string value;
		if (!packages->FindIniValue("system", ButtonSection, ButtonTable[i].Name, value))
		{
			packages->SetIniValue("system", ButtonSection, ButtonTable[i].Name, KeyName(ButtonTable[i].Key));
			written = true;
		}
	}
}

// No defaults and never written back: each line a source and a key of its
// own, with an optional Scale= (1000) and Deadzone= (0, in the source's
// units); a source or a key that does not read, or a key a stick, a trigger,
// a button or an earlier line has, refused and logged
void DXControllerPad::LoadAxisMap(PackageManager* packages)
{
	static const struct { const char* Name; Source From; } sources[] =
	{
		{ "gyro.pitch", Source::GyroPitch }, { "gyro.yaw", Source::GyroYaw }, { "gyro.roll", Source::GyroRoll },
		{ "accel.x", Source::AccelX }, { "accel.y", Source::AccelY }, { "accel.z", Source::AccelZ },
		{ "touchpad.x", Source::TouchX }, { "touchpad.y", Source::TouchY },
	};

	AxisLines.clear();
	WantGyro = WantAccel = false;
	for (const auto& [name, value] : packages->GetIniKeyValues("system", AxisSection))
	{
		AxisLine line;
		bool known = false;
		for (const auto& source : sources)
		{
			if (StrTools::equals_ignore_case(name, source.Name))
			{
				line.From = source.From;
				known = true;
			}
		}
		if (!known && name.size() > 8 && StrTools::equals_ignore_case(name.substr(0, 8), "joyaxis."))
		{
			const std::string number = name.substr(8);
			if (number.size() <= 3 && std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; }))
			{
				line.From = Source::JoyAxis;
				line.JoyAxis = std::atoi(number.c_str());
				known = true;
			}
		}
		const std::string say = std::string("DXController: ") + AxisSection + ": " + name + "=" + value;
		if (!known)
		{
			LogMessage(say + ": no such source, passed over");
			continue;
		}

		Array<std::string> words;
		for (size_t pos = 0; pos < value.size();)
		{
			size_t start = value.find_first_not_of(" \t", pos);
			if (start == std::string::npos)
				break;
			size_t end = value.find_first_of(" \t", start);
			if (end == std::string::npos)
				end = value.size();
			words.push_back(value.substr(start, end - start));
			pos = end;
		}
		if (words.empty() || !ParseKey(words[0], line.Key) || line.Key == IK_None)
		{
			LogMessage(say + ": names no key, passed over");
			continue;
		}
		for (size_t i = 1; i < words.size(); i++)
		{
			const std::string& word = words[i];
			size_t equals = word.find('=');
			std::string option = equals == std::string::npos ? word : word.substr(0, equals);
			double number = equals == std::string::npos ? 0.0 : std::strtod(word.c_str() + equals + 1, nullptr);
			if (StrTools::equals_ignore_case(option, "scale") && std::isfinite(number))
				line.Scale = (float)number;
			else if (StrTools::equals_ignore_case(option, "deadzone") && std::isfinite(number))
				line.Deadzone = (float)std::fabs(number);
			else
				LogMessage(say + ": " + word + " not read");
		}

		bool taken = IsAxisKey(line.Key);
		for (int i = 0; i < NumButtons && !taken; i++)
			taken = ButtonKeys[i] == line.Key;
		for (const AxisLine& earlier : AxisLines)
			taken = taken || earlier.Key == line.Key;
		if (taken)
		{
			LogMessage(say + ": " + KeyName(line.Key) + " is taken, refused");
			continue;
		}

		WantGyro = WantGyro || line.From == Source::GyroPitch || line.From == Source::GyroYaw || line.From == Source::GyroRoll;
		WantAccel = WantAccel || line.From == Source::AccelX || line.From == Source::AccelY || line.From == Source::AccelZ;
		AxisLines.push_back(line);
	}
}

void DXControllerPad::Reload(Engine* engine, PackageManager* packages)
{
	for (int i = 0; i < NumButtons; i++)
		HeldOver[i] = HeldOver[i] || Held[i];
	ReleaseAll(engine);
	LoadIni(packages);
}

void DXControllerPad::ReadButtons(const GamepadFullState& pad, std::array<bool, NumButtons>& down)
{
	for (int i = 0; i < (int)GamepadButton::Count; i++)
		down[i] = pad.Buttons[i];
	for (int i = 0; i < (int)GamepadExtraButton::Count; i++)
		down[(int)GamepadButton::Count + i] = pad.Extra[i];
}

// A pad not the active one takes over by a button's press or a stick past
// SwitchTravel; a trigger never, some resting off their floor
bool DXControllerPad::OtherPadWantsIn(const GamepadFullState& pad, const std::array<bool, NumButtons>& before)
{
	std::array<bool, NumButtons> down;
	ReadButtons(pad, down);
	for (int i = 0; i < NumButtons; i++)
	{
		if (down[i] && !before[i])
			return true;
	}
	const GamepadAxis sticks[] = { GamepadAxis::LeftX, GamepadAxis::LeftY, GamepadAxis::RightX, GamepadAxis::RightY };
	for (GamepadAxis axis : sticks)
	{
		if (std::fabs(pad.Axes[(int)axis]) > SwitchTravel)
			return true;
	}
	return false;
}

void DXControllerPad::Update(Engine* engine)
{
	DisplayBackend* backend = DisplayBackend::Get();
	if (!backend)
		return;
	if (!SensorsSet || SensorsGyro != WantGyro || SensorsAccel != WantAccel)
	{
		backend->SetGamepadSensors(WantGyro, WantAccel);
		SensorsSet = true;
		SensorsGyro = WantGyro;
		SensorsAccel = WantAccel;
	}
	backend->GetGamepads(Pads);

	// The active pad gone: what it held let go, the next pad attached
	// active
	const GamepadFullState* active = nullptr;
	for (const GamepadFullState& pad : Pads)
	{
		if (HasActive && pad.Id == ActiveId)
			active = &pad;
	}
	if (HasActive && !active)
	{
		LogMessage("DXController: the " + ActiveType + " pad went");
		ReleaseAll(engine);
		HasActive = false;
	}

	// A press or a push on another pad makes it the active one, what the
	// last one held let go first
	for (const GamepadFullState& pad : Pads)
	{
		if (!active || pad.Id == ActiveId)
			continue;
		auto last = LastButtons.find(pad.Id);
		std::array<bool, NumButtons> before = {};
		if (last != LastButtons.end())
			before = last->second;
		if (last != LastButtons.end() && OtherPadWantsIn(pad, before))
		{
			ReleaseAll(engine);
			active = &pad;
			HasActive = false;
			break;
		}
	}
	if (!active && !Pads.empty())
		active = &Pads.front();
	if (active && (!HasActive || active->Id != ActiveId))
	{
		HasActive = true;
		ActiveId = active->Id;
		ActiveType = active->Type;
		std::fill(std::begin(HeldOver), std::end(HeldOver), false);
		LogMessage("DXController: the " + ActiveType + " pad active");
	}

	// Every pad's buttons kept, those of pads gone dropped
	std::map<unsigned int, std::array<bool, NumButtons>> buttons;
	for (const GamepadFullState& pad : Pads)
		ReadButtons(pad, buttons[pad.Id]);
	LastButtons.swap(buttons);

	if (!active)
	{
		RawLeft = RawRight = 0.0f;
		return;
	}
	Buttons(engine, *active);
	Axes(engine, *active);
}

void DXControllerPad::Buttons(Engine* engine, const GamepadFullState& pad)
{
	std::array<bool, NumButtons> down;
	ReadButtons(pad, down);
	for (int i = 0; i < NumButtons; i++)
	{
		if (!down[i])
			HeldOver[i] = false;
		if (down[i] && !Held[i] && !HeldOver[i])
		{
			Held[i] = true;
			HeldKey[i] = ButtonKeys[i];
			if (HeldKey[i] != IK_None)
				engine->PadInputEvent(HeldKey[i], IST_Press, 0.0f);
		}
		else if (!down[i] && Held[i])
		{
			Held[i] = false;
			if (HeldKey[i] != IK_None)
				engine->PadInputEvent(HeldKey[i], IST_Release, 0.0f);
		}
	}
}

// One axis's value: sent every tick while not 0, and one 0 the first tick
// back at 0, which the mod's scripts read as the stick or trigger let go
void DXControllerPad::SendAxis(Engine* engine, EInputKey key, float value, bool& active)
{
	// The mark set before the event, which may reach a GamepadReload that
	// makes the axis map's lines again
	if (value != 0.0f)
	{
		active = true;
		engine->PadInputEvent(key, IST_Axis, value);
	}
	else if (active)
	{
		active = false;
		engine->PadInputEvent(key, IST_Axis, 0.0f);
	}
}

void DXControllerPad::Axes(Engine* engine, const GamepadFullState& pad)
{
	const float full = StickResponse::FullTravel;
	const float lx = pad.Axes[(int)GamepadAxis::LeftX] * full, ly = pad.Axes[(int)GamepadAxis::LeftY] * full;
	const float rx = pad.Axes[(int)GamepadAxis::RightX] * full, ry = pad.Axes[(int)GamepadAxis::RightY] * full;
	RawLeft = std::min(std::sqrt(lx * lx + ly * ly) / full, 1.0f);
	RawRight = std::min(std::sqrt(rx * rx + ry * ry) / full, 1.0f);

	float values[6];
	Left.Axes(lx, ly, values[0], values[1]);
	Right.Axes(rx, ry, values[2], values[3]);
	// The look inverted by the engine, as the mod's scripts expect: they
	// turn JoyV back for their own wheel and scrolling
	if (InvertLookY)
		values[3] = -values[3];
	values[4] = TriggerResponse(pad.Axes[(int)GamepadAxis::LeftTrigger] * full, TriggerThreshold);
	values[5] = TriggerResponse(pad.Axes[(int)GamepadAxis::RightTrigger] * full, TriggerThreshold);
	for (int i = 0; i < 6; i++)
		SendAxis(engine, AxisKeys[i], values[i], AxisActive[i]);

	// By index: a GamepadReload from a script the event reaches makes the
	// lines again
	for (size_t n = 0; n < AxisLines.size(); n++)
	{
		const AxisLine line = AxisLines[n];
		float source = 0.0f;
		switch (line.From)
		{
		case Source::GyroPitch: source = pad.Gyro[0]; break;
		case Source::GyroYaw: source = pad.Gyro[1]; break;
		case Source::GyroRoll: source = pad.Gyro[2]; break;
		case Source::AccelX: source = pad.Accel[0]; break;
		case Source::AccelY: source = pad.Accel[1]; break;
		case Source::AccelZ: source = pad.Accel[2]; break;
		// -1..1 across the touchpad while it is touched, up positive
		case Source::TouchX: source = pad.Touching ? pad.Touch[0] * 2.0f - 1.0f : 0.0f; break;
		case Source::TouchY: source = pad.Touching ? 1.0f - pad.Touch[1] * 2.0f : 0.0f; break;
		case Source::JoyAxis: source = line.JoyAxis < (int)pad.JoystickAxes.size() ? pad.JoystickAxes[line.JoyAxis] : 0.0f; break;
		}
		float value = std::fabs(source) <= line.Deadzone ? 0.0f : std::clamp(source * line.Scale, -1000.0f, 1000.0f);
		if (!std::isfinite(value))
			value = 0.0f;
		SendAxis(engine, line.Key, value, AxisLines[n].Active);
	}
}

void DXControllerPad::ReleaseAll(Engine* engine)
{
	// Each mark cleared before its event, so that a GamepadReload a script
	// sends from it releases nothing twice
	for (int i = 0; i < NumButtons; i++)
	{
		const bool held = Held[i];
		Held[i] = false;
		if (held && HeldKey[i] != IK_None)
			engine->PadInputEvent(HeldKey[i], IST_Release, 0.0f);
	}
	for (int i = 0; i < 6; i++)
	{
		const bool active = AxisActive[i];
		AxisActive[i] = false;
		if (active)
			engine->PadInputEvent(AxisKeys[i], IST_Axis, 0.0f);
	}
	for (size_t n = 0; n < AxisLines.size(); n++)
	{
		const bool active = AxisLines[n].Active;
		AxisLines[n].Active = false;
		if (active)
			engine->PadInputEvent(AxisLines[n].Key, IST_Axis, 0.0f);
	}
	RawLeft = RawRight = 0.0f;
}

std::string DXControllerPad::SampleCurve(const std::string& args) const
{
	Array<std::string> words;
	for (size_t pos = 0; pos < args.size();)
	{
		size_t start = args.find_first_not_of(" \t", pos);
		if (start == std::string::npos)
			break;
		size_t end = args.find_first_of(" \t", start);
		if (end == std::string::npos)
			end = args.size();
		words.push_back(args.substr(start, end - start));
		pos = end;
	}
	if (words.empty())
		return {};
	const StickResponse* stick = StrTools::equals_ignore_case(words[0], "left") ? &Left : StrTools::equals_ignore_case(words[0], "right") ? &Right : nullptr;
	if (!stick)
		return {};
	const int count = std::clamp(words.size() > 1 ? std::atoi(words[1].c_str()) : 0, 2, 256);
	std::string text;
	for (int i = 0; i < count; i++)
	{
		if (i > 0)
			text += ",";
		const float x = (float)i / (float)(count - 1);
		text += Format("%.4f", std::clamp(stick->Magnitude(x * StickResponse::FullTravel), 0.0f, 1.0f));
	}
	return text;
}

std::string DXControllerPad::RawMagnitudes() const
{
	return "L=" + Format("%.4f", RawLeft) + " R=" + Format("%.4f", RawRight);
}

std::string DXControllerPad::PadInfo() const
{
	return HasActive ? ActiveType : std::string("None");
}
