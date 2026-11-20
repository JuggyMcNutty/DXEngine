#pragma once

#include <SDL2/SDL_gamecontroller.h>

#include "window/window.h"

class SDL2DisplayBackend : public DisplayBackend
{
public:
	SDL2DisplayBackend();

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

	bool IsSDL2() override { return true; }

private:
	double UIScale = 1.0;
	SDL_GameController* Gamepad = nullptr;
	// GetGamepads' pads, each with the sensors switched on on it
	struct OpenPad { SDL_GameController* Pad; bool Gyro; bool Accel; };
	std::vector<OpenPad> Pads;
	bool WantGyro = false;
	bool WantAccel = false;
	SDL_Joystick* VirtualPad();
	SDL_Joystick* Virtual = nullptr;
};
