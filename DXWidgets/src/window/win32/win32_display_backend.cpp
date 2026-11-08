
#include "win32_display_backend.h"
#include "win32_display_window.h"
#include "win32_open_file_dialog.h"
#include "win32_save_file_dialog.h"
#include "win32_open_folder_dialog.h"

std::unique_ptr<DisplayWindow> Win32DisplayBackend::Create(DisplayWindowHost* windowHost, WidgetType type, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<Win32DisplayWindow>(windowHost, type, static_cast<Win32DisplayWindow*>(owner), renderAPI);
}

void Win32DisplayBackend::ProcessEvents()
{
	Win32DisplayWindow::ProcessEvents();
}

void Win32DisplayBackend::RunLoop()
{
	Win32DisplayWindow::RunLoop();
}

void Win32DisplayBackend::ExitLoop()
{
	Win32DisplayWindow::ExitLoop();
}

// The display the windows are on, where full screen goes
// (Win32DisplayWindow::ShowFullscreen): a window's, or the primary display
// before there is one. False where Windows tells none.
static bool WindowsMonitor(MONITORINFO& info, HMONITOR* found = nullptr)
{
	HMONITOR monitor = nullptr;
	for (Win32DisplayWindow* window : Win32DisplayWindow::Windows)
	{
		if (window->WindowHandle.hwnd && !window->PopupWindow)
		{
			monitor = MonitorFromWindow(window->WindowHandle.hwnd, MONITOR_DEFAULTTONEAREST);
			break;
		}
	}
	if (!monitor)
		monitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
	info = {};
	info.cbSize = sizeof(MONITORINFO);
	if (found)
		*found = monitor;
	return monitor && GetMonitorInfo(monitor, &info);
}

// That display's scale, by its own DPI (GetDpiForMonitor, Windows 8.1 and
// later), as a window on it takes its own (GetDpiForWindow), which the game
// multiplies the size by (GameWindow::GetMenuResolutions): the system's DPI
// is the primary display's, so on a desktop of mixed DPI the windowed sizes
// offered were cut short or did not fit. The system's where there is none
typedef HRESULT(WINAPI* PFN_GetDpiForMonitor)(HMONITOR monitor, int dpiType, UINT* dpiX, UINT* dpiY);

static double MonitorDpiScale(HMONITOR monitor)
{
	static PFN_GetDpiForMonitor getDpiForMonitor = []() -> PFN_GetDpiForMonitor
	{
		HMODULE shcore = LoadLibraryW(L"shcore.dll");
		return shcore ? (PFN_GetDpiForMonitor)GetProcAddress(shcore, "GetDpiForMonitor") : nullptr;
	}();
	UINT dpiX = 0, dpiY = 0;
	if (monitor && getDpiForMonitor && SUCCEEDED(getDpiForMonitor(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dpiX, &dpiY)) && dpiX > 0)
		return dpiX / 96.0;

	HDC screenDC = GetDC(0);
	double dpiScale = GetDeviceCaps(screenDC, LOGPIXELSX) / 96.0;
	ReleaseDC(0, screenDC);
	return dpiScale;
}

Size Win32DisplayBackend::GetScreenSize()
{
	MONITORINFO info;
	HMONITOR monitor = nullptr;
	if (!WindowsMonitor(info, &monitor))
		return Win32DisplayWindow::GetScreenSize();
	double dpiScale = MonitorDpiScale(monitor);
	return Size((info.rcMonitor.right - info.rcMonitor.left) / dpiScale, (info.rcMonitor.bottom - info.rcMonitor.top) / dpiScale);
}

// That display's work area, its taskbar left out, by the DPI
// GetScreenSize's size takes
Rect Win32DisplayBackend::GetScreenWorkArea()
{
	MONITORINFO info;
	HMONITOR monitor = nullptr;
	if (!WindowsMonitor(info, &monitor))
		return DisplayBackend::GetScreenWorkArea();
	const RECT& rect = info.rcWork;
	double dpiScale = MonitorDpiScale(monitor);
	return Rect::xywh(rect.left / dpiScale, rect.top / dpiScale, (rect.right - rect.left) / dpiScale, (rect.bottom - rect.top) / dpiScale);
}

// That display's size alone: no mode is changed here. In pixels, as the
// engine's resolution list takes them, not GetScreenSize's DIPs.
std::vector<Size> Win32DisplayBackend::GetFullscreenModes()
{
	MONITORINFO info;
	if (WindowsMonitor(info))
		return { Size(info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top) };
	HDC screenDC = GetDC(0);
	int screenWidth = GetDeviceCaps(screenDC, HORZRES);
	int screenHeight = GetDeviceCaps(screenDC, VERTRES);
	ReleaseDC(0, screenDC);
	return { Size(screenWidth, screenHeight) };
}

void* Win32DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return Win32DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void Win32DisplayBackend::StopTimer(void* timerID)
{
	Win32DisplayWindow::StopTimer(timerID);
}

std::unique_ptr<OpenFileDialog> Win32DisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32OpenFileDialog>(static_cast<Win32DisplayWindow*>(owner));
}

std::unique_ptr<SaveFileDialog> Win32DisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32SaveFileDialog>(static_cast<Win32DisplayWindow*>(owner));
}

std::unique_ptr<OpenFolderDialog> Win32DisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32OpenFolderDialog>(static_cast<Win32DisplayWindow*>(owner));
}
