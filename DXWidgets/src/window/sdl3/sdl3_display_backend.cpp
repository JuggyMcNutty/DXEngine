#include "sdl3_display_backend.h"
#include "sdl3_display_window.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_joystick.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

SDL3DisplayBackend::SDL3DisplayBackend()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD))
		throw std::runtime_error(std::string("Unable to initialize SDL:") + SDL_GetError());

	SDL3DisplayWindow::PaintEventNumber = SDL_RegisterEvents(1);

	// Each pad open as a gamepad, as SDL2's window opens its game
	// controllers: SDL sends a gamepad's button events only for one open.
	int count = 0;
	SDL_JoystickID* ids = SDL_GetGamepads(&count);
	for (int i = 0; ids && i < count; i++)
		SDL_OpenGamepad(ids[i]);
	SDL_free(ids);
}

bool SDL3DisplayBackend::GetGamepadState(GamepadState& state)
{
	state = {};

	// Controllers can be plugged in or out at any time; keep the first
	// attached one, and look again when it goes away. Opening is
	// reference-counted, so reopening one the constructor opened is harmless.
	if (Gamepad && !SDL_GamepadConnected(Gamepad))
	{
		SDL_CloseGamepad(Gamepad);
		Gamepad = nullptr;
	}
	if (!Gamepad)
	{
		int count = 0;
		SDL_JoystickID* ids = SDL_GetGamepads(&count);
		if (ids && count > 0)
			Gamepad = SDL_OpenGamepad(ids[0]);
		SDL_free(ids);
	}
	if (!Gamepad)
		return false;

	static const SDL_GamepadAxis axes[(int)GamepadAxis::Count] =
	{
		SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
		SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
		SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
	};
	for (int i = 0; i < (int)GamepadAxis::Count; i++)
	{
		float v = SDL_GetGamepadAxis(Gamepad, axes[i]) / 32767.0f;
		state.Axes[i] = v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
	}

	// GamepadButton's order is SDL's: south, east, west and north its A, B,
	// X and Y.
	static const SDL_GamepadButton buttons[(int)GamepadButton::Count] =
	{
		SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
		SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
		SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
		SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	};
	for (int i = 0; i < (int)GamepadButton::Count; i++)
		state.Buttons[i] = SDL_GetGamepadButton(Gamepad, buttons[i]);

	state.Connected = true;
	return true;
}

void SDL3DisplayBackend::SetGamepadKeyEmulation(bool enable)
{
	SDL3DisplayWindow::GamepadKeyEmulation = enable;
}

bool SDL3DisplayBackend::SimulateKey(const std::string& name, bool down)
{
	SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
	if (scancode == SDL_SCANCODE_UNKNOWN || SDL3DisplayWindow::WindowList.empty())
		return false;
	SDL_Event event = {};
	event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	event.key.windowID = SDL3DisplayWindow::WindowList.begin()->first;
	event.key.scancode = scancode;
	event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
	event.key.down = down;
	return SDL_PushEvent(&event);
}

// The timeline's pad: a virtual gamepad of SDL's, its axes and buttons in
// SDL's gamepad order (GamepadAxis's and GamepadButton's), named by the
// vendor and product ids a run's SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT
// can keep alone.
SDL_Joystick* SDL3DisplayBackend::VirtualPad()
{
	if (Virtual)
		return Virtual;
	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.vendor_id = 0x1209;
	desc.product_id = 0xd0d1;
	desc.naxes = (Uint16)GamepadAxis::Count;
	desc.nbuttons = (Uint16)GamepadButton::Count;
	desc.axis_mask = (1u << (int)GamepadAxis::Count) - 1;
	desc.button_mask = (1u << (int)GamepadButton::Count) - 1;
	desc.name = "DXEngine's timeline pad";
	SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
	if (id)
		Virtual = SDL_OpenJoystick(id);
	return Virtual;
}

bool SDL3DisplayBackend::SimulateGamepadButton(GamepadButton button, bool down)
{
	SDL_Joystick* pad = VirtualPad();
	return pad && SDL_SetJoystickVirtualButton(pad, (int)button, down);
}

bool SDL3DisplayBackend::SimulateGamepadAxis(GamepadAxis axis, float value)
{
	SDL_Joystick* pad = VirtualPad();
	if (!pad)
		return false;
	// A trigger's axis is the whole range, the released at its least.
	bool trigger = axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger;
	float v = trigger ? std::fmin(std::fmax(value, 0.0f), 1.0f) * 2.0f - 1.0f : std::fmin(std::fmax(value, -1.0f), 1.0f);
	Sint16 raw = (Sint16)std::lround(v < 0.0f ? v * 32768.0f : v * 32767.0f);
	return SDL_SetJoystickVirtualAxis(pad, (int)axis, raw);
}

std::unique_ptr<DisplayWindow> SDL3DisplayBackend::Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<SDL3DisplayWindow>(windowHost, type, static_cast<SDL3DisplayWindow*>(owner), renderAPI, UIScale);
}

void SDL3DisplayBackend::ProcessEvents()
{
	SDL3DisplayWindow::ProcessEvents();
}

void SDL3DisplayBackend::RunLoop()
{
	SDL3DisplayWindow::RunLoop();
}

void SDL3DisplayBackend::ExitLoop()
{
	SDL3DisplayWindow::ExitLoop();
}

Size SDL3DisplayBackend::GetScreenSize()
{
	SDL_Rect rect = {};
	SDL_DisplayID *displays = SDL_GetDisplays(nullptr);
	if (!displays || !SDL_GetDisplayBounds(displays[0], &rect))
		throw std::runtime_error(std::string("Unable to get screen size:") + SDL_GetError());
	SDL_free(displays);

	return {rect.w / UIScale, rect.h / UIScale};
}

std::vector<Size> SDL3DisplayBackend::GetFullscreenModes()
{
	std::vector<Size> modes;
	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	int count = 0;
	if (SDL_DisplayMode** list = SDL_GetFullscreenDisplayModes(display, &count))
	{
		for (int i = 0; i < count; i++)
		{
			Size size(list[i]->w, list[i]->h);
			if (std::find(modes.begin(), modes.end(), size) == modes.end())
				modes.push_back(size);
		}
		SDL_free(list);
	}
	if (modes.empty())
	{
		if (const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display))
			modes.push_back(Size(desktop->w, desktop->h));
	}
	return modes;
}

void* SDL3DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return SDL3DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void SDL3DisplayBackend::StopTimer(void* timerID)
{
	SDL3DisplayWindow::StopTimer(timerID);
}
