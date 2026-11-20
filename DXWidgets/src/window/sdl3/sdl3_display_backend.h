#pragma once

#include <SDL3/SDL_gamepad.h>

#include "dxwidgets/window/window.h"

class SDL3DisplayBackend : public DisplayBackend
{
public:
	SDL3DisplayBackend();

	std::unique_ptr<DisplayWindow> Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI) override;
	void ProcessEvents() override;
	void RunLoop() override;
	void ExitLoop() override;

	void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer) override;
	void StopTimer(void* timerID) override;

	Size GetScreenSize() override;
	Rect GetScreenWorkArea() override;
	std::vector<Size> GetFullscreenModes() override;

	bool GetGamepadState(GamepadState& state) override;
	void GetGamepads(std::vector<GamepadFullState>& pads) override;
	void SetGamepadSensors(bool gyro, bool accel) override;
	void SetGamepadKeyEmulation(bool enable) override;
	bool SimulateKey(const std::string& name, bool down) override;
	bool SimulateKeyLayout(const std::string& family) override;
	bool SimulateGamepadButton(GamepadButton button, bool down) override;
	bool SimulateGamepadAxis(GamepadAxis axis, float value) override;

	bool IsSDL3() override { return true; }

private:
	SDL_Gamepad* Gamepad = nullptr;
	// GetGamepads' pads, each with the sensors switched on on it
	struct OpenPad { SDL_Gamepad* Pad; bool Gyro; bool Accel; };
	std::vector<OpenPad> Pads;
	bool WantGyro = false;
	bool WantAccel = false;
	SDL_Joystick* VirtualPad();
	SDL_Joystick* Virtual = nullptr;
};
