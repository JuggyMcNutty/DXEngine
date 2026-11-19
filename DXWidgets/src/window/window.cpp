
#include "window/window.h"
#include "window/stub/stub_open_folder_dialog.h"
#include "window/stub/stub_open_file_dialog.h"
#include "window/stub/stub_save_file_dialog.h"
#include "window/sdlnativehandle.h"
#include "core/widget.h"
#include <stdexcept>
#include <algorithm>

std::unique_ptr<DisplayWindow> DisplayWindow::Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI)
{
	if (type == WidgetType::Child)
		throw std::runtime_error("WidgetType.Child not allowed for DisplayWindow");
	return DisplayBackend::Get()->Create(windowHost, type, owner, renderAPI);
}

void DisplayWindow::ProcessEvents()
{
	DisplayBackend::Get()->ProcessEvents();
}

void DisplayWindow::RunLoop()
{
	DisplayBackend::Get()->RunLoop();
}

void DisplayWindow::ExitLoop()
{
	DisplayBackend::Get()->ExitLoop();
}

void* DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return DisplayBackend::Get()->StartTimer(timeoutMilliseconds, onTimer);
}

void DisplayWindow::StopTimer(void* timerID)
{
	DisplayBackend::Get()->StopTimer(timerID);
}

Size DisplayWindow::GetScreenSize()
{
	return DisplayBackend::Get()->GetScreenSize();
}

/////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<DisplayBackend>& GetBackendVar()
{
	// In C++, static variables in functions are constructed on first encounter and is destructed in the reverse order when main() ends.
	static std::unique_ptr<DisplayBackend> p;
	return p;
}

DisplayBackend* DisplayBackend::Get()
{
	return GetBackendVar().get();
}

void DisplayBackend::Set(std::unique_ptr<DisplayBackend> instance)
{
	GetBackendVar() = std::move(instance);
}

static std::vector<std::string>& GetMessagesVar()
{
	static std::vector<std::string> messages;
	return messages;
}

// A line already waiting is not added again: a backend asked for by
// DXE_DISPLAY_BACKEND is tried again in the order after it
void DisplayBackend::AddMessage(std::string text)
{
	auto& messages = GetMessagesVar();
	if (std::find(messages.begin(), messages.end(), text) == messages.end())
		messages.push_back(std::move(text));
}

std::vector<std::string> DisplayBackend::TakeMessages()
{
	std::vector<std::string> messages;
	messages.swap(GetMessagesVar());
	return messages;
}

// A backend that did not start: its reason kept for the host's log, the next
// one tried (bugs.md WIN.48)
[[maybe_unused]] static std::unique_ptr<DisplayBackend> NotStarted(const char* name, const char* reason)
{
	DisplayBackend::AddMessage(std::string("Window: no ") + name + " window: " + (reason && *reason ? reason : "an unknown error"));
	return nullptr;
}

std::unique_ptr<OpenFileDialog> DisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	return std::make_unique<StubOpenFileDialog>(owner);
}

std::unique_ptr<SaveFileDialog> DisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	return std::make_unique<StubSaveFileDialog>(owner);
}

std::unique_ptr<OpenFolderDialog> DisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	return std::make_unique<StubOpenFolderDialog>(owner);
}

#ifdef _MSC_VER
#pragma warning(disable: 4996) // warning C4996 : 'getenv' : This function or variable may be unsafe.Consider using _dupenv_s instead.To disable deprecation, use _CRT_SECURE_NO_WARNINGS.See online help for details.
#endif

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateBackend()
{
	std::unique_ptr<DisplayBackend> backend;
	// Check if there is an environment variable specified for the desired backend
	const char* backendSelectionEnv = std::getenv("DXE_DISPLAY_BACKEND");
	if (backendSelectionEnv)
	{
		std::string backendSelectionStr(backendSelectionEnv);
		size_t said = GetMessagesVar().size();
		if (backendSelectionStr == "Win32")
		{
			backend = TryCreateWin32();
		}
		else if (backendSelectionStr == "X11")
		{
			backend = TryCreateX11();
		}
		else if (backendSelectionStr == "SDL3")
		{
			backend = TryCreateSDL3();
		}
		else if (backendSelectionStr == "SDL2")
		{
			backend = TryCreateSDL2();
		}
		// One this build lacks, or a name of none, said as a failure is
		if (!backend && GetMessagesVar().size() == said)
			AddMessage("Window: no " + backendSelectionStr + " window (DXE_DISPLAY_BACKEND): none in this build");
	}

	if (!backend)
	{
#ifdef WIN32
		// SDL3's window before Windows' own where it is built: the Win32 one
		// has none of what the engine's window has since gained -- the
		// display modes, the pad, a timeline's keys.
		backend = TryCreateSDL3();
		if (!backend) backend = TryCreateWin32();
#else
		backend = TryCreateWin32();
		if (!backend) backend = TryCreateWayland();
		if (!backend) backend = TryCreateX11();
		if (!backend) backend = TryCreateSDL3();
#endif
		if (!backend) backend = TryCreateSDL2();
	}

	return backend;
}

#ifdef WIN32

#include "win32/win32_display_backend.h"

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateWin32()
{
	return std::make_unique<Win32DisplayBackend>();
}

#else

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateWin32()
{
	return nullptr;
}

#endif

#ifdef USE_SDL2

#include "sdl2/sdl2_display_backend.h"

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateSDL2()
{
	// A failed SDL_Init throws: the next backend is tried, as after X11's
	// and Wayland's
	try
	{
		return std::make_unique<SDL2DisplayBackend>();
	}
	catch (const std::exception& e)
	{
		return NotStarted("SDL2", e.what());
	}
	catch (...)
	{
		return NotStarted("SDL2", nullptr);
	}
}

#else

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateSDL2()
{
	return nullptr;
}

#endif

#ifdef USE_SDL3

#include "sdl3/sdl3_display_backend.h"

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateSDL3()
{
	// A failed SDL_Init throws: the next backend is tried (on Windows, the
	// Win32 window), as after X11's and Wayland's
	try
	{
		return std::make_unique<SDL3DisplayBackend>();
	}
	catch (const std::exception& e)
	{
		return NotStarted("SDL3", e.what());
	}
	catch (...)
	{
		return NotStarted("SDL3", nullptr);
	}
}

#else

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateSDL3()
{
	return nullptr;
}

#endif

#ifdef USE_X11

#include "x11/x11_display_backend.h"

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateX11()
{
	try
	{
		return std::make_unique<X11DisplayBackend>();
	}
	catch (const std::exception& e)
	{
		return NotStarted("X11", e.what());
	}
	catch (...)
	{
		return NotStarted("X11", nullptr);
	}
}

#else

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateX11()
{
	return nullptr;
}

#endif

#ifdef USE_WAYLAND

#include "wayland/wayland_display_backend.h"

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateWayland()
{
	try
	{
		return std::make_unique<WaylandDisplayBackend>();
	}
	catch (const std::exception& e)
	{
		return NotStarted("Wayland", e.what());
	}
	catch (...)
	{
		return NotStarted("Wayland", nullptr);
	}
}

#else

std::unique_ptr<DisplayBackend> DisplayBackend::TryCreateWayland()
{
	return nullptr;
}

#endif

