#pragma once

#include <unordered_map>
#include <mutex>
#include <dxwidgets/window/window.h>
#include <dxwidgets/window/sdlnativehandle.h>
#include <SDL3/SDL.h>

#include "dxwidgets/core/image.h"

class SDL3DisplayWindow : public DisplayWindow
{
public:
	SDL3DisplayWindow(DisplayWindowHost* windowHost, WidgetType type, SDL3DisplayWindow* owner, RenderAPI renderAPI);
	~SDL3DisplayWindow();

	void SetWindowTitle(const std::string& text) override;
	void SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images) override;
	void SetClientFrame(const Rect& box) override;
	void Show() override;
	void ShowFullscreen() override;
	void SetFullscreenMode(int width, int height) override;
	void ShowMaximized() override;
	void ShowMinimized() override;
	void ShowNormal() override;
	void SetWindowResizable(bool enable) override;
	void SetWindowMinSize(int width, int height) override;
	void SetWindowMaxSize(int width, int height) override;
	bool IsWindowFullscreen() override;
	void Hide() override;
	void Activate() override;
	void ShowCursor(bool enable) override;
	void LockKeyboard() override;
	void UnlockKeyboard() override;
	void LockCursor() override;
	void UnlockCursor() override;
	void CaptureMouse() override;
	void ReleaseMouseCapture() override;
	void Update() override;
	bool GetKeyState(InputKey key) override;
	void SetCursor(StandardCursor cursor, std::shared_ptr<CustomCursor> custom) override;

	Rect GetClientFrame() const override;
	Size GetClientSize() const override;
	int GetPixelWidth() const override;
	int GetPixelHeight() const override;
	double GetDpiScale() const override;
	// SDL's window units per DIP: the display scale over the pixel density.
	// A high-pixel-density window's units on Wayland are points, its density
	// its scale, so the scale is applied once, not twice
	double UnitsPerDip() const;

	void PresentBitmap(int width, int height, const uint32_t* pixels) override;
	void SetBitmapSwapInterval(int interval) override;

	void SetBorderColor(uint32_t rgba8) override;
	void SetCaptionColor(uint32_t rgba8) override;
	void SetCaptionTextColor(uint32_t rgba8) override;

	std::string GetClipboardText() override;
	void SetClipboardText(const std::string& text) override;

	Point MapFromGlobal(const Point& pos) const override;
	Point MapToGlobal(const Point& pos) const override;

	void* GetNativeHandle() override { return &Handle; }
	void* GetWin32Handle() override;

	std::vector<std::string> GetVulkanInstanceExtensions() override;
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;

	static void DispatchEvent(const SDL_Event& event);
	static SDL3DisplayWindow* FindEventWindow(const SDL_Event& event);

	void OnWindowEvent(const SDL_WindowEvent& event);
	void OnTextInput(const SDL_TextInputEvent& event);
	void OnKeyUp(const SDL_KeyboardEvent& event);
	void OnKeyDown(const SDL_KeyboardEvent& event);
	void OnMouseButtonUp(const SDL_MouseButtonEvent& event);
	void OnMouseButtonDown(const SDL_MouseButtonEvent& event);
	void OnMouseWheel(const SDL_MouseWheelEvent& event);
	void OnMouseMotion(const SDL_MouseMotionEvent& event);
	void OnJoyButtonUp(const SDL_GamepadButtonEvent& event);
	void OnJoyButtonDown(const SDL_GamepadButtonEvent& event);
	void OnPaintEvent();
	static void OnTimerEvent(const SDL_UserEvent& event);

	InputKey GetMouseButtonKey(const SDL_MouseButtonEvent& event);

	static InputKey ScancodeToInputKey(SDL_Scancode keycode);
	static InputKey ScancodeToSideKey(SDL_Scancode keycode);
	static InputKey GameControllerButtonToInputKey(SDL_GamepadButton button);
	static SDL_Scancode InputKeyToScancode(InputKey inputkey);

	static SDL_PixelFormat ImageFormatToSDLPixelFormat(const ImageFormat& format);
	static SDL_Surface* CreateIconSurface(const Image& image);

	template<typename T>
	Point GetMousePos(const T& event)
	{
		double units = UnitsPerDip();
		return Point(event.x / units, event.y / units);
	}

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();

	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);
	static Uint32 ExecTimer(void* id, SDL_TimerID timerID, Uint32 interval);

	DisplayWindowHost* WindowHost = nullptr;
	SDLNativeHandle Handle;
	SDL_Renderer* RendererHandle = nullptr;
	SDL_Texture* BackBufferTexture = nullptr;
	int BackBufferWidth = 0;
	int BackBufferHeight = 0;

	SDL_Surface* windowIconSurface = nullptr;

	bool CursorLocked = false;
	// The relative motion's fractions not yet passed on
	float RawMoveFractionX = 0.0f;
	float RawMoveFractionY = 0.0f;
	bool isFullscreen = false;

	static bool ExitRunLoop;
	static Uint32 PaintEventNumber;
	// Controller buttons become key presses unless the application reads the
	// pad itself (DisplayBackend::SetGamepadKeyEmulation).
	static bool GamepadKeyEmulation;
	// The keys a timeline holds down (DisplayBackend::SimulateKey)
	static bool SimulatedKeys[SDL_SCANCODE_COUNT];
	static std::unordered_map<unsigned int, SDL3DisplayWindow*> WindowList;

	static std::unordered_map<SDL_TimerID, void *> TimerHandles;
	static std::unordered_map<void *, std::function<void()>> Timers;
	static std::mutex TimersMutex; // SDL's timer thread reads Timers
	static unsigned long TimerIDs;
	static Uint32 TimerEventNumber;

	SDL_GLContext m_GLContext = nullptr;
	bool m_GLIsES = false; // RenderAPI::GLES: create an OpenGL ES context

	void CreateGLContext() override;
	void MakeGLContextCurrent() override;
	bool SetGLSwapInterval(int interval) override;
	void SwapGLBuffers() override;
	GLFuncPtr GetGLProcAddress(const char* name) override;
};
