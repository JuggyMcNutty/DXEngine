#pragma once

#include "GameWindow.h"
#include "DXControllerStick.h"
#include <dxwidgets/window/window.h>
#include <array>
#include <map>
#include <string>
#include <vector>

class Engine;
class PackageManager;

// The pad's DXController mode (Settings.json's Gamepad.DXController;
// GamepadInput): the pad as the DXController mod's scripts take it, each
// event through the engine's ordinary path to the console's KeyEvent first
// (Engine::PadInputEvent), with no menu translation of its own.
//
//  - Buttons, a press and a release as each changes: by default A, B, X, Y
//    Joy1 to Joy4, the shoulders Joy5 and Joy6, Back and Start Joy7 and
//    Joy8, the stick clicks Joy9 and Joy10, the four paddles Joy11 to Joy14,
//    Misc1 Joy15, the touchpad's click Joy16, the d-pad the four JoyPov
//    keys, Guide and Misc2 to Misc6 nothing; [DXController.GamepadButtonMap]
//    changes any.
//  - Axes, -1000..1000, up positive: the left stick JoyX and JoyY, the right
//    one JoyU and JoyV, the triggers JoyZ and JoyR (axes alone, never
//    buttons), each through [DXController.ControllerSettings]' dead zone,
//    curve, scale and threshold; [DXController.GamepadAxisMap]'s sources onto
//    keys of their own. A value not 0 goes every tick; the first tick back
//    at 0 sends one 0, never a release.
//  - One pad's events at a time, the active one's: the first pad, until a
//    button or a stick pushed past a quarter of its travel on another makes
//    that one active. Every button held is released and every axis not at 0
//    sent its 0 before a switch, as its pad goes, at a focus loss and at a
//    GamepadReload.
//  - The console's GamepadReload, GamepadSampleCurve, GamepadGetRawMag and
//    GamepadGetInfo (Engine::WindowExec), which the mod's scripts send.
class DXControllerPad
{
public:
	// The three sections read from the system ini, the settings' and the
	// button map's missing keys written back: at the engine's start, before
	// the mod's classes read their config, and at a GamepadReload
	void LoadIni(PackageManager* packages);
	// A tick with the window's focus: the pads read, the active one's events
	// sent
	void Update(Engine* engine);
	// Every button held released, every axis not at 0 sent its 0
	void ReleaseAll(Engine* engine);
	// GamepadReload: everything released, the sections read again
	void Reload(Engine* engine, PackageManager* packages);

	// GamepadSampleCurve Left|Right N: N values from 2 to 256 of the stick's
	// output, 0..1, at even steps of its travel from 0 to full, "%.4f" each,
	// comma-separated
	std::string SampleCurve(const std::string& args) const;
	// GamepadGetRawMag: "L=%.4f R=%.4f", the active pad's sticks' distances
	// from their centres as last read, 0..1
	std::string RawMagnitudes() const;
	// GamepadGetInfo: the active pad's family (GamepadFullState::Type), or
	// None
	std::string PadInfo() const;

	enum { NumButtons = (int)GamepadButton::Count + (int)GamepadExtraButton::Count };

private:
	enum class Source { GyroPitch, GyroYaw, GyroRoll, AccelX, AccelY, AccelZ, TouchX, TouchY, JoyAxis };
	struct AxisLine
	{
		Source From = Source::JoyAxis;
		int JoyAxis = 0;
		EInputKey Key = IK_None;
		float Scale = 1000.0f;
		float Deadzone = 0.0f;
		bool Active = false;
	};

	void LoadSettings(PackageManager* packages, bool& written);
	void LoadButtonMap(PackageManager* packages, bool& written);
	void LoadAxisMap(PackageManager* packages);
	void Buttons(Engine* engine, const GamepadFullState& pad);
	void Axes(Engine* engine, const GamepadFullState& pad);
	void SendAxis(Engine* engine, EInputKey key, float value, bool& active);
	static bool OtherPadWantsIn(const GamepadFullState& pad, const std::array<bool, NumButtons>& before);
	static void ReadButtons(const GamepadFullState& pad, std::array<bool, NumButtons>& down);

	StickResponse Left, Right;
	int TriggerThreshold = 30;
	bool InvertLookY = false;

	EInputKey ButtonKeys[NumButtons] = {};
	std::vector<AxisLine> AxisLines;
	bool WantGyro = false, WantAccel = false;
	bool SensorsSet = false, SensorsGyro = false, SensorsAccel = false;

	// The active pad, by the backend's number
	bool HasActive = false;
	unsigned int ActiveId = 0;
	std::string ActiveType;
	// Each attached pad's buttons as last read, so a press on another pad is
	// told from a hold
	std::map<unsigned int, std::array<bool, NumButtons>> LastButtons;

	// The active pad's buttons held, each with the key its press sent
	bool Held[NumButtons] = {};
	EInputKey HeldKey[NumButtons] = {};
	// Buttons held through a GamepadReload, released by it and not pressed
	// again until they come up: the mod's settings call GamepadReload from
	// the press that changes a value, which would else change it each tick
	bool HeldOver[NumButtons] = {};
	// JoyX, JoyY, JoyU, JoyV, JoyZ, JoyR not at 0
	bool AxisActive[6] = {};

	float RawLeft = 0.0f, RawRight = 0.0f;
	std::vector<GamepadFullState> Pads;
};
