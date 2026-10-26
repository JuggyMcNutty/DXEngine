
#include "Precomp.h"
#include "GameWindow.h"
#include "LauncherSettings.h"
#include "RenderDevice/RenderDevice.h"
#include <dxgpu/vulkansurface.h>
#include <dxgpu/vulkancompatibledevice.h>
#include <dxgpu/vulkanbuilders.h>

GameWindow::GameWindow(GameWindowHost* windowHost, RenderAPI renderAPI) : Widget(nullptr, WidgetType::Window, renderAPI), windowHost(windowHost)
{
	device = RenderDevice::Create(this, renderAPI);
	SetCanvas(std::make_unique<RenderDeviceCanvas>(device.get()));
	SetFocus();
}

RenderDevice* GameWindow::GetRenderDevice()
{
	return device.get();
}

// The size the game draws at: the window's, unless the render device draws
// the scene smaller and scales it up (RenderDevice::GetRenderScale).
int GameWindow::GetPixelWidth()
{
	return device ? device->GetRenderWidth() : GetNativePixelWidth();
}

int GameWindow::GetPixelHeight()
{
	return device ? device->GetRenderHeight() : GetNativePixelHeight();
}

void GameWindow::ToggleWindowFullscreen(Size newResolution)
{
	if (this->IsFullscreen())
	{
		// First switch to normal
		this->ShowNormal();

		// Then resize the window
		auto geometry = this->GetFrameGeometry();
		geometry.width = newResolution.width;
		geometry.height = newResolution.height;
		this->SetFrameGeometry(geometry);
	}
	else
	{
		// Full screen in the display's mode nearest the size, or the
		// desktop's for none (0 x 0)
		if (newResolution.width > 0 && newResolution.height > 0)
		{
			Size mode = GetClosestResolution(newResolution);
			SetFullscreenMode((int)mode.width, (int)mode.height);
		}
		else
		{
			SetFullscreenMode(0, 0);
		}
		this->ShowFullscreen();
	}
}

bool GameWindow::GetKeyState(EInputKey key)
{
	return Widget::GetKeyState((InputKey)key);
}

// The display's full screen modes (DisplayBackend::GetFullscreenModes).
Array<Size> GameWindow::QueryAvailableResolutions() const
{
	Array<Size> sizes;
	for (const Size& size : DisplayBackend::Get()->GetFullscreenModes())
		sizes.push_back(size);
	return sizes;
}

// A paint event (the window shown, exposed, resized, its full screen
// toggled) draws nothing here: the game draws every frame itself (DrawGame),
// and Widget's repaint would lock the render device cleared to the theme's
// window background and present that, a solid frame between the game's.
void GameWindow::OnWindowPaint()
{
}

void GameWindow::OnPaint(Canvas* canvas)
{
	windowHost->OnWindowPaint();
}

bool GameWindow::OnMouseDown(const Point& pos, InputKey key)
{
	windowHost->OnWindowMouseDown(pos, (EInputKey)key);
	return true;
}

bool GameWindow::OnMouseDoubleclick(const Point& pos, InputKey key)
{
	windowHost->OnWindowMouseDoubleclick(pos, (EInputKey)key);
	return true;
}

bool GameWindow::OnMouseUp(const Point& pos, InputKey key)
{
	windowHost->OnWindowMouseUp(pos, (EInputKey)key);
	return true;
}

bool GameWindow::OnMouseWheel(const Point& pos, InputKey key)
{
	windowHost->OnWindowMouseWheel(pos, (EInputKey)key);
	return true;
}

void GameWindow::OnMouseMove(const Point& pos)
{
	windowHost->OnWindowMouseMove(pos);
}

void GameWindow::OnRawMouseMove(int dx, int dy)
{
	windowHost->OnWindowRawMouseMove(dx, dy);
}

void GameWindow::OnKeyChar(std::string chars)
{
	windowHost->OnWindowKeyChar(chars);
}

void GameWindow::OnKeyDown(InputKey key)
{
	windowHost->OnWindowKeyDown((EInputKey)key);
}

void GameWindow::OnKeyUp(InputKey key)
{
	windowHost->OnWindowKeyUp((EInputKey)key);
}

void GameWindow::OnGeometryChanged()
{
	windowHost->OnWindowGeometryChanged();
}

void GameWindow::OnClose()
{
	windowHost->OnWindowClose();
}

void GameWindow::OnSetFocus()
{
	windowHost->OnWindowActivated();
}

void GameWindow::OnLostFocus()
{
	windowHost->OnWindowDeactivated();
}

std::unique_ptr<GameWindow> GameWindow::Create(GameWindowHost* windowHost, RenderDeviceType type)
{
	RenderAPI api;
	switch (type)
	{
	default:
	case RenderDeviceType::Vulkan: api = RenderAPI::Vulkan; break;
	case RenderDeviceType::D3D11: api = RenderAPI::D3D11; break;
	case RenderDeviceType::OpenGL: api = RenderAPI::OpenGL; break;
	case RenderDeviceType::GLES: api = RenderAPI::GLES; break;
	case RenderDeviceType::Software: api = RenderAPI::Bitmap; break;
	}
	return std::make_unique<GameWindow>(windowHost, api);
}

void GameWindow::ProcessEvents()
{
	DisplayBackend::Get()->ProcessEvents();
}

void GameWindow::RunLoop()
{
	DisplayBackend::Get()->RunLoop();
}

void GameWindow::ExitLoop()
{
	DisplayBackend::Get()->ExitLoop();
}

// Deus Ex's Display menu reads the sizes it offers from GETRES, smallest
// first with a space between, as the original lists its display modes. It
// takes at most 16, and none from a list with no space. Full screen, the
// display's modes from 640x480 are offered, as the original's; in a window,
// which takes any size, the common sizes from 640x480 that fit the screen,
// with the window's own size and the screen's. A list of one is followed by
// the space the menu needs to read it.
std::string GameWindow::GetMenuResolutions()
{
	Size screen = DisplayBackend::Get()->GetScreenSize();
	Size current((double)GetPixelWidth(), (double)GetPixelHeight());

	Array<Size> sizes;
	if (IsFullscreen())
	{
		for (const Size& mode : QueryAvailableResolutions())
		{
			if (mode.width >= 640 && mode.height >= 480)
				AddResolutionIfNotAdded(sizes, mode);
		}
		AddResolutionIfNotAdded(sizes, current);
		std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) { return a.width != b.width ? a.width < b.width : a.height < b.height; });
		// The menu's 16 keep the current mode: the smallest others go first.
		for (size_t i = 0; sizes.size() > 16 && i < sizes.size();)
		{
			if (sizes[i] == current)
				i++;
			else
				sizes.erase(sizes.begin() + i);
		}
	}
	else
	{
		static const int common[][2] =
		{
			{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 800 },
			{ 1280, 960 }, { 1280, 1024 }, { 1366, 768 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 },
			{ 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 2560, 1600 }, { 3840, 2160 }
		};
		for (auto& size : common)
		{
			if (size[0] <= screen.width && size[1] <= screen.height)
				AddResolutionIfNotAdded(sizes, Size(size[0], size[1]));
		}
		AddResolutionIfNotAdded(sizes, current);
		AddResolutionIfNotAdded(sizes, screen);
		std::sort(sizes.begin(), sizes.end(), [](const Size& a, const Size& b) { return a.width != b.width ? a.width < b.width : a.height < b.height; });
		// The menu's 16 keep the window's own size and the screen's: the
		// smallest others go first.
		for (size_t i = 0; sizes.size() > 16 && i < sizes.size();)
		{
			if (sizes[i] == current || sizes[i] == screen)
				i++;
			else
				sizes.erase(sizes.begin() + i);
		}
	}

	std::string result;
	for (size_t i = 0; i < sizes.size(); i++)
	{
		if (i > 0)
			result += " ";
		result += std::to_string((int)sizes[i].width) + "x" + std::to_string((int)sizes[i].height);
	}
	if (sizes.size() == 1)
		result += " ";
	return result;
}

void GameWindow::AddResolutionIfNotAdded(Array<Size>& resList, Size resolution) const
{
	// Skip over the current resolution if it is already inserted
	// (in case of multiple refresh rates being available for the display)
	for (auto& res : resList)
	{
		if (resolution == res)
			return;
	}

	// Add the resolution, as it is not added before
	resList.push_back(resolution);
}

Size GameWindow::ParseResolutionString(const std::string& resolutionString) const
{
	if (resolutionString.empty())
		return Size(0, 0);

#ifdef WIN32
	int width, height;
	int parsedDataCount = sscanf_s(resolutionString.c_str(), "%dx%d", &width, &height);
#else
	int width, height;
	int parsedDataCount = sscanf(resolutionString.c_str(), "%dx%d", &width, &height);
#endif

	// Handle incorrect parsings
	// sscanf() returns EOF on input failure, or the amount of "data processed" otherwise
	// Since we want to process width and height of a window, we expect it to return 2
	if (parsedDataCount == EOF || parsedDataCount != 2)
	{
		return Size(0, 0);
	}

	// Resolution shouldn't be smaller than 640x480
	if (width < 640 || height < 480)
	{
		// Maybe produce a log here too?
		return Size(0, 0);
	}

	return Size(width, height);
}

Size GameWindow::GetClosestResolution(Size resolution) const
{
	auto resolutions = QueryAvailableResolutions();

	if (resolutions.empty())
		return resolution;

	if (resolutions.size() == 1)
		return resolutions[0];

	int index = 0;
	double minDist = std::abs(pow(resolutions[0].width - resolution.width, 2) + pow(resolutions[0].height - resolution.height, 2));

	for (int i = 1; i < resolutions.size(); i++)
	{
		auto& currRes = resolutions[i];

		double dist = std::abs(pow(currRes.width - resolution.width, 2) + pow(currRes.height - resolution.height, 2));

		if (currRes == resolution)
			return resolutions[i];

		if (dist < minDist)
		{
			minDist = dist;
			index = i;
		}
	}

	return resolutions[index];
}

void GameWindow::SetResolution(const std::string& resolutionString)
{
	Size parsedResolution = ParseResolutionString(resolutionString);
	if (parsedResolution == Size(0, 0))
		return;

	// Full screen, the display's mode nearest the size: the window takes
	// the mode's size.
	if (IsFullscreen())
	{
		parsedResolution = GetClosestResolution(parsedResolution);
		SetFullscreenMode((int)parsedResolution.width, (int)parsedResolution.height);
		return;
	}

	Rect windowRect = GetFrameGeometry();
	auto dpi = GetDpiScale();
	windowRect.width = parsedResolution.width / dpi;
	windowRect.height = parsedResolution.height / dpi;

	SetFrameGeometry(windowRect);
}
