#include "sdl3_display_window.h"
#include <stdexcept>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_mouse.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "dxwidgets/core/image.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

Uint32 SDL3DisplayWindow::PaintEventNumber = 0xffffffff;
bool SDL3DisplayWindow::ExitRunLoop;
bool SDL3DisplayWindow::GamepadKeyEmulation = true;
bool SDL3DisplayWindow::SimulatedKeys[SDL_SCANCODE_COUNT];
bool SDL3DisplayWindow::SimulatedNumLock = true;
bool SDL3DisplayWindow::SimulatedCapsLock = false;
std::string SDL3DisplayWindow::SimulatedLayout;
InputKey SDL3DisplayWindow::PressedAs[SDL_SCANCODE_COUNT];
std::unordered_map<unsigned int, SDL3DisplayWindow*> SDL3DisplayWindow::WindowList;

#ifdef _WIN32
// Maximize -- the caption's box, a double click on the caption, the window
// menu's, Wine's for its window manager's -- as Windows asks it of the
// window, before it is maximized: the host's to take, the game's window
// toggling full screen and never maximized, as the original's takes the
// command (dx-reverse-info/windrv-dll.md, the window's commands), as the
// Win32 window takes it; a widget window's goes on to SDL and Windows. SDL
// itself tells of a maximize only once it is done (OnWindowEvent). SDL
// leaves a procedure the program puts over its own in place
static LRESULT CALLBACK MaximizeCommandProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	auto window = (SDL3DisplayWindow*)GetPropW(hwnd, L"DXWidgets.SDL3DisplayWindow");
	if (!window || !window->SDLWindowProc)
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	if (msg == WM_SYSCOMMAND && (wparam & 0xFFF0) == SC_MAXIMIZE && window->WindowHost->OnWindowMaximize())
		return 0;
	return CallWindowProcW(reinterpret_cast<WNDPROC>(window->SDLWindowProc), hwnd, msg, wparam, lparam);
}
#endif

SDL3DisplayWindow::SDL3DisplayWindow(DisplayWindowHost* windowHost, WidgetType type, SDL3DisplayWindow* owner, RenderAPI renderAPI) : WindowHost(windowHost), SystemKeys(renderAPI == RenderAPI::Unspecified)
{
	m_GLIsES = renderAPI == RenderAPI::GLES;

	unsigned int flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (renderAPI == RenderAPI::Vulkan)
		flags |= SDL_WINDOW_VULKAN;
	else if (renderAPI == RenderAPI::OpenGL || renderAPI == RenderAPI::GLES)
		flags |= SDL_WINDOW_OPENGL;
	if (type == WidgetType::Popup)
		flags |= SDL_WINDOW_BORDERLESS;

	// A device's window is a plain one, D3D11's too, whose swap chain is on
	// its HWND; SDL's renderer is for a window that presents a bitmap.
	if (renderAPI == RenderAPI::Vulkan || renderAPI == RenderAPI::OpenGL || renderAPI == RenderAPI::GLES || renderAPI == RenderAPI::D3D11)
	{
		Handle.window = SDL_CreateWindow("", 320, 200, flags);
		if (!Handle.window)
			throw std::runtime_error(std::string("Unable to create SDL window:") + SDL_GetError());
	}
	else
	{
		if (!SDL_CreateWindowAndRenderer("", 320, 200, flags, &Handle.window, &RendererHandle))
			throw std::runtime_error(std::string("Unable to create SDL window:") + SDL_GetError());
	}

	SDL_StartTextInput(Handle.window);

#ifdef _WIN32
	// Maximize taken as Windows asks it (MaximizeCommandProc)
	if (HWND hwnd = (HWND)GetWin32Handle())
	{
		SetPropW(hwnd, L"DXWidgets.SDL3DisplayWindow", this);
		SDLWindowProc = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&MaximizeCommandProc);
	}
#endif

	WindowList[SDL_GetWindowID(Handle.window)] = this;
}

SDL3DisplayWindow::~SDL3DisplayWindow()
{
	SDL3DisplayWindow::UnlockCursor();

	SDL_StopTextInput(Handle.window);

	WindowList.erase(WindowList.find(SDL_GetWindowID(Handle.window)));

	if (BackBufferTexture)
	{
		SDL_DestroyTexture(BackBufferTexture);
		BackBufferTexture = nullptr;
	}

	if (m_GLContext)
	{
		SDL_GL_DestroyContext(m_GLContext);
		m_GLContext = nullptr;
	}

	if (RendererHandle)
		SDL_DestroyRenderer(RendererHandle);
#ifdef _WIN32
	// SDL's own procedure back for the window's last messages
	if (HWND hwnd = (HWND)GetWin32Handle())
	{
		if (SDLWindowProc)
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, SDLWindowProc);
		RemovePropW(hwnd, L"DXWidgets.SDL3DisplayWindow");
	}
#endif
	SDL_DestroyWindow(Handle.window);
	RendererHandle = nullptr;
	Handle.window = nullptr;
}

void* SDL3DisplayWindow::GetWin32Handle()
{
	return SDL_GetPointerProperty(SDL_GetWindowProperties(Handle.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}

std::vector<std::string> SDL3DisplayWindow::GetVulkanInstanceExtensions()
{
	Uint32 extCount = 0;
	char const* const* extNames = SDL_Vulkan_GetInstanceExtensions(&extCount);

	std::vector<std::string> result;

	for (Uint32 i = 0 ; i < extCount ; i++)
		result.emplace_back(std::string(extNames[i]));

	return result;
}

VkSurfaceKHR SDL3DisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	VkSurfaceKHR surfaceHandle = {};
	if (!SDL_Vulkan_CreateSurface(Handle.window, instance, nullptr, &surfaceHandle))
		throw std::runtime_error("Could not create vulkan surface");
	return surfaceHandle;
}

void SDL3DisplayWindow::SetWindowTitle(const std::string& text)
{
	SDL_SetWindowTitle(Handle.window, text.c_str());
}

SDL_PixelFormat SDL3DisplayWindow::ImageFormatToSDLPixelFormat(const ImageFormat& format)
{
	switch (format)
	{
	case ImageFormat::B8G8R8A8:
		return SDL_PIXELFORMAT_BGRA32;
	case ImageFormat::R8G8B8A8:
		return SDL_PIXELFORMAT_RGBA32;
	}

	return SDL_PIXELFORMAT_UNKNOWN;
}

// An image's pixels in a surface of its own, row by row: a surface's rows can
// be longer than an image's.
SDL_Surface* SDL3DisplayWindow::CreateIconSurface(const Image& image)
{
	SDL_Surface* surface = SDL_CreateSurface(image.GetWidth(), image.GetHeight(), ImageFormatToSDLPixelFormat(image.GetFormat()));
	if (!surface)
		throw std::runtime_error(std::string("Error on SDL_CreateSurface(): ") + SDL_GetError());
	const size_t rowBytes = (size_t)image.GetWidth() * 4;
	for (int y = 0; y < image.GetHeight(); y++)
		memcpy((uint8_t*)surface->pixels + (size_t)y * surface->pitch, (const uint8_t*)image.GetData() + y * rowBytes, rowBytes);
	return surface;
}

// The first image is the icon, the others alternates of it; SDL keeps its
// own copy.
void SDL3DisplayWindow::SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images)
{
	if (images.empty())
		return;

	SDL_Surface* iconSurface = CreateIconSurface(*images[0]);

	// Add the remaining images
	for (size_t i = 1 ; i < images.size() ; i++)
	{
		SDL_Surface* altImageSurface = CreateIconSurface(*images[i]);
		if (!SDL_AddSurfaceAlternateImage(iconSurface, altImageSurface))
			throw std::runtime_error(std::string("Error on SDL_AddSurfaceAlternateImage(): ") + SDL_GetError());
		SDL_DestroySurface(altImageSurface); // SDL recommends calling this after AddSurfaceAlternateImage() is successful.
	}

	SDL_SetWindowIcon(Handle.window, iconSurface);
	SDL_DestroySurface(iconSurface);
}

void SDL3DisplayWindow::SetClientFrame(const Rect& box)
{
	// Is there a way to set both in one call?

	double units = UnitsPerDip();
	int x = (int)std::round(box.x * units);
	int y = (int)std::round(box.y * units);
	int w = (int)std::round(box.width * units);
	int h = (int)std::round(box.height * units);

	// SDL_SetWindowSize sets the client size of the window
	SDL_SetWindowSize(Handle.window, w, h);
	SDL_SetWindowPosition(Handle.window, x, y);
	placed = true;
}

void SDL3DisplayWindow::SetClientSize(const Size& size)
{
	double units = UnitsPerDip();
	SDL_SetWindowSize(Handle.window, (int)std::round(size.width * units), (int)std::round(size.height * units));
	// Never placed nor shown: SDL puts a new window in the middle of the
	// display, but on Windows it keeps the corner its first size centered it
	// by, which a larger one runs off the screen from: centered again. One
	// shown stays where it is, as one back from full screen where SDL put it
	// back. Elsewhere the window manager or the compositor places it.
	if (!placed && (SDL_GetWindowFlags(Handle.window) & SDL_WINDOW_HIDDEN) && SDL_strcmp(SDL_GetCurrentVideoDriver(), "windows") == 0)
		SDL_SetWindowPosition(Handle.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void SDL3DisplayWindow::Show()
{
	SDL_ShowWindow(Handle.window);
}

// A maximized window is restored first, the window manager's answer waited
// for: SDL brings a window back from full screen maximized as it went, which
// the game's window would take as Maximize again (OnWindowEvent)
void SDL3DisplayWindow::ShowFullscreen()
{
	SDL_ShowWindow(Handle.window);
	if (SDL_GetWindowFlags(Handle.window) & SDL_WINDOW_MAXIMIZED)
	{
		SDL_RestoreWindow(Handle.window);
		SDL_SyncWindow(Handle.window);
	}
	SDL_SetWindowFullscreen(Handle.window, true);
	isFullscreen = true;
}

// The mode SDL takes when the window is full screen: the display's nearest to
// the size, or none, the desktop's, borderless.
void SDL3DisplayWindow::SetFullscreenMode(int width, int height)
{
	if (width > 0 && height > 0)
	{
		SDL_DisplayID display = SDL_GetDisplayForWindow(Handle.window);
		SDL_DisplayMode mode = {};
		if (display && SDL_GetClosestFullscreenDisplayMode(display, width, height, 0.0f, false, &mode))
		{
			SDL_SetWindowFullscreenMode(Handle.window, &mode);
			return;
		}
	}
	SDL_SetWindowFullscreenMode(Handle.window, nullptr);
}

void SDL3DisplayWindow::ShowMaximized()
{
	SDL_ShowWindow(Handle.window);
	SDL_MaximizeWindow(Handle.window);
}

void SDL3DisplayWindow::ShowMinimized()
{
	SDL_ShowWindow(Handle.window);
	SDL_MinimizeWindow(Handle.window);
}

// Out of full screen and not maximized, as Win32's normal window
void SDL3DisplayWindow::ShowNormal()
{
	SDL_ShowWindow(Handle.window);
	SDL_SetWindowFullscreen(Handle.window, false);
	if (SDL_GetWindowFlags(Handle.window) & SDL_WINDOW_MAXIMIZED)
		SDL_RestoreWindow(Handle.window);
	isFullscreen = false;
}

void SDL3DisplayWindow::SetWindowResizable(bool enable)
{
	SDL_SetWindowResizable(Handle.window, enable);
}

void SDL3DisplayWindow::SetWindowMinSize(int width, int height)
{
	SDL_SetWindowMinimumSize(Handle.window, width, height);
}

void SDL3DisplayWindow::SetWindowMaxSize(int width, int height)
{
	SDL_SetWindowMaximumSize(Handle.window, width, height);
}

bool SDL3DisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void SDL3DisplayWindow::Hide()
{
	SDL_HideWindow(Handle.window);
}

void SDL3DisplayWindow::Activate()
{
	SDL_RaiseWindow(Handle.window);
}

void SDL3DisplayWindow::ShowCursor(bool enable)
{
	if (enable)
		SDL_ShowCursor();
	else
		SDL_HideCursor();
}

// The keyboard locked to the program: no text input, so a desktop's input
// method leaves held keys alone (KDE's accent picker on Wayland takes a
// held letter in a window with text input on); unlocked, typed text again.
void SDL3DisplayWindow::LockKeyboard()
{
	SDL_StopTextInput(Handle.window);
}

void SDL3DisplayWindow::UnlockKeyboard()
{
	SDL_StartTextInput(Handle.window);
}

void SDL3DisplayWindow::LockCursor()
{
	if (!CursorLocked)
	{
		SDL_SetWindowRelativeMouseMode(Handle.window, true);
		CursorLocked = true;
	}
}

void SDL3DisplayWindow::UnlockCursor()
{
	if (CursorLocked)
	{
		SDL_SetWindowRelativeMouseMode(Handle.window, false);
		CursorLocked = false;
	}
}

void SDL3DisplayWindow::CaptureMouse()
{
	SDL_CaptureMouse(true);
}

void SDL3DisplayWindow::ReleaseMouseCapture()
{
	SDL_CaptureMouse(false);
}

void SDL3DisplayWindow::SetCursor(StandardCursor cursor, std::shared_ptr<CustomCursor> custom)
{
}

void SDL3DisplayWindow::Update()
{
	SDL_Event event = {};
	event.type = PaintEventNumber;
	event.user.windowID = SDL_GetWindowID(Handle.window);
	SDL_PushEvent(&event);
}

bool SDL3DisplayWindow::GetKeyState(InputKey key)
{
	int numkeys = 0;
	const bool* state = SDL_GetKeyboardState(&numkeys);
	if (!state) return false;

	// A timeline's key (SimulateKey) is an event SDL's own state never
	// sees: it counts as down from its press to its release
	auto down = [&](SDL_Scancode index) { return (index < numkeys && state[index]) || (index < SDL_SCANCODE_COUNT && SimulatedKeys[index]); };
	// Either side's, as Windows' state of VK_SHIFT, VK_CONTROL and VK_MENU.
	switch (key)
	{
	case InputKey::Shift: return down(SDL_SCANCODE_LSHIFT) || down(SDL_SCANCODE_RSHIFT);
	case InputKey::Ctrl: return down(SDL_SCANCODE_LCTRL) || down(SDL_SCANCODE_RCTRL);
	case InputKey::Alt: return down(SDL_SCANCODE_LALT) || down(SDL_SCANCODE_RALT);
	default: return down(InputKeyToScancode(key));
	}
}

Rect SDL3DisplayWindow::GetClientFrame() const
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	double units = UnitsPerDip();
	SDL_GetWindowPosition(Handle.window, &x, &y);
	SDL_GetWindowSize(Handle.window, &w, &h);
	return Rect::xywh(x / units, y / units, w / units, h / units);
}

Point SDL3DisplayWindow::MapFromGlobal(const Point& pos) const
{
	int x = 0;
	int y = 0;
	double units = UnitsPerDip();
	SDL_GetWindowPosition(Handle.window, &x, &y);
	return Point(pos.x - x / units, pos.y - y / units);
}

Point SDL3DisplayWindow::MapToGlobal(const Point& pos) const
{
	int x = 0;
	int y = 0;
	double units = UnitsPerDip();
	SDL_GetWindowPosition(Handle.window, &x, &y);
	return Point(pos.x + x / units, pos.y + y / units);
}

Size SDL3DisplayWindow::GetClientSize() const
{
	int w = 0;
	int h = 0;
	double units = UnitsPerDip();
	SDL_GetWindowSize(Handle.window, &w, &h);
	return Size(w / units, h / units);
}

int SDL3DisplayWindow::GetPixelWidth() const
{
	int w = 0;
	int h = 0;
	if (RendererHandle)
		SDL_GetCurrentRenderOutputSize(RendererHandle, &w, &h);
	else
		SDL_GetWindowSizeInPixels(Handle.window, &w, &h);
	return w;
}

int SDL3DisplayWindow::GetPixelHeight() const
{
	int w = 0;
	int h = 0;
	if (RendererHandle)
		SDL_GetCurrentRenderOutputSize(RendererHandle, &w, &h);
	else
		SDL_GetWindowSizeInPixels(Handle.window, &w, &h);
	return h;
}

double SDL3DisplayWindow::GetDpiScale() const
{
	return SDL_GetWindowDisplayScale(Handle.window);
}

double SDL3DisplayWindow::UnitsPerDip() const
{
	double density = SDL_GetWindowPixelDensity(Handle.window);
	return density > 0.0 ? GetDpiScale() / density : GetDpiScale();
}

void SDL3DisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (!RendererHandle)
		return;

	if (!BackBufferTexture || BackBufferWidth != width || BackBufferHeight != height)
	{
		if (BackBufferTexture)
		{
			SDL_DestroyTexture(BackBufferTexture);
			BackBufferTexture = nullptr;
		}

		BackBufferTexture = SDL_CreateTexture(RendererHandle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!BackBufferTexture)
			return;
		// Copied as it is: the format's alpha would otherwise blend it
		// over what the renderer last drew
		SDL_SetTextureBlendMode(BackBufferTexture, SDL_BLENDMODE_NONE);

		BackBufferWidth = width;
		BackBufferHeight = height;
	}

	int destpitch = 0;
	void* dest = nullptr;
	if (!SDL_LockTexture(BackBufferTexture, nullptr, &dest, &destpitch))
		return;
	for (int y = 0; y < height; y++)
	{
		const void* sline = pixels + y * width;
		void* dline = (uint8_t*)dest + y * destpitch;
		memcpy(dline, sline, width << 2);
	}
	SDL_UnlockTexture(BackBufferTexture);

	SDL_RenderTexture(RendererHandle, BackBufferTexture, nullptr, nullptr);
	SDL_RenderPresent(RendererHandle);
}

void SDL3DisplayWindow::SetBitmapSwapInterval(int interval)
{
	if (RendererHandle)
		SDL_SetRenderVSync(RendererHandle, interval);
}

void SDL3DisplayWindow::SetBorderColor(uint32_t rgba8)
{
	// SDL doesn't have this
}

void SDL3DisplayWindow::SetCaptionColor(uint32_t rgba8)
{
	// SDL doesn't have this
}

void SDL3DisplayWindow::SetCaptionTextColor(uint32_t rgba8)
{
	// SDL doesn't have this
}

std::string SDL3DisplayWindow::GetClipboardText()
{
	char* buffer = SDL_GetClipboardText();
	if (!buffer)
		return {};
	std::string text = buffer;
	SDL_free(buffer);
	return text;
}

void SDL3DisplayWindow::SetClipboardText(const std::string& text)
{
	SDL_SetClipboardText(text.c_str());
}

void SDL3DisplayWindow::ProcessEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		DispatchEvent(event);
	}
}

void SDL3DisplayWindow::RunLoop()
{
	ExitRunLoop = false;
	while (!ExitRunLoop)
	{
		SDL_Event event = {};
		if (SDL_WaitEvent(&event))
			DispatchEvent(event); // Silently ignore if it fails and pray it doesn't busy loop, because SDL and Linux utterly sucks!
	}
}

void SDL3DisplayWindow::ExitLoop()
{
	ExitRunLoop = true;
}

std::unordered_map<void *, std::function<void()>> SDL3DisplayWindow::Timers;
std::unordered_map<SDL_TimerID, void *> SDL3DisplayWindow::TimerHandles;
std::mutex SDL3DisplayWindow::TimersMutex;
unsigned long SDL3DisplayWindow::TimerIDs = 0;
Uint32 TimerEventID = SDL_RegisterEvents(1);

// On SDL's timer thread: Timers looked at under the lock the main thread
// changes it under
Uint32 SDL3DisplayWindow::ExecTimer(void* execID, SDL_TimerID timerID, Uint32 interval)
{
	{
		std::lock_guard<std::mutex> lock(TimersMutex);
		// cancel event and stop loop if function not found
		if (Timers.find(execID) == Timers.end())
			return 0;
	}

	SDL_Event timerEvent;
	SDL_zero(timerEvent);

	timerEvent.user.type = TimerEventID;
	timerEvent.user.data1 = execID;

	SDL_PushEvent(&timerEvent);

	return interval;
}

void* SDL3DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	void* execID = (void*)(uintptr_t)++TimerIDs;
	// The function in place first: the timer can go off before SDL_AddTimer returns
	{
		std::lock_guard<std::mutex> lock(TimersMutex);
		Timers.insert({execID, onTimer});
	}
	SDL_TimerID id = SDL_AddTimer(timeoutMilliseconds, SDL3DisplayWindow::ExecTimer, execID);

	std::lock_guard<std::mutex> lock(TimersMutex);
	if (!id)
	{
		Timers.erase(execID);
		return nullptr;
	}
	TimerHandles.insert({id, execID});

	return (void*)(uintptr_t) id;
}

void SDL3DisplayWindow::StopTimer(void* timerID)
{
	SDL_TimerID SDLTimerID = (SDL_TimerID)(uintptr_t)timerID;
	SDL_RemoveTimer(SDLTimerID);

	std::lock_guard<std::mutex> lock(TimersMutex);
	auto execID = TimerHandles.find(SDLTimerID);
	if (execID == TimerHandles.end())
		return;

	Timers.erase(execID->second);
	TimerHandles.erase(execID);
}

SDL3DisplayWindow* SDL3DisplayWindow::FindEventWindow(const SDL_Event& event)
{
	static unsigned int windowID;

	if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
		windowID = event.window.windowID;
	else if (event.type == SDL_EVENT_TEXT_INPUT)
		windowID = event.text.windowID;
	else if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN)
		windowID = event.key.windowID;
	else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
		windowID = event.button.windowID;
	else if (event.type == SDL_EVENT_MOUSE_WHEEL)
		windowID = event.wheel.windowID;
	else if (event.type == SDL_EVENT_MOUSE_MOTION)
		windowID = event.motion.windowID;
	else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP || event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
		; // a pad's has no window: the last event's, as SDL2's window takes it
	else if (event.type == PaintEventNumber)
		windowID = event.user.windowID;
	else
		return nullptr;

	const auto it = WindowList.find(windowID);
	return it != WindowList.end() ? it->second : nullptr;
}

void SDL3DisplayWindow::DispatchEvent(const SDL_Event& event)
{
	// timers are created in a non-window context
	if (event.type == TimerEventID)
		return OnTimerEvent(event.user);

	// Posted by SDL's own SIGTERM and SIGINT handlers, installed where the
	// engine's is not (the crash report window): every window closes, as
	// a closed window does. Ignored, the window outlived the signal.
	if (event.type == SDL_EVENT_QUIT)
	{
		std::vector<SDL3DisplayWindow*> windows;
		for (auto& it : WindowList)
			windows.push_back(it.second);
		for (SDL3DisplayWindow* w : windows)
			for (auto& it : WindowList)
				if (it.second == w)
				{
					w->WindowHost->OnWindowClose();
					break;
				}
		return;
	}

	SDL3DisplayWindow* window = FindEventWindow(event);
	if (!window) return;

	static bool dropJoyDown, joyDown, keyDown;
	static unsigned long eventFrame = 0;

	// A pad button and a key down within the same 10 ms are one press sent
	// twice; the pad's is dropped for those 10 ms only
	if (eventFrame != event.common.timestamp / 10000000) // nanoseconds / 10 ms
	{
		eventFrame = event.common.timestamp / 10000000;
		dropJoyDown = joyDown = keyDown = false;
	}

	if (!GamepadKeyEmulation && (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP))
		return;

	if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) joyDown = true;
	if (event.type == SDL_EVENT_KEY_DOWN) keyDown = true;
	if (joyDown && keyDown) dropJoyDown = true;

	// steamdeck desktop mode fires double events
	if (dropJoyDown && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) return;

	if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
		window->OnWindowEvent(event.window);
	else if (event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING)
		window->OnTextInput(event.text);
	else if (event.type == SDL_EVENT_KEY_UP)
		window->OnKeyUp(event.key);
	else if (event.type == SDL_EVENT_KEY_DOWN)
		window->OnKeyDown(event.key);
	else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
		window->OnMouseButtonUp(event.button);
	else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
		window->OnMouseButtonDown(event.button);
	else if (event.type == SDL_EVENT_MOUSE_WHEEL)
		window->OnMouseWheel(event.wheel);
	else if (event.type == SDL_EVENT_MOUSE_MOTION)
		window->OnMouseMotion(event.motion);
	else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
		window->OnJoyButtonUp(event.gbutton);
	else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
		window->OnJoyButtonDown(event.gbutton);
	else if (event.type == PaintEventNumber)
		window->OnPaintEvent();
}

void SDL3DisplayWindow::OnWindowEvent(const SDL_WindowEvent& event)
{
	if (!(event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST))
		return;

	if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		WindowHost->OnWindowClose();
	else if (event.type == SDL_EVENT_WINDOW_MOVED ||
			 event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
			 event.type == SDL_EVENT_WINDOW_RESIZED)
		WindowHost->OnWindowGeometryChanged();
	else if (event.type == SDL_EVENT_WINDOW_SHOWN ||
			 event.type == SDL_EVENT_WINDOW_EXPOSED)
		WindowHost->OnWindowPaint();
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
	{
		// Alt+F4 and Alt+Space as the focused window takes them on Windows,
		// where SDL answers WM_SYSKEYDOWN itself and drops SC_KEYMENU: a
		// device's window, the game's, as keys (SDL3DisplayBackend), its
		// window menu opened by OnKeyDown alone; a widget window, the error
		// window, as Windows' own, closing it and opening its window menu
		SDL_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, SystemKeys ? "1" : "0");
		SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, SystemKeys ? "1" : "0");
		WindowHost->OnWindowActivated();
	}
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
		WindowHost->OnWindowDeactivated();
	else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
		WindowHost->OnWindowMouseLeave();
#ifndef _WIN32
	else if (event.type == SDL_EVENT_WINDOW_MAXIMIZED && !isFullscreen)
	{
		// Maximize -- the title bar's button, a double click on it, the
		// window menu's -- which SDL tells of once the window manager has
		// maximized the window: the host's to take, the game's window
		// toggling full screen, restored as it goes (ShowFullscreen), as the
		// original's takes the command (dx-reverse-info/windrv-dll.md, the
		// window's commands). None is taken in full screen: one queued as
		// the toggle went is stale. On Windows the command is taken before
		// Windows maximizes the window (MaximizeCommandProc), and a maximize
		// no command carries stays one, as the original's
		WindowHost->OnWindowMaximize();
	}
#endif
}

void SDL3DisplayWindow::OnTextInput(const SDL_TextInputEvent& event)
{
	WindowHost->OnWindowKeyChar(event.text);
}

void SDL3DisplayWindow::OnJoyButtonUp(const SDL_GamepadButtonEvent& event)
{
	WindowHost->OnWindowKeyUp(GameControllerButtonToInputKey((SDL_GamepadButton)event.button));
}

void SDL3DisplayWindow::OnJoyButtonDown(const SDL_GamepadButtonEvent& event)
{
	WindowHost->OnWindowKeyDown(GameControllerButtonToInputKey((SDL_GamepadButton)event.button));
}

// A shift, control or alt is its name for both sides, then its side's own,
// as the original's are: the first from the window's key message, the
// second from its poll of every key, which sees a held key once
// (dx-reverse-info/windrv-dll.md, the keys). A key's repeats and its
// release are the name it was pressed as, whatever Num Lock or the layout
// became meanwhile.
void SDL3DisplayWindow::OnKeyUp(const SDL_KeyboardEvent& event)
{
	InputKey key = PressedAs[event.scancode];
	if (key == InputKey::None)
		key = KeyEventToInputKey(event);
	PressedAs[event.scancode] = InputKey::None;
	WindowHost->OnWindowKeyUp(key);
	InputKey side = ScancodeToSideKey(event.scancode);
	if (side != InputKey::None)
		WindowHost->OnWindowKeyUp(side);
}

void SDL3DisplayWindow::OnKeyDown(const SDL_KeyboardEvent& event)
{
	InputKey key = PressedAs[event.scancode];
	if (!event.repeat || key == InputKey::None)
		PressedAs[event.scancode] = key = KeyEventToInputKey(event);
	WindowHost->OnWindowKeyDown(key);
	InputKey side = ScancodeToSideKey(event.scancode);
	if (side != InputKey::None && !event.repeat)
		WindowHost->OnWindowKeyDown(side);
	std::string chars = AltCharacter(event);
	if (!chars.empty())
		WindowHost->OnWindowKeyChar(chars);
#ifdef _WIN32
	// Alt+Space in the game's window, while the game takes no typed text
	// (text input off, Engine::TickWindow): Windows' window menu, as the
	// original's DefWindowProc opens it from the WM_SYSCHAR its game leaves
	// (dx-reverse-info/windrv-dll.md, the keys). SDL drops SC_KEYMENU in the
	// game's window, its mnemonics hint off (OnWindowEvent), so that no other
	// Alt+character reaches the window menu, which has no item for it and
	// would beep. Full screen the window has no window menu (no WS_SYSMENU)
	if (!SystemKeys && chars == " " && !SDL_TextInputActive(Handle.window))
	{
		if (HWND hwnd = (HWND)GetWin32Handle())
			DefWindowProcW(hwnd, WM_SYSCOMMAND, SC_KEYMENU, ' ');
	}
#endif
}

// A key as the original's WinDrv names it: the Windows virtual-key code its
// key message carries, which is EInputKey's (dx-reverse-info/windrv-dll.md,
// the keys). Windows' code follows Num Lock and the layout: with Num Lock
// off the keypad's digits and point are the keys they move by (VK_CLEAR,
// Unknown0C, for 5); a typing key on Windows is the code Windows' own
// layout gives its scancode, elsewhere it is named as the original's Wine
// names it (LayoutKeyName). Any other key by its place on a US keyboard.
InputKey SDL3DisplayWindow::KeyEventToInputKey(const SDL_KeyboardEvent& event)
{
	if (!(event.mod & SDL_KMOD_NUM))
	{
		switch (event.scancode)
		{
		case SDL_SCANCODE_KP_0:      return InputKey::Insert;
		case SDL_SCANCODE_KP_1:      return InputKey::End;
		case SDL_SCANCODE_KP_2:      return InputKey::Down;
		case SDL_SCANCODE_KP_3:      return InputKey::PageDown;
		case SDL_SCANCODE_KP_4:      return InputKey::Left;
		case SDL_SCANCODE_KP_5:      return InputKey::Unknown0C;
		case SDL_SCANCODE_KP_6:      return InputKey::Right;
		case SDL_SCANCODE_KP_7:      return InputKey::Home;
		case SDL_SCANCODE_KP_8:      return InputKey::Up;
		case SDL_SCANCODE_KP_9:      return InputKey::PageUp;
		case SDL_SCANCODE_KP_PERIOD: return InputKey::Delete;
		default: break;
		}
	}
	InputKey typing = LayoutKeyName(event.scancode);
	if (typing == InputKey::None)
		return ScancodeToInputKey(event.scancode);
#ifdef _WIN32
	if (event.raw != 0 && SimulatedLayout.empty())
	{
		UINT vk = MapVirtualKeyExW(event.raw, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
		if (vk != 0 && vk < 0xFF)
			return (InputKey)vk;
	}
#endif
	return typing;
}

// The typing keys -- a US keyboard's letters, digits and punctuation, the
// ISO key by the left Shift and an ABNT or JIS keyboard's own keys -- as
// Wine's X11 keyboard driver, the original's on Linux, names them
// (dlls/winex11.drv/keyboard.c, main_key_vkey_*): the layout's family told
// by what a few keys type (SDL's keycodes, Latin letters on any layout, a
// dead key its US key's) -- AZERTY's A and Z, QWERTZ's Z and Y, Dvorak's O
// and E, Swedish's (and Finnish's) å, ö and ä, Brazilian ABNT's ç and [,
// Japanese 106's @ and : -- a QWERTY's keys by their places, as every other
// layout's are, each other family's by its table, the French, Dvorak and
// Swedish ones Windows' own. None for any other key.
InputKey SDL3DisplayWindow::LayoutKeyName(SDL_Scancode scancode)
{
	InputKey place;
	switch (scancode)
	{
	case SDL_SCANCODE_NONUSBACKSLASH:
	case SDL_SCANCODE_INTERNATIONAL1: place = InputKey::JoyZ; break;      // VK_OEM_102: ABNT's /?, JIS's ro too
	case SDL_SCANCODE_INTERNATIONAL3: place = InputKey::Backslash; break; // VK_OEM_5: JIS's yen
	case SDL_SCANCODE_NONUSHASH:      place = InputKey::Backslash; break;
	default:                          place = ScancodeToInputKey(scancode); break;
	}
	bool typing = (place >= InputKey::_0 && place <= InputKey::_9) || (place >= InputKey::A && place <= InputKey::Z) ||
		(place >= InputKey::Semicolon && place <= InputKey::Tilde) || (place >= InputKey::LeftBracket && place <= InputKey::SingleQuote) ||
		place == InputKey::JoyZ;
	if (!typing)
		return InputKey::None;

	auto at = [](SDL_Scancode s) { return SDL_GetKeyFromScancode(s, SDL_KMOD_NONE, true); };
	std::string family = SimulatedLayout;
	if (family.empty())
	{
		if (at(SDL_SCANCODE_Q) == 'a' && at(SDL_SCANCODE_W) == 'z')
			family = "AZERTY";
		else if (at(SDL_SCANCODE_Y) == 'z' && at(SDL_SCANCODE_Z) == 'y')
			family = "QWERTZ";
		else if (at(SDL_SCANCODE_S) == 'o' && at(SDL_SCANCODE_D) == 'e')
			family = "Dvorak";
		else if (at(SDL_SCANCODE_LEFTBRACKET) == 0xe5 && at(SDL_SCANCODE_SEMICOLON) == 0xf6 && at(SDL_SCANCODE_APOSTROPHE) == 0xe4)
			family = "Swedish";
		else if (at(SDL_SCANCODE_SEMICOLON) == 0xe7 && at(SDL_SCANCODE_RIGHTBRACKET) == '[')
			family = "ABNT";
		else if (at(SDL_SCANCODE_LEFTBRACKET) == '@' && at(SDL_SCANCODE_APOSTROPHE) == ':')
			family = "JP106";
	}

	if (family == "QWERTZ")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_Y:            return InputKey::Z;
		case SDL_SCANCODE_Z:            return InputKey::Y;
		default: break;
		}
	}
	else if (family == "AZERTY")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_GRAVE:        return InputKey::SingleQuote;  // VK_OEM_7
		case SDL_SCANCODE_MINUS:        return InputKey::LeftBracket;  // VK_OEM_4
		case SDL_SCANCODE_Q:            return InputKey::A;
		case SDL_SCANCODE_W:            return InputKey::Z;
		case SDL_SCANCODE_LEFTBRACKET:  return InputKey::RightBracket; // VK_OEM_6
		case SDL_SCANCODE_RIGHTBRACKET: return InputKey::Semicolon;    // VK_OEM_1
		case SDL_SCANCODE_A:            return InputKey::Q;
		case SDL_SCANCODE_SEMICOLON:    return InputKey::M;
		case SDL_SCANCODE_APOSTROPHE:   return InputKey::Tilde;        // VK_OEM_3
		case SDL_SCANCODE_Z:            return InputKey::W;
		case SDL_SCANCODE_M:            return InputKey::Comma;
		case SDL_SCANCODE_COMMA:        return InputKey::Period;
		case SDL_SCANCODE_PERIOD:       return InputKey::Slash;        // VK_OEM_2
		case SDL_SCANCODE_SLASH:        return InputKey::UnknownDF;    // VK_OEM_8
		default: break;
		}
	}
	else if (family == "Swedish")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_GRAVE:        return InputKey::Backslash;    // VK_OEM_5
		case SDL_SCANCODE_MINUS:        return InputKey::Equals;       // VK_OEM_PLUS
		case SDL_SCANCODE_EQUALS:       return InputKey::LeftBracket;  // VK_OEM_4
		case SDL_SCANCODE_LEFTBRACKET:  return InputKey::RightBracket; // VK_OEM_6
		case SDL_SCANCODE_RIGHTBRACKET: return InputKey::Semicolon;    // VK_OEM_1
		case SDL_SCANCODE_SEMICOLON:    return InputKey::Tilde;        // VK_OEM_3
		case SDL_SCANCODE_BACKSLASH:
		case SDL_SCANCODE_NONUSHASH:    return InputKey::Slash;        // VK_OEM_2
		case SDL_SCANCODE_SLASH:        return InputKey::Minus;
		default: break;
		}
	}
	else if (family == "ABNT")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_APOSTROPHE:     return InputKey::UnknownDF;    // VK_OEM_8
		case SDL_SCANCODE_NONUSBACKSLASH: return InputKey::SingleQuote;  // VK_OEM_7
		default: break;
		}
	}
	else if (family == "JP106")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_GRAVE:        return InputKey::JoyPovRight;  // Hankaku/Zenkaku, VK_DBE_SBCSCHAR
		case SDL_SCANCODE_EQUALS:       return InputKey::SingleQuote;  // VK_OEM_7
		case SDL_SCANCODE_LEFTBRACKET:  return InputKey::Tilde;        // VK_OEM_3
		case SDL_SCANCODE_RIGHTBRACKET: return InputKey::LeftBracket;  // VK_OEM_4
		case SDL_SCANCODE_SEMICOLON:    return InputKey::Equals;       // VK_OEM_PLUS
		case SDL_SCANCODE_APOSTROPHE:   return InputKey::Semicolon;    // VK_OEM_1
		case SDL_SCANCODE_BACKSLASH:
		case SDL_SCANCODE_NONUSHASH:    return InputKey::RightBracket; // VK_OEM_6
		default: break;
		}
	}
	else if (family == "Dvorak")
	{
		switch (scancode)
		{
		case SDL_SCANCODE_MINUS:        return InputKey::LeftBracket;
		case SDL_SCANCODE_EQUALS:       return InputKey::RightBracket;
		case SDL_SCANCODE_Q:            return InputKey::SingleQuote;
		case SDL_SCANCODE_W:            return InputKey::Comma;
		case SDL_SCANCODE_E:            return InputKey::Period;
		case SDL_SCANCODE_R:            return InputKey::P;
		case SDL_SCANCODE_T:            return InputKey::Y;
		case SDL_SCANCODE_Y:            return InputKey::F;
		case SDL_SCANCODE_U:            return InputKey::G;
		case SDL_SCANCODE_I:            return InputKey::C;
		case SDL_SCANCODE_O:            return InputKey::R;
		case SDL_SCANCODE_P:            return InputKey::L;
		case SDL_SCANCODE_LEFTBRACKET:  return InputKey::Slash;
		case SDL_SCANCODE_RIGHTBRACKET: return InputKey::Equals;
		case SDL_SCANCODE_S:            return InputKey::O;
		case SDL_SCANCODE_D:            return InputKey::E;
		case SDL_SCANCODE_F:            return InputKey::U;
		case SDL_SCANCODE_G:            return InputKey::I;
		case SDL_SCANCODE_H:            return InputKey::D;
		case SDL_SCANCODE_J:            return InputKey::H;
		case SDL_SCANCODE_K:            return InputKey::T;
		case SDL_SCANCODE_L:            return InputKey::N;
		case SDL_SCANCODE_SEMICOLON:    return InputKey::S;
		case SDL_SCANCODE_APOSTROPHE:   return InputKey::Minus;
		case SDL_SCANCODE_Z:            return InputKey::Semicolon;
		case SDL_SCANCODE_X:            return InputKey::Q;
		case SDL_SCANCODE_C:            return InputKey::J;
		case SDL_SCANCODE_V:            return InputKey::K;
		case SDL_SCANCODE_B:            return InputKey::X;
		case SDL_SCANCODE_N:            return InputKey::B;
		case SDL_SCANCODE_COMMA:        return InputKey::W;
		case SDL_SCANCODE_PERIOD:       return InputKey::V;
		case SDL_SCANCODE_SLASH:        return InputKey::Z;
		default: break;
		}
	}
	return place;
}

// The character a key types while Alt is held, for which SDL sends no text
// (its X11 and Wayland windows drop the text under Alt, its Windows one
// takes no WM_SYSCHAR): the one the layout gives the key with Shift and
// Caps Lock as they are, as the original's window gets it in a WM_SYSCHAR
// after the key (dx-reverse-info/windrv-dll.md, typed characters). None
// with Ctrl or AltGr held too, nor for a key that types no character.
std::string SDL3DisplayWindow::AltCharacter(const SDL_KeyboardEvent& event)
{
	if (!(event.mod & SDL_KMOD_ALT) || (event.mod & (SDL_KMOD_CTRL | SDL_KMOD_MODE)))
		return {};
	SDL_Keycode c = SDL_GetKeyFromScancode(event.scancode, SDL_KMOD_NONE, false);
	if (c >= 'a' && c <= 'z')
	{
		if (((event.mod & SDL_KMOD_SHIFT) != 0) != ((event.mod & SDL_KMOD_CAPS) != 0))
			c = c - 'a' + 'A';
	}
	else if (event.mod & SDL_KMOD_SHIFT)
		c = SDL_GetKeyFromScancode(event.scancode, SDL_KMOD_SHIFT, false);
	if (c < 0x20 || c == 0x7f || (c & SDLK_SCANCODE_MASK) || c > 0x10ffff)
		return {};
	char utf8[5] = {};
	SDL_UCS4ToUTF8(c, utf8);
	return utf8;
}

InputKey SDL3DisplayWindow::GetMouseButtonKey(const SDL_MouseButtonEvent& event)
{
	switch (event.button)
	{
		case SDL_BUTTON_LEFT: return InputKey::LeftMouse;
		case SDL_BUTTON_MIDDLE: return InputKey::MiddleMouse;
		case SDL_BUTTON_RIGHT: return InputKey::RightMouse;
		// case SDL_BUTTON_X1: return InputKey::XButton1;
		// case SDL_BUTTON_X2: return InputKey::XButton2;
		default: return InputKey::None;
	}
}

void SDL3DisplayWindow::OnMouseButtonUp(const SDL_MouseButtonEvent& event)
{
	InputKey key = GetMouseButtonKey(event);
	if (key != InputKey::None)
	{
		WindowHost->OnWindowMouseUp(GetMousePos(event), key);
	}
}

void SDL3DisplayWindow::OnMouseButtonDown(const SDL_MouseButtonEvent& event)
{
	InputKey key = GetMouseButtonKey(event);
	if (key != InputKey::None)
	{
		WindowHost->OnWindowMouseDown(GetMousePos(event), key);
	}
}

void SDL3DisplayWindow::OnMouseWheel(const SDL_MouseWheelEvent& event)
{
	float x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	double units = UnitsPerDip();
	Point mousepos(x / units, y / units);

	// A press a notch: the original presses one a WM_MOUSEWHEEL message
	// whatever its delta (WinDrv 0x1110b2bd), which Windows sends a notch of
	// a wheel, where X11's XI2 and Wayland send a high-resolution wheel's and
	// a touchpad's fractions as events of their own (bugs.md WIN.10). Each
	// event's y is added up and pressed as the sum reaches a notch's nearest
	// half, the sum begun again where the scroll turns: a classic notch at
	// once, a fraction's notch half-way into it, so one is pressed though
	// SDL's XI2 drops a device's first scroll event, a fraction. SDL's own
	// integer_y keeps what is left over across a turn, and loses a notch so.
	double scroll = event.y;
	if ((scroll > 0.0 && WheelScroll < 0.0) || (scroll < 0.0 && WheelScroll > 0.0))
	{
		WheelScroll = 0.0;
		WheelPressed = 0;
	}
	WheelScroll += scroll;
	long notches = std::lround(WheelScroll);
	for (; WheelPressed < notches; WheelPressed++)
		WindowHost->OnWindowMouseWheel(mousepos, InputKey::MouseWheelUp);
	for (; WheelPressed > notches; WheelPressed--)
		WindowHost->OnWindowMouseWheel(mousepos, InputKey::MouseWheelDown);
	if (WheelPressed > 1000 || WheelPressed < -1000)
	{
		WheelScroll -= WheelPressed;
		WheelPressed = 0;
	}
}

void SDL3DisplayWindow::OnMouseMotion(const SDL_MouseMotionEvent& event)
{
	if (CursorLocked)
	{
		// SDL's deltas are floats: a fraction is kept for the next event,
		// not dropped (a touchpad's slow move is all fractions)
		float x = event.xrel + RawMoveFractionX;
		float y = event.yrel + RawMoveFractionY;
		int dx = (int)x;
		int dy = (int)y;
		RawMoveFractionX = x - dx;
		RawMoveFractionY = y - dy;
		if (dx != 0 || dy != 0)
			WindowHost->OnWindowRawMouseMove(dx, dy);
	}
	else
	{
		WindowHost->OnWindowMouseMove(GetMousePos(event));
	}
}

void SDL3DisplayWindow::OnPaintEvent()
{
	WindowHost->OnWindowPaint();
}

void SDL3DisplayWindow::OnTimerEvent(const SDL_UserEvent& event)
{
	// A copy is called: the function may stop its own timer, which drops the
	// one in Timers
	std::function<void()> func;
	{
		std::lock_guard<std::mutex> lock(TimersMutex);
		auto it = Timers.find(event.data1);

		// incase timer was cancelled before we get here
		if (it == Timers.end())
			return;
		func = it->second;
	}

	func();
}

void SDL3DisplayWindow::CreateGLContext()
{
	if (m_GLIsES)
	{
		// OpenGL ES 3.2: the GL device's ES shaders are GLSL ES 3.20.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	}
	else
	{
		// Desktop 4.2 core: the shaders are GLSL 4.20.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	}

	m_GLContext = SDL_GL_CreateContext(Handle.window);
	// None is an error here, with SDL's reason, so the GL device fails at
	// its start and the engine falls back to the software device: the GL
	// loader on no context read a null extension string.
	if (!m_GLContext)
		throw std::runtime_error(std::string("Could not create an OpenGL context: ") + SDL_GetError());
}

void SDL3DisplayWindow::MakeGLContextCurrent()
{
	if (!m_GLContext)
		return;

	SDL_GL_MakeCurrent(Handle.window, m_GLContext);
}

bool SDL3DisplayWindow::SetGLSwapInterval(int interval)
{
	// SDL's own answer: a refused interval (adaptive vsync where the driver
	// has none) lets the GL device fall back to a whole one
	return SDL_GL_SetSwapInterval(interval);
}

void SDL3DisplayWindow::SwapGLBuffers()
{
	SDL_GL_SwapWindow(Handle.window);
}

DisplayWindow::GLFuncPtr SDL3DisplayWindow::GetGLProcAddress(const char* name)
{
	return (GLFuncPtr)SDL_GL_GetProcAddress(name);
}

// A key by its place on a US keyboard, as Windows' virtual-key code names
// it there (KeyEventToInputKey).
InputKey SDL3DisplayWindow::ScancodeToInputKey(SDL_Scancode keycode)
{
	switch (keycode)
	{
		case SDL_SCANCODE_BACKSPACE:    return InputKey::Backspace;
		case SDL_SCANCODE_TAB:          return InputKey::Tab;
		case SDL_SCANCODE_CLEAR:        return InputKey::OEMClear;
		case SDL_SCANCODE_RETURN:       return InputKey::Enter;
		case SDL_SCANCODE_KP_ENTER:     return InputKey::Enter;
		case SDL_SCANCODE_LSHIFT:
		case SDL_SCANCODE_RSHIFT:       return InputKey::Shift;
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL:        return InputKey::Ctrl;
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:         return InputKey::Alt;
		case SDL_SCANCODE_CAPSLOCK:     return InputKey::CapsLock;
		case SDL_SCANCODE_LGUI:         return InputKey::Unknown5B;   // VK_LWIN
		case SDL_SCANCODE_RGUI:         return InputKey::Unknown5C;   // VK_RWIN
		case SDL_SCANCODE_APPLICATION:
		case SDL_SCANCODE_MENU:         return InputKey::Unknown5D;   // VK_APPS
		case SDL_SCANCODE_PAUSE:        return InputKey::Pause;
		case SDL_SCANCODE_ESCAPE:       return InputKey::Escape;
		case SDL_SCANCODE_SPACE:        return InputKey::Space;
		case SDL_SCANCODE_END:          return InputKey::End;
		case SDL_SCANCODE_PAGEDOWN:     return InputKey::PageDown;
		case SDL_SCANCODE_HOME:         return InputKey::Home;
		case SDL_SCANCODE_PAGEUP:       return InputKey::PageUp;
		case SDL_SCANCODE_LEFT:         return InputKey::Left;
		case SDL_SCANCODE_UP:           return InputKey::Up;
		case SDL_SCANCODE_RIGHT:        return InputKey::Right;
		case SDL_SCANCODE_DOWN:         return InputKey::Down;
		case SDL_SCANCODE_SELECT:       return InputKey::Select;
		case SDL_SCANCODE_PRINTSCREEN:  return InputKey::PrintScrn;   // VK_SNAPSHOT
		case SDL_SCANCODE_EXECUTE:      return InputKey::Execute;
		case SDL_SCANCODE_INSERT:       return InputKey::Insert;
		case SDL_SCANCODE_DELETE:       return InputKey::Delete;
		case SDL_SCANCODE_HELP:         return InputKey::Help;
		case SDL_SCANCODE_0:            return InputKey::_0;
		case SDL_SCANCODE_1:            return InputKey::_1;
		case SDL_SCANCODE_2:            return InputKey::_2;
		case SDL_SCANCODE_3:            return InputKey::_3;
		case SDL_SCANCODE_4:            return InputKey::_4;
		case SDL_SCANCODE_5:            return InputKey::_5;
		case SDL_SCANCODE_6:            return InputKey::_6;
		case SDL_SCANCODE_7:            return InputKey::_7;
		case SDL_SCANCODE_8:            return InputKey::_8;
		case SDL_SCANCODE_9:            return InputKey::_9;
		case SDL_SCANCODE_A:            return InputKey::A;
		case SDL_SCANCODE_B:            return InputKey::B;
		case SDL_SCANCODE_C:            return InputKey::C;
		case SDL_SCANCODE_D:            return InputKey::D;
		case SDL_SCANCODE_E:            return InputKey::E;
		case SDL_SCANCODE_F:            return InputKey::F;
		case SDL_SCANCODE_G:            return InputKey::G;
		case SDL_SCANCODE_H:            return InputKey::H;
		case SDL_SCANCODE_I:            return InputKey::I;
		case SDL_SCANCODE_J:            return InputKey::J;
		case SDL_SCANCODE_K:            return InputKey::K;
		case SDL_SCANCODE_L:            return InputKey::L;
		case SDL_SCANCODE_M:            return InputKey::M;
		case SDL_SCANCODE_N:            return InputKey::N;
		case SDL_SCANCODE_O:            return InputKey::O;
		case SDL_SCANCODE_P:            return InputKey::P;
		case SDL_SCANCODE_Q:            return InputKey::Q;
		case SDL_SCANCODE_R:            return InputKey::R;
		case SDL_SCANCODE_S:            return InputKey::S;
		case SDL_SCANCODE_T:            return InputKey::T;
		case SDL_SCANCODE_U:            return InputKey::U;
		case SDL_SCANCODE_V:            return InputKey::V;
		case SDL_SCANCODE_W:            return InputKey::W;
		case SDL_SCANCODE_X:            return InputKey::X;
		case SDL_SCANCODE_Y:            return InputKey::Y;
		case SDL_SCANCODE_Z:            return InputKey::Z;
		case SDL_SCANCODE_KP_0:         return InputKey::NumPad0;
		case SDL_SCANCODE_KP_1:         return InputKey::NumPad1;
		case SDL_SCANCODE_KP_2:         return InputKey::NumPad2;
		case SDL_SCANCODE_KP_3:         return InputKey::NumPad3;
		case SDL_SCANCODE_KP_4:         return InputKey::NumPad4;
		case SDL_SCANCODE_KP_5:         return InputKey::NumPad5;
		case SDL_SCANCODE_KP_6:         return InputKey::NumPad6;
		case SDL_SCANCODE_KP_7:         return InputKey::NumPad7;
		case SDL_SCANCODE_KP_8:         return InputKey::NumPad8;
		case SDL_SCANCODE_KP_9:         return InputKey::NumPad9;
		case SDL_SCANCODE_KP_MULTIPLY:  return InputKey::GreyStar;
		case SDL_SCANCODE_KP_PLUS:      return InputKey::GreyPlus;
		case SDL_SCANCODE_SEPARATOR:    return InputKey::Separator;
		case SDL_SCANCODE_KP_MINUS:     return InputKey::GreyMinus;
		case SDL_SCANCODE_KP_PERIOD:    return InputKey::NumPadPeriod;
		case SDL_SCANCODE_KP_DIVIDE:    return InputKey::GreySlash;
		case SDL_SCANCODE_F1:           return InputKey::F1;
		case SDL_SCANCODE_F2:           return InputKey::F2;
		case SDL_SCANCODE_F3:           return InputKey::F3;
		case SDL_SCANCODE_F4:           return InputKey::F4;
		case SDL_SCANCODE_F5:           return InputKey::F5;
		case SDL_SCANCODE_F6:           return InputKey::F6;
		case SDL_SCANCODE_F7:           return InputKey::F7;
		case SDL_SCANCODE_F8:           return InputKey::F8;
		case SDL_SCANCODE_F9:           return InputKey::F9;
		case SDL_SCANCODE_F10:          return InputKey::F10;
		case SDL_SCANCODE_F11:          return InputKey::F11;
		case SDL_SCANCODE_F12:          return InputKey::F12;
		case SDL_SCANCODE_F13:          return InputKey::F13;
		case SDL_SCANCODE_F14:          return InputKey::F14;
		case SDL_SCANCODE_F15:          return InputKey::F15;
		case SDL_SCANCODE_F16:          return InputKey::F16;
		case SDL_SCANCODE_F17:          return InputKey::F17;
		case SDL_SCANCODE_F18:          return InputKey::F18;
		case SDL_SCANCODE_F19:          return InputKey::F19;
		case SDL_SCANCODE_F20:          return InputKey::F20;
		case SDL_SCANCODE_F21:          return InputKey::F21;
		case SDL_SCANCODE_F22:          return InputKey::F22;
		case SDL_SCANCODE_F23:          return InputKey::F23;
		case SDL_SCANCODE_F24:          return InputKey::F24;
		case SDL_SCANCODE_NUMLOCKCLEAR: return InputKey::NumLock;
		case SDL_SCANCODE_SCROLLLOCK:   return InputKey::ScrollLock;
		case SDL_SCANCODE_SEMICOLON:    return InputKey::Semicolon;
		case SDL_SCANCODE_EQUALS:       return InputKey::Equals;
		case SDL_SCANCODE_COMMA:        return InputKey::Comma;
		case SDL_SCANCODE_MINUS:        return InputKey::Minus;
		case SDL_SCANCODE_PERIOD:       return InputKey::Period;
		case SDL_SCANCODE_SLASH:        return InputKey::Slash;
		case SDL_SCANCODE_GRAVE:        return InputKey::Tilde;
		case SDL_SCANCODE_LEFTBRACKET:  return InputKey::LeftBracket;
		case SDL_SCANCODE_BACKSLASH:    return InputKey::Backslash;
		case SDL_SCANCODE_RIGHTBRACKET: return InputKey::RightBracket;
		case SDL_SCANCODE_APOSTROPHE:   return InputKey::SingleQuote;
		default:                        return InputKey::None;
	}
}

// A shift's, control's or alt's side, its own key beside the one of both:
// VK_LSHIFT and the rest, LMENU and RMENU EInputKey's UnknownA4 and A5.
InputKey SDL3DisplayWindow::ScancodeToSideKey(SDL_Scancode keycode)
{
	switch (keycode)
	{
		case SDL_SCANCODE_LSHIFT:       return InputKey::LShift;
		case SDL_SCANCODE_RSHIFT:       return InputKey::RShift;
		case SDL_SCANCODE_LCTRL:        return InputKey::LControl;
		case SDL_SCANCODE_RCTRL:        return InputKey::RControl;
		case SDL_SCANCODE_LALT:         return InputKey::UnknownA4;
		case SDL_SCANCODE_RALT:         return InputKey::UnknownA5;
		default:                        return InputKey::None;
	}
}

InputKey SDL3DisplayWindow::GameControllerButtonToInputKey(const SDL_GamepadButton button)
{
	switch (button)
	{
		case SDL_GAMEPAD_BUTTON_DPAD_UP:        return InputKey::Up;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return InputKey::Down;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return InputKey::Left;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return InputKey::Right;
		case SDL_GAMEPAD_BUTTON_SOUTH:          return InputKey::Enter;
		case SDL_GAMEPAD_BUTTON_EAST:           return InputKey::Backspace;
		case SDL_GAMEPAD_BUTTON_WEST:           return InputKey::Space;
		case SDL_GAMEPAD_BUTTON_NORTH:          return InputKey::Escape;
		case SDL_GAMEPAD_BUTTON_START:          return InputKey::Enter;
		case SDL_GAMEPAD_BUTTON_BACK:           return InputKey::Escape;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return InputKey::Tab;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return InputKey::Tab;
		default:                                return InputKey::None;
	}
}

SDL_Scancode SDL3DisplayWindow::InputKeyToScancode(InputKey inputkey)
{
	switch (inputkey)
	{
		case InputKey::Backspace:     return SDL_SCANCODE_BACKSPACE;
		case InputKey::Tab:           return SDL_SCANCODE_TAB;
		case InputKey::OEMClear:      return SDL_SCANCODE_CLEAR;
		case InputKey::Enter:         return SDL_SCANCODE_RETURN;
		case InputKey::CapsLock:      return SDL_SCANCODE_CAPSLOCK;
		case InputKey::Unknown5B:     return SDL_SCANCODE_LGUI;
		case InputKey::Unknown5C:     return SDL_SCANCODE_RGUI;
		case InputKey::Unknown5D:     return SDL_SCANCODE_APPLICATION;
		case InputKey::UnknownA4:     return SDL_SCANCODE_LALT;
		case InputKey::UnknownA5:     return SDL_SCANCODE_RALT;
		case InputKey::Pause:         return SDL_SCANCODE_PAUSE;
		case InputKey::Escape:        return SDL_SCANCODE_ESCAPE;
		case InputKey::Space:         return SDL_SCANCODE_SPACE;
		case InputKey::End:           return SDL_SCANCODE_END;
		case InputKey::Home:          return SDL_SCANCODE_HOME;
		case InputKey::Left:          return SDL_SCANCODE_LEFT;
		case InputKey::Up:            return SDL_SCANCODE_UP;
		case InputKey::Right:         return SDL_SCANCODE_RIGHT;
		case InputKey::Down:          return SDL_SCANCODE_DOWN;
		case InputKey::Select:        return SDL_SCANCODE_SELECT;
		case InputKey::PrintScrn:     return SDL_SCANCODE_PRINTSCREEN;
		case InputKey::Execute:       return SDL_SCANCODE_EXECUTE;
		case InputKey::Insert:        return SDL_SCANCODE_INSERT;
		case InputKey::Delete:        return SDL_SCANCODE_DELETE;
		case InputKey::Help:          return SDL_SCANCODE_HELP;
		case InputKey::_0:            return SDL_SCANCODE_0;
		case InputKey::_1:            return SDL_SCANCODE_1;
		case InputKey::_2:            return SDL_SCANCODE_2;
		case InputKey::_3:            return SDL_SCANCODE_3;
		case InputKey::_4:            return SDL_SCANCODE_4;
		case InputKey::_5:            return SDL_SCANCODE_5;
		case InputKey::_6:            return SDL_SCANCODE_6;
		case InputKey::_7:            return SDL_SCANCODE_7;
		case InputKey::_8:            return SDL_SCANCODE_8;
		case InputKey::_9:            return SDL_SCANCODE_9;
		case InputKey::A:             return SDL_SCANCODE_A;
		case InputKey::B:             return SDL_SCANCODE_B;
		case InputKey::C:             return SDL_SCANCODE_C;
		case InputKey::D:             return SDL_SCANCODE_D;
		case InputKey::E:             return SDL_SCANCODE_E;
		case InputKey::F:             return SDL_SCANCODE_F;
		case InputKey::G:             return SDL_SCANCODE_G;
		case InputKey::H:             return SDL_SCANCODE_H;
		case InputKey::I:             return SDL_SCANCODE_I;
		case InputKey::J:             return SDL_SCANCODE_J;
		case InputKey::K:             return SDL_SCANCODE_K;
		case InputKey::L:             return SDL_SCANCODE_L;
		case InputKey::M:             return SDL_SCANCODE_M;
		case InputKey::N:             return SDL_SCANCODE_N;
		case InputKey::O:             return SDL_SCANCODE_O;
		case InputKey::P:             return SDL_SCANCODE_P;
		case InputKey::Q:             return SDL_SCANCODE_Q;
		case InputKey::R:             return SDL_SCANCODE_R;
		case InputKey::S:             return SDL_SCANCODE_S;
		case InputKey::T:             return SDL_SCANCODE_T;
		case InputKey::U:             return SDL_SCANCODE_U;
		case InputKey::V:             return SDL_SCANCODE_V;
		case InputKey::W:             return SDL_SCANCODE_W;
		case InputKey::X:             return SDL_SCANCODE_X;
		case InputKey::Y:             return SDL_SCANCODE_Y;
		case InputKey::Z:             return SDL_SCANCODE_Z;
		case InputKey::NumPad0:       return SDL_SCANCODE_KP_0;
		case InputKey::NumPad1:       return SDL_SCANCODE_KP_1;
		case InputKey::NumPad2:       return SDL_SCANCODE_KP_2;
		case InputKey::NumPad3:       return SDL_SCANCODE_KP_3;
		case InputKey::NumPad4:       return SDL_SCANCODE_KP_4;
		case InputKey::NumPad5:       return SDL_SCANCODE_KP_5;
		case InputKey::NumPad6:       return SDL_SCANCODE_KP_6;
		case InputKey::NumPad7:       return SDL_SCANCODE_KP_7;
		case InputKey::NumPad8:       return SDL_SCANCODE_KP_8;
		case InputKey::NumPad9:       return SDL_SCANCODE_KP_9;
		case InputKey::Separator:     return SDL_SCANCODE_SEPARATOR;
		case InputKey::NumPadPeriod:  return SDL_SCANCODE_KP_PERIOD;
		case InputKey::F1:            return SDL_SCANCODE_F1;
		case InputKey::F2:            return SDL_SCANCODE_F2;
		case InputKey::F3:            return SDL_SCANCODE_F3;
		case InputKey::F4:            return SDL_SCANCODE_F4;
		case InputKey::F5:            return SDL_SCANCODE_F5;
		case InputKey::F6:            return SDL_SCANCODE_F6;
		case InputKey::F7:            return SDL_SCANCODE_F7;
		case InputKey::F8:            return SDL_SCANCODE_F8;
		case InputKey::F9:            return SDL_SCANCODE_F9;
		case InputKey::F10:           return SDL_SCANCODE_F10;
		case InputKey::F11:           return SDL_SCANCODE_F11;
		case InputKey::F12:           return SDL_SCANCODE_F12;
		case InputKey::F13:           return SDL_SCANCODE_F13;
		case InputKey::F14:           return SDL_SCANCODE_F14;
		case InputKey::F15:           return SDL_SCANCODE_F15;
		case InputKey::F16:           return SDL_SCANCODE_F16;
		case InputKey::F17:           return SDL_SCANCODE_F17;
		case InputKey::F18:           return SDL_SCANCODE_F18;
		case InputKey::F19:           return SDL_SCANCODE_F19;
		case InputKey::F20:           return SDL_SCANCODE_F20;
		case InputKey::F21:           return SDL_SCANCODE_F21;
		case InputKey::F22:           return SDL_SCANCODE_F22;
		case InputKey::F23:           return SDL_SCANCODE_F23;
		case InputKey::F24:           return SDL_SCANCODE_F24;
		case InputKey::NumLock:       return SDL_SCANCODE_NUMLOCKCLEAR;
		case InputKey::ScrollLock:    return SDL_SCANCODE_SCROLLLOCK;
		case InputKey::LShift:        return SDL_SCANCODE_LSHIFT;
		case InputKey::RShift:        return SDL_SCANCODE_RSHIFT;
		case InputKey::LControl:      return SDL_SCANCODE_LCTRL;
		case InputKey::RControl:      return SDL_SCANCODE_RCTRL;
		case InputKey::Tilde:         return SDL_SCANCODE_GRAVE;
		case InputKey::Semicolon:     return SDL_SCANCODE_SEMICOLON;
		case InputKey::Equals:        return SDL_SCANCODE_EQUALS;
		case InputKey::Comma:         return SDL_SCANCODE_COMMA;
		case InputKey::Minus:         return SDL_SCANCODE_MINUS;
		case InputKey::Period:        return SDL_SCANCODE_PERIOD;
		case InputKey::Slash:         return SDL_SCANCODE_SLASH;
		case InputKey::LeftBracket:   return SDL_SCANCODE_LEFTBRACKET;
		case InputKey::Backslash:     return SDL_SCANCODE_BACKSLASH;
		case InputKey::RightBracket:  return SDL_SCANCODE_RIGHTBRACKET;
		case InputKey::SingleQuote:   return SDL_SCANCODE_APOSTROPHE;
		case InputKey::GreyStar:      return SDL_SCANCODE_KP_MULTIPLY;
		case InputKey::GreyPlus:      return SDL_SCANCODE_KP_PLUS;
		case InputKey::GreyMinus:     return SDL_SCANCODE_KP_MINUS;
		case InputKey::GreySlash:     return SDL_SCANCODE_KP_DIVIDE;
		default:                      return SDL_SCANCODE_UNKNOWN;
	}
}
