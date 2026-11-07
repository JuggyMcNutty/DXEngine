
#include "x11_display_backend.h"
#include "x11_display_window.h"
#include "x11_connection.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#ifdef USE_DBUS
#include "window/dbus/dbus_open_file_dialog.h"
#include "window/dbus/dbus_save_file_dialog.h"
#include "window/dbus/dbus_open_folder_dialog.h"
#endif

// The display opened as the backend is made: with no X server the
// connection throws here, and the next backend is tried (DisplayBackend's
// TryCreateX11 catches it), not at the first window
X11DisplayBackend::X11DisplayBackend()
{
	GetX11Connection();
}

std::unique_ptr<DisplayWindow> X11DisplayBackend::Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<X11DisplayWindow>(windowHost, type, static_cast<X11DisplayWindow*>(owner), renderAPI);
}

void X11DisplayBackend::ProcessEvents()
{
	GetX11Connection()->ProcessEvents();
}

void X11DisplayBackend::RunLoop()
{
	GetX11Connection()->RunLoop();
}

void X11DisplayBackend::ExitLoop()
{
	GetX11Connection()->ExitLoop();
}

Size X11DisplayBackend::GetScreenSize()
{
	return GetX11Connection()->GetScreenSize();
}

// The screen's size alone: no mode is changed here. In pixels, as the
// engine's resolution list takes them, not GetScreenSize's DIPs.
std::vector<Size> X11DisplayBackend::GetFullscreenModes()
{
	return { GetX11Connection()->GetScreenPixelSize() };
}

void* X11DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return GetX11Connection()->StartTimer(timeoutMilliseconds, onTimer);
}

void X11DisplayBackend::StopTimer(void* timerID)
{
	GetX11Connection()->StopTimer(timerID);
}

#ifdef USE_DBUS
// The dialog's parent as the portal names a window: "x11:" and its id in
// hex, which the portal reads (SDL's own portal dialogs write it so); none
// without an owner
static std::string PortalParent(DisplayWindow* owner)
{
	if (!owner)
		return {};
	char handle[32];
	snprintf(handle, sizeof(handle), "x11:%lx", (unsigned long)reinterpret_cast<uintptr_t>(owner->GetNativeHandle()));
	return handle;
}

std::unique_ptr<OpenFileDialog> X11DisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	return std::make_unique<DBusOpenFileDialog>(PortalParent(owner));
}

std::unique_ptr<SaveFileDialog> X11DisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	return std::make_unique<DBusSaveFileDialog>(PortalParent(owner));
}

std::unique_ptr<OpenFolderDialog> X11DisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	return std::make_unique<DBusOpenFolderDialog>(PortalParent(owner));
}
#endif
