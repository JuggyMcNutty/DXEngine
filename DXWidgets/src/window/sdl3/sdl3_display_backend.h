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
	std::vector<Size> GetFullscreenModes() override;

	bool GetGamepadState(GamepadState& state) override;
	void SetGamepadKeyEmulation(bool enable) override;
	bool SimulateKey(const std::string& name, bool down) override;
	bool SimulateGamepadButton(GamepadButton button, bool down) override;
	bool SimulateGamepadAxis(GamepadAxis axis, float value) override;

	bool IsSDL3() override { return true; }

private:
	SDL_Gamepad* Gamepad = nullptr;
	SDL_Joystick* VirtualPad();
	SDL_Joystick* Virtual = nullptr;
};
