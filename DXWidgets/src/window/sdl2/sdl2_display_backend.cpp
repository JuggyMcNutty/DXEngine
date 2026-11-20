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

#ifndef WIN32
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>

// In a rootless container (podman's, so distrobox's and toolbox's) the
// host's root is not mapped into the user namespace: what it owns shows as
// the overflow uid, and libudev's monitor drops each message from a sender
// other than uid 0, so SDL's udev enumeration never hears of a pad plugged
// in after the start. SDL watches /dev/input itself only in a Flatpak or a
// Snap (SDL_GetSandbox); there it is asked to (SDL_JOYSTICK_DISABLE_UDEV).
static bool HostRootUnmapped()
{
	struct stat st;
	if (stat("/dev/input", &st) != 0)
		return false;
	unsigned long overflow = 65534;
	if (FILE* file = fopen("/proc/sys/kernel/overflowuid", "r"))
	{
		if (fscanf(file, "%lu", &overflow) != 1)
			overflow = 65534;
		fclose(file);
	}
	return st.st_uid == overflow;
}
#endif

SDL2DisplayBackend::SDL2DisplayBackend()
{
#ifndef WIN32
	if (HostRootUnmapped())
		SDL_SetHint("SDL_JOYSTICK_DISABLE_UDEV", "1");
#endif
	// A pad's face buttons by their places, as SDL3 reads every pad and the
	// launcher promises (Joy1 the bottom one): SDL2 reads a Nintendo-layout
	// pad's by their letters unless told (bugs.md WIN.49)
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
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

// A pad's sticks, triggers and buttons
static void ReadGamepad(SDL_GameController* pad, GamepadState& state)
{
	static const SDL_GameControllerAxis axes[(int)GamepadAxis::Count] =
	{
		SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY,
		SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY,
		SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT
	};
	for (int i = 0; i < (int)GamepadAxis::Count; i++)
	{
		float v = SDL_GameControllerGetAxis(pad, axes[i]) / 32767.0f;
		state.Axes[i] = v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
	}

	// GamepadButton follows SDL's own order, so the index carries over.
	for (int i = 0; i < (int)GamepadButton::Count; i++)
		state.Buttons[i] = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i) != 0;

	state.Connected = true;
}

// SDL2's controller types by the names SDL3's take; Amazon's, Google's and
// NVIDIA's pads SDL3 counts as standard ones
static const char* GamepadTypeName(SDL_GameControllerType type)
{
	switch (type)
	{
	case SDL_CONTROLLER_TYPE_XBOX360: return "Xbox360";
	case SDL_CONTROLLER_TYPE_XBOXONE: return "XboxOne";
	case SDL_CONTROLLER_TYPE_PS3: return "PS3";
	case SDL_CONTROLLER_TYPE_PS4: return "PS4";
	case SDL_CONTROLLER_TYPE_PS5: return "PS5";
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "SwitchPro";
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "JoyconLeft";
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "JoyconRight";
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "JoyconPair";
	case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
	case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
	case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD: return "Standard";
	default: return "Unknown";
	}
}

bool SDL2DisplayBackend::GetGamepadState(GamepadState& state)
{
	state = {};

	// Controllers can be plugged in or out at any time; keep the first
	// attached one, and look again when it goes away. One the constructor
	// opened is taken as it is, so that its one close here as it goes frees
	// it (opening is reference-counted; bugs.md WIN.57); one plugged in
	// later is opened here.
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
			{
				Gamepad = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
				if (!Gamepad)
					Gamepad = SDL_GameControllerOpen(i);
			}
		}
	}
	if (!Gamepad)
		return false;

	ReadGamepad(Gamepad, state);
	return true;
}

// Every attached pad (DXEngine's DXController mode): one gone closed, once,
// which frees one the constructor opened too (bugs.md WIN.57); one not open
// yet opened; the sensors asked for switched on as each is first seen.
// SDL2 has no buttons past the touchpad's, Misc2 to Misc6 never down.
void SDL2DisplayBackend::GetGamepads(std::vector<GamepadFullState>& pads)
{
	pads.clear();
	for (auto it = Pads.begin(); it != Pads.end();)
	{
		if (!SDL_GameControllerGetAttached(it->Pad))
		{
			if (Gamepad == it->Pad)
				Gamepad = nullptr;
			SDL_GameControllerClose(it->Pad);
			it = Pads.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if (!SDL_IsGameController(i))
			continue;
		SDL_GameController* pad = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
		if (!pad)
			pad = SDL_GameControllerOpen(i);
		if (!pad)
			continue;
		auto open = std::find_if(Pads.begin(), Pads.end(), [&](const OpenPad& p) { return p.Pad == pad; });
		if (open == Pads.end())
		{
			Pads.push_back({ pad, false, false });
			open = Pads.end() - 1;
		}
		if (open->Gyro != WantGyro)
		{
			if (SDL_GameControllerHasSensor(pad, SDL_SENSOR_GYRO))
				SDL_GameControllerSetSensorEnabled(pad, SDL_SENSOR_GYRO, WantGyro ? SDL_TRUE : SDL_FALSE);
			open->Gyro = WantGyro;
		}
		if (open->Accel != WantAccel)
		{
			if (SDL_GameControllerHasSensor(pad, SDL_SENSOR_ACCEL))
				SDL_GameControllerSetSensorEnabled(pad, SDL_SENSOR_ACCEL, WantAccel ? SDL_TRUE : SDL_FALSE);
			open->Accel = WantAccel;
		}

		GamepadFullState state;
		ReadGamepad(pad, state);
		SDL_Joystick* joystick = SDL_GameControllerGetJoystick(pad);
		state.Id = joystick ? (unsigned int)SDL_JoystickInstanceID(joystick) : 0u;
		state.Type = GamepadTypeName(SDL_GameControllerGetType(pad));
		// SDL2's buttons from MISC1 to TOUCHPAD are GamepadExtraButton's first six
		for (int b = 0; b <= (int)GamepadExtraButton::Touchpad; b++)
			state.Extra[b] = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)(SDL_CONTROLLER_BUTTON_MISC1 + b)) != 0;
		if (WantGyro && SDL_GameControllerHasSensor(pad, SDL_SENSOR_GYRO))
			SDL_GameControllerGetSensorData(pad, SDL_SENSOR_GYRO, state.Gyro, 3);
		if (WantAccel && SDL_GameControllerHasSensor(pad, SDL_SENSOR_ACCEL))
			SDL_GameControllerGetSensorData(pad, SDL_SENSOR_ACCEL, state.Accel, 3);
		if (SDL_GameControllerGetNumTouchpads(pad) > 0)
		{
			Uint8 down = 0;
			float x = 0.0f, y = 0.0f, pressure = 0.0f;
			if (SDL_GameControllerGetTouchpadFinger(pad, 0, 0, &down, &x, &y, &pressure) == 0 && down)
			{
				state.Touching = true;
				state.Touch[0] = x;
				state.Touch[1] = y;
			}
		}
		if (joystick)
		{
			int axes = SDL_JoystickNumAxes(joystick);
			for (int a = 0; a < axes; a++)
				state.JoystickAxes.push_back(std::clamp(SDL_JoystickGetAxis(joystick, a) / 32767.0f, -1.0f, 1.0f));
		}
		pads.push_back(std::move(state));
	}
}

void SDL2DisplayBackend::SetGamepadSensors(bool gyro, bool accel)
{
	WantGyro = gyro;
	WantAccel = accel;
}

void SDL2DisplayBackend::SetGamepadKeyEmulation(bool enable)
{
	SDL2DisplayWindow::GamepadKeyEmulation = enable;
}

// The key's event as the keyboard's: its modifiers those the timeline holds
// and its locks, Num Lock and Caps Lock toggled by their presses.
bool SDL2DisplayBackend::SimulateKey(const std::string& name, bool down)
{
	SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
	if (scancode == SDL_SCANCODE_UNKNOWN || SDL2DisplayWindow::WindowList.empty())
		return false;
	if (down && !SDL2DisplayWindow::SimulatedKeys[scancode])
	{
		if (scancode == SDL_SCANCODE_NUMLOCKCLEAR)
			SDL2DisplayWindow::SimulatedNumLock = !SDL2DisplayWindow::SimulatedNumLock;
		else if (scancode == SDL_SCANCODE_CAPSLOCK)
			SDL2DisplayWindow::SimulatedCapsLock = !SDL2DisplayWindow::SimulatedCapsLock;
	}
	SDL2DisplayWindow::SimulatedKeys[scancode] = down;

	static const struct { SDL_Scancode key; int mod; } modifiers[] = {
		{ SDL_SCANCODE_LSHIFT, KMOD_LSHIFT }, { SDL_SCANCODE_RSHIFT, KMOD_RSHIFT },
		{ SDL_SCANCODE_LCTRL, KMOD_LCTRL }, { SDL_SCANCODE_RCTRL, KMOD_RCTRL },
		{ SDL_SCANCODE_LALT, KMOD_LALT }, { SDL_SCANCODE_RALT, KMOD_RALT },
		{ SDL_SCANCODE_LGUI, KMOD_LGUI }, { SDL_SCANCODE_RGUI, KMOD_RGUI },
	};
	int mod = KMOD_NONE;
	for (const auto& m : modifiers)
		if (SDL2DisplayWindow::SimulatedKeys[m.key])
			mod |= m.mod;
	if (SDL2DisplayWindow::SimulatedNumLock)
		mod |= KMOD_NUM;
	if (SDL2DisplayWindow::SimulatedCapsLock)
		mod |= KMOD_CAPS;

	SDL_Event event = {};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.windowID = SDL2DisplayWindow::WindowList.begin()->first;
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.unused = SDL2DisplayWindow::SimulatedKeyMark;
	event.key.keysym.scancode = scancode;
	event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
	event.key.keysym.mod = (Uint16)mod;
	return SDL_PushEvent(&event) == 1;
}

bool SDL2DisplayBackend::SimulateKeyLayout(const std::string& family)
{
	if (!family.empty() && family != "QWERTY" && family != "QWERTZ" && family != "AZERTY" && family != "Dvorak" && family != "Swedish" && family != "ABNT" && family != "JP106")
		return false;
	SDL2DisplayWindow::SimulatedLayout = family;
	return true;
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

// The display the windows are on, whose modes full screen takes
// (SDL2DisplayWindow::ApplyFullscreen, ShowFullscreen): a window's, or
// display 0, the primary, before there is one
static int WindowsDisplay()
{
	for (const auto& it : SDL2DisplayWindow::WindowList)
	{
		int display = SDL_GetWindowDisplayIndex(it.second->Handle.window);
		if (display >= 0)
			return display;
	}
	return 0;
}

Size SDL2DisplayBackend::GetScreenSize()
{
	SDL_Rect rect = {};
	int result = SDL_GetDisplayBounds(WindowsDisplay(), &rect);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to get screen size:") + SDL_GetError());

	return Size(rect.w / UIScale, rect.h / UIScale);
}

// That display's usable bounds (X11's _NET_WORKAREA, Windows' work area),
// in DIPs as GetScreenSize's
Rect SDL2DisplayBackend::GetScreenWorkArea()
{
	SDL_Rect rect = {};
	if (SDL_GetDisplayUsableBounds(WindowsDisplay(), &rect) != 0)
		return DisplayBackend::GetScreenWorkArea();
	return Rect::xywh(rect.x / UIScale, rect.y / UIScale, rect.w / UIScale, rect.h / UIScale);
}

std::vector<Size> SDL2DisplayBackend::GetFullscreenModes()
{
	std::vector<Size> modes;
	int display = WindowsDisplay();
#ifndef DXE_SDL2_BORDERLESS_FULLSCREEN
	int count = SDL_GetNumDisplayModes(display);
	for (int i = 0; i < count; i++)
	{
		SDL_DisplayMode mode = {};
		if (SDL_GetDisplayMode(display, i, &mode) != 0)
			continue;
		Size size(mode.w, mode.h);
		if (std::find(modes.begin(), modes.end(), size) == modes.end())
			modes.push_back(size);
	}
#endif
	if (modes.empty())
	{
		SDL_DisplayMode desktop = {};
		if (SDL_GetDesktopDisplayMode(display, &desktop) == 0)
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
