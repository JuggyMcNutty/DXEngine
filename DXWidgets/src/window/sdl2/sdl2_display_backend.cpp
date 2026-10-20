#include "sdl2_display_backend.h"
#include "sdl2_display_window.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <SDL2/SDL_video.h>
#ifndef WIN32
#include <dlfcn.h>
#include <cstring>
#endif

namespace X11DPI
{
	typedef struct _XDisplay Display;
	typedef Display *(*XOpenDisplayPtr)(const char*);
	typedef char *(*XGetDefaultPtr)(Display*, const char*, const char*);
	typedef int (*XCloseDisplayPtr)(Display*);
}

SDL2DisplayBackend::SDL2DisplayBackend()
{
	int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to initialize SDL:") + SDL_GetError());

	SDL2DisplayWindow::PaintEventNumber = SDL_RegisterEvents(1);

	// SDL2 doesn't have proper native hidpi support for Linux. Cheat a bit here by asking X11 ourselves.
#ifndef WIN32
	const char* driver = SDL_GetCurrentVideoDriver();
	if (driver && strcmp(driver, "x11") == 0)
	{
		void* module = dlopen("libX11.so", RTLD_NOW | RTLD_LOCAL);
		if (module)
		{
			using namespace X11DPI;
			auto XOpenDisplay = (XOpenDisplayPtr)dlsym(module, "XOpenDisplay");
			auto XGetDefault = (XGetDefaultPtr)dlsym(module, "XGetDefault");
			auto XCloseDisplay = (XCloseDisplayPtr)dlsym(module, "XCloseDisplay");
			if (XOpenDisplay && XGetDefault && XCloseDisplay)
			{
				auto display = XOpenDisplay(nullptr);
				if (display)
				{
					char* value = XGetDefault(display, "Xft", "dpi");
					if (value)
					{
						int dpi = std::atoi(value);
						if (dpi != 0)
						{
							UIScale = dpi / 96.0;
						}
					}
					XCloseDisplay(display);
				}
			}
		}
	}
#endif

	for (auto i = 0; i < SDL_NumJoysticks(); i++)
	{
		SDL_GameControllerOpen(i);
	}
}

bool SDL2DisplayBackend::GetGamepadState(GamepadState& state)
{
	state = {};

	// Controllers can be plugged in or out at any time; keep the first
	// attached one, and look again when it goes away. Opening is
	// reference-counted, so reopening one the constructor opened is harmless.
	if (Gamepad && !SDL_GameControllerGetAttached(Gamepad))
	{
		SDL_GameControllerClose(Gamepad);
		Gamepad = nullptr;
	}
	if (!Gamepad)
	{
		for (int i = 0; i < SDL_NumJoysticks() && !Gamepad; i++)
		{
			if (SDL_IsGameController(i))
				Gamepad = SDL_GameControllerOpen(i);
		}
	}
	if (!Gamepad)
		return false;

	static const SDL_GameControllerAxis axes[(int)GamepadAxis::Count] =
	{
		SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY,
		SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY,
		SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT
	};
	for (int i = 0; i < (int)GamepadAxis::Count; i++)
	{
		float v = SDL_GameControllerGetAxis(Gamepad, axes[i]) / 32767.0f;
		state.Axes[i] = v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
	}

	// GamepadButton follows SDL's own order, so the index carries over.
	for (int i = 0; i < (int)GamepadButton::Count; i++)
		state.Buttons[i] = SDL_GameControllerGetButton(Gamepad, (SDL_GameControllerButton)i) != 0;

	state.Connected = true;
	return true;
}

void SDL2DisplayBackend::SetGamepadKeyEmulation(bool enable)
{
	SDL2DisplayWindow::GamepadKeyEmulation = enable;
}

bool SDL2DisplayBackend::SimulateKey(const std::string& name, bool down)
{
	SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
	if (scancode == SDL_SCANCODE_UNKNOWN || SDL2DisplayWindow::WindowList.empty())
		return false;
	SDL_Event event = {};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.windowID = SDL2DisplayWindow::WindowList.begin()->first;
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.scancode = scancode;
	event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
	return SDL_PushEvent(&event) == 1;
}

// The timeline's pad: a virtual game controller of SDL's, its axes and
// buttons in SDL's order (GamepadAxis's and GamepadButton's), named by the
// vendor and product ids a run's SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT
// can keep alone.
SDL_Joystick* SDL2DisplayBackend::VirtualPad()
{
	if (Virtual)
		return Virtual;
	SDL_VirtualJoystickDesc desc = {};
	desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	desc.vendor_id = 0x1209;
	desc.product_id = 0xd0d1;
	desc.naxes = (Uint16)GamepadAxis::Count;
	desc.nbuttons = (Uint16)GamepadButton::Count;
	desc.axis_mask = (1u << (int)GamepadAxis::Count) - 1;
	desc.button_mask = (1u << (int)GamepadButton::Count) - 1;
	desc.name = "DXEngine's timeline pad";
	int index = SDL_JoystickAttachVirtualEx(&desc);
	if (index >= 0)
		Virtual = SDL_JoystickOpen(index);
	return Virtual;
}

bool SDL2DisplayBackend::SimulateGamepadButton(GamepadButton button, bool down)
{
	SDL_Joystick* pad = VirtualPad();
	return pad && SDL_JoystickSetVirtualButton(pad, (int)button, down ? SDL_PRESSED : SDL_RELEASED) == 0;
}

bool SDL2DisplayBackend::SimulateGamepadAxis(GamepadAxis axis, float value)
{
	SDL_Joystick* pad = VirtualPad();
	if (!pad)
		return false;
	// A trigger's axis is the whole range, the released at its least.
	bool trigger = axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger;
	float v = trigger ? std::fmin(std::fmax(value, 0.0f), 1.0f) * 2.0f - 1.0f : std::fmin(std::fmax(value, -1.0f), 1.0f);
	Sint16 raw = (Sint16)std::lround(v < 0.0f ? v * 32768.0f : v * 32767.0f);
	return SDL_JoystickSetVirtualAxis(pad, (int)axis, raw) == 0;
}

std::unique_ptr<DisplayWindow> SDL2DisplayBackend::Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<SDL2DisplayWindow>(windowHost, type, static_cast<SDL2DisplayWindow*>(owner), renderAPI, UIScale);
}

void SDL2DisplayBackend::ProcessEvents()
{
	SDL2DisplayWindow::ProcessEvents();
}

void SDL2DisplayBackend::RunLoop()
{
	SDL2DisplayWindow::RunLoop();
}

void SDL2DisplayBackend::ExitLoop()
{
	SDL2DisplayWindow::ExitLoop();
}

Size SDL2DisplayBackend::GetScreenSize()
{
	SDL_Rect rect = {};
	int result = SDL_GetDisplayBounds(0, &rect);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to get screen size:") + SDL_GetError());

	return Size(rect.w / UIScale, rect.h / UIScale);
}

std::vector<Size> SDL2DisplayBackend::GetFullscreenModes()
{
	std::vector<Size> modes;
#ifndef DXE_SDL2_BORDERLESS_FULLSCREEN
	int count = SDL_GetNumDisplayModes(0);
	for (int i = 0; i < count; i++)
	{
		SDL_DisplayMode mode = {};
		if (SDL_GetDisplayMode(0, i, &mode) != 0)
			continue;
		Size size(mode.w, mode.h);
		if (std::find(modes.begin(), modes.end(), size) == modes.end())
			modes.push_back(size);
	}
#endif
	if (modes.empty())
	{
		SDL_DisplayMode desktop = {};
		if (SDL_GetDesktopDisplayMode(0, &desktop) == 0)
			modes.push_back(Size(desktop.w, desktop.h));
	}
	return modes;
}

void* SDL2DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return SDL2DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void SDL2DisplayBackend::StopTimer(void* timerID)
{
	SDL2DisplayWindow::StopTimer(timerID);
}
