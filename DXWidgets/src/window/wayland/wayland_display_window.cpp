#include "wayland_display_window.h"

#include <cstring>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <dlfcn.h>
#include <pthread.h>

#include "core/image.h"

WaylandDisplayWindow::WaylandDisplayWindow(WaylandDisplayBackend* backend, DisplayWindowHost* windowHost, WidgetType type, WaylandDisplayWindow* owner, RenderAPI renderAPI)
	: backend(backend), m_owner(owner), windowHost(windowHost), m_WidgetType(type), m_renderAPI(renderAPI)
{
	m_WindowSurface = backend->m_waylandCompositor.create_surface();

	if (m_renderAPI == RenderAPI::OpenGL || m_renderAPI == RenderAPI::GLES)
	{
		m_EGLWindow = wayland::egl_window_t(m_WindowSurface, 320, 240);
	}

	m_NativeHandle.display = backend->s_waylandDisplay;
	m_NativeHandle.surface = m_WindowSurface;

	if (backend->m_Viewporter)
	{
		m_Viewport = backend->m_Viewporter.get_viewport(m_WindowSurface);

		if (backend->m_FractionalScaleManager)
		{
			m_FractionalScale = backend->m_FractionalScaleManager.get_fractional_scale(m_WindowSurface);

			m_FractionalScale.on_preferred_scale() = [&](uint32_t scale_numerator) {
				// parameter is the numerator of a fraction with the denominator of 120
				ApplyScale(scale_numerator / 120.0);
			};
		}
	}

	m_XDGSurface = backend->m_XDGWMBase.get_xdg_surface(m_WindowSurface);
	m_XDGSurface.on_configure() = [&] (uint32_t serial) {
		m_XDGSurface.ack_configure(serial);
	};

	if (m_WidgetType == WidgetType::Popup)
	{
		InitializePopup();
	}
	else
	{
		InitializeToplevel();
	}

	backend->OnWindowCreated(this);

	this->DrawSurface();
}

WaylandDisplayWindow::~WaylandDisplayWindow()
{
	if (m_EGLContext)
	{
		// No context takes no surfaces (EGL 3.7.3), as the X11 window's
		eglMakeCurrent(m_EGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(m_EGLDisplay, m_EGLContext);
		eglDestroySurface(m_EGLDisplay, m_EGLSurface);
		eglTerminate(m_EGLDisplay);

		m_EGLContext = nullptr;
		m_EGLSurface = nullptr;
		m_EGLDisplay = nullptr;
	}
	backend->OnWindowDestroyed(this);
}

void WaylandDisplayWindow::InitializeToplevel()
{
	m_XDGToplevel = m_XDGSurface.get_toplevel();
	m_XDGToplevel.set_title("ZWidget Window");

	if (m_WidgetType == WidgetType::Dialog)
		m_XDGDialog = backend->m_XDGWMDialog.get_xdg_dialog(m_XDGToplevel);

	if (m_owner)
		m_XDGToplevel.set_parent(m_owner->m_XDGToplevel);

	if (backend->m_XDGDecorationManager)
	{
		// Force server side decorations if possible
		m_XDGToplevelDecoration = backend->m_XDGDecorationManager.get_toplevel_decoration(m_XDGToplevel);
		m_XDGToplevelDecoration.set_mode(wayland::zxdg_toplevel_decoration_v1_mode::server_side);
	}

	m_WindowSurface.commit();

	backend->s_waylandDisplay.roundtrip();

	// These have to be added after the roundtrip
	m_XDGToplevel.on_configure() = [&] (int32_t width, int32_t height, wayland::array_t states) {
		OnXDGToplevelConfigureEvent(width, height, std::vector<wayland::xdg_toplevel_state>(states));
	};

	m_XDGToplevel.on_close() = [&] () {
		OnExitEvent();
	};

	m_XDGToplevel.on_configure_bounds() = [this] (int32_t width, int32_t height)
	{
		if (!isResizable)
			return;

		auto clientFrame = GetClientFrame();

		bool changed = false;

		if (width > 0 && clientFrame.width > width)
		{
			clientFrame.width = width;
			changed = true;
		}

		if (height > 0 && clientFrame.height > height)
		{
			clientFrame.height = height;
			changed = true;
		}

		if (changed)
		{
			SetClientFrame(clientFrame);
			windowHost->OnWindowGeometryChanged();
		}
	};

	m_XDGExported = backend->m_XDGExporter.export_toplevel(m_WindowSurface);

	m_XDGExported.on_handle() = [&] (std::string handleStr) {
		OnExportHandleEvent(handleStr);
	};
}

void WaylandDisplayWindow::InitializePopup()
{
	if (!m_owner)
		throw std::runtime_error("Popup window must have an owner!");

	wayland::xdg_positioner_t popupPositioner = backend->m_XDGWMBase.create_positioner();

	// Assign dummy values to the positioner.
	popupPositioner.set_anchor(wayland::xdg_positioner_anchor::bottom);
	popupPositioner.set_anchor_rect(0, 0, 1, 30);
	popupPositioner.set_size(1, 1);

	m_XDGPopup = m_XDGSurface.get_popup(m_owner->m_XDGSurface, popupPositioner);

	m_XDGPopup.on_configure() = [&] (int32_t x, int32_t y, int32_t width, int32_t height) {
		//SetClientFrame(Rect::xywh(x, y, width, height));
	};

	m_XDGPopup.on_popup_done() = [&] () {
		OnExitEvent();
	};

	m_WindowSurface.commit();

	backend->s_waylandDisplay.roundtrip();
}


void WaylandDisplayWindow::SetWindowTitle(const std::string& text)
{
	if (m_XDGToplevel)
		m_XDGToplevel.set_title(text);
}

void WaylandDisplayWindow::SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images)
{
	if (backend->m_XDGToplevelIconManager)
	{
		if (images.empty())
		{
			backend->m_XDGToplevelIconManager.set_icon(m_XDGToplevel, nullptr);
			return;
		}

		m_XDGToplevelIcon = backend->m_XDGToplevelIconManager.create_icon();

		CreateAppIconBuffers(images);

		for (auto& icon_buffer : appIconBuffers)
		{
			m_XDGToplevelIcon.add_buffer(icon_buffer, 1);
		}

		backend->m_XDGToplevelIconManager.set_icon(m_XDGToplevel, m_XDGToplevelIcon);
	}
}

// A new scale (the fractional scale's, or wl_output's whole one): the window
// keeps its size in surface units, its buffers and its EGL window made at its
// pixels at the new scale, and the host told its size changed, as SDL's
// window tells of its pixels
void WaylandDisplayWindow::ApplyScale(double scale)
{
	if (scale <= 0.0 || scale == m_ScaleFactor)
		return;
	m_ScaleFactor = scale;
	if (m_minPixelWidth > 0)
		SetWindowMinSize(m_minPixelWidth, m_minPixelHeight);
	if (m_ClientSize.width > 0 && m_ClientSize.height > 0)
	{
		CreateBuffers((int32_t)m_ClientSize.width, (int32_t)m_ClientSize.height);
		m_WindowSurface.commit();
		windowHost->OnWindowGeometryChanged();
	}

	m_NeedsUpdate = true;
	windowHost->OnWindowDpiScaleChanged();
}

void WaylandDisplayWindow::SetClientSize(const Size& size)
{
	// A Wayland window has no place of its own to keep: the compositor's
	CreateBuffers(size.width, size.height);
	windowHost->OnWindowGeometryChanged();
}

void WaylandDisplayWindow::SetClientFrame(const Rect& box)
{
	if (m_XDGPopup && m_owner)
	{
		m_OwnerOffset = m_owner->MapFromGlobal(box.topLeft());
		auto ownerRect = m_owner->GetClientFrame();
		auto positioner = backend->m_XDGWMBase.create_positioner();

		positioner.set_offset(0, 0);
		positioner.set_anchor(wayland::xdg_positioner_anchor::top_left);
		positioner.set_anchor_rect(m_OwnerOffset.x, m_OwnerOffset.y, ownerRect.width, ownerRect.height);

		m_XDGPopup.reposition(positioner, 0);
	}

	// Resizing will be shown on the next commit
	CreateBuffers(box.width, box.height);
	windowHost->OnWindowGeometryChanged();
	m_NeedsUpdate = true;
	m_WindowSurface.commit();
}

// A bitmap window shown with its newest frame, or one of black at its size
// till it draws; a device window's buffers are EGL's or Vulkan's, the window
// shown by its first frame, never by an SHM buffer of ours under them
void WaylandDisplayWindow::Show()
{
	if (!IsBitmapWindow())
	{
		m_WindowSurface.commit();
		return;
	}

	// A frame asked for before a Hide is never answered: asked again
	m_FrameCallback = wayland::callback_t();
	m_FramePending = false;
	if (!m_ShmFront && m_WindowSize.width > 0 && m_WindowSize.height > 0)
	{
		m_ShmFront = AcquireShmBuffer((int)m_WindowSize.width, (int)m_WindowSize.height);
		if (m_ShmFront)
			std::memset(m_ShmFront->mem->get_mem(), 0, (size_t)m_ShmFront->width * m_ShmFront->height * 4);
	}
	if (m_ShmFront)
		m_ShmFront->attached = false;
	DrawSurface();
}

// A maximized window is unset first: one maximized under full screen comes
// back from it maximized, which the game's window would take as Maximize
// again (OnXDGToplevelConfigureEvent)
void WaylandDisplayWindow::ShowFullscreen()
{
	if (m_XDGToplevel)
	{
		if (isMaximized)
		{
			m_XDGToplevel.unset_maximized();
			isMaximized = false;
			unmaximizing = true;
		}
		// On the output the window is on, which the compositor chooses for
		// no output given, not always the first output
		m_XDGToplevel.set_fullscreen(wayland::output_t());
		isFullscreen = true;
	}
}

void WaylandDisplayWindow::ShowMaximized()
{
	if (m_XDGToplevel)
	{
		m_XDGToplevel.set_maximized();
		isMaximized = true;
	}
}

void WaylandDisplayWindow::ShowMinimized()
{
	if (m_XDGToplevel)
	{
		m_XDGToplevel.set_minimized();
		isMaximized = false;
	}
}

// Out of full screen and not maximized: a compositor that keeps a full
// screen window maximized brings it back so
void WaylandDisplayWindow::ShowNormal()
{
	if (m_XDGToplevel)
	{
		bool wasFullscreen = isFullscreen;
		if (isFullscreen)
		{
			m_XDGToplevel.unset_fullscreen();
			isFullscreen = false;
		}
		if (wasFullscreen || isMaximized)
		{
			m_XDGToplevel.unset_maximized();
			isMaximized = false;
			unmaximizing = true;
		}
	}
}

void WaylandDisplayWindow::SetWindowResizable(bool enable)
{
	isResizable = enable;
	auto rect = GetClientFrame();
	if (!enable)
	{
		m_XDGToplevel.set_min_size(rect.width, rect.height);
		m_XDGToplevel.set_max_size(rect.width, rect.height);
	}
	else
	{
		m_XDGToplevel.set_min_size(m_minWindowWidth, m_minWindowHeight);
		m_XDGToplevel.set_max_size(m_maxWindowWidth, m_maxWindowHeight);
	}
}

// The pixels in the toplevel's surface units, the scale's
void WaylandDisplayWindow::SetWindowMinSize(int width, int height)
{
	m_minPixelWidth = width;
	m_minPixelHeight = height;
	if (m_XDGToplevel)
	{
		m_minWindowWidth = (int)std::ceil(width / m_ScaleFactor);
		m_minWindowHeight = (int)std::ceil(height / m_ScaleFactor);
		if (isResizable)
			m_XDGToplevel.set_min_size(m_minWindowWidth, m_minWindowHeight);
	}
}

void WaylandDisplayWindow::SetWindowMaxSize(int width, int height)
{
	if (m_XDGToplevel)
	{
		m_XDGToplevel.set_max_size(width, height);
		m_maxWindowWidth = width;
		m_maxWindowHeight = height;
	}
}

bool WaylandDisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void WaylandDisplayWindow::Hide()
{
	// Apparently this is how hiding a window works
	// By attaching a null buffer to the surface
	// See: https://lists.freedesktop.org/archives/wayland-devel/2017-November/035963.html
	m_WindowSurface.attach(nullptr, 0, 0);
	m_WindowSurface.commit();
}

void WaylandDisplayWindow::Activate()
{
	m_WindowActivationToken = backend->m_XDGActivation.get_activation_token();
	// The token comes in its done event, after the commit: the activation
	// waits for it (an empty or stale token activated nothing), and the
	// compositor's configure says whether the window was activated
	m_WindowActivationToken.on_done() = [this](const std::string& newActivationToken)
	{
		m_ActivationTokenString = newActivationToken;
		backend->m_XDGActivation.activate(m_ActivationTokenString, m_WindowSurface);
	};

	m_WindowActivationToken.set_surface(m_WindowSurface);
	m_WindowActivationToken.set_serial(backend->GetKeyboardSerial(), backend->m_waylandSeat);
	m_WindowActivationToken.commit();

	backend->m_FocusWindow = this;
	backend->m_MouseFocusWindow = this;
}

void WaylandDisplayWindow::ShowCursor(bool enable)
{
	backend->ShowCursor(enable);
}

// Typed text off in play, as the engine asks: the backend sends this window
// no characters while locked, its keys still
void WaylandDisplayWindow::LockKeyboard()
{
	keyboardLocked = true;
}

void WaylandDisplayWindow::UnlockKeyboard()
{
	keyboardLocked = false;
}

// The pointer locked and hidden in play, the game drawing its own, as the
// X11 and SDL windows hide it; shown again as it is let go
void WaylandDisplayWindow::LockCursor()
{
	m_LockedPointer = backend->m_PointerConstraints.lock_pointer(m_WindowSurface, backend->m_waylandPointer, nullptr, wayland::zwp_pointer_constraints_v1_lifetime::persistent);
	backend->SetMouseLockOwnerWindow(this);
	ShowCursor(false);
}

void WaylandDisplayWindow::UnlockCursor()
{
	if (m_LockedPointer)
		m_LockedPointer.proxy_release();
	backend->SetMouseLockOwnerWindow(nullptr);
	ShowCursor(true);
}

void WaylandDisplayWindow::CaptureMouse()
{
	m_ConfinedPointer = backend->m_PointerConstraints.confine_pointer(GetWindowSurface(), backend->m_waylandPointer, nullptr, wayland::zwp_pointer_constraints_v1_lifetime::persistent);
	backend->SetMouseLockOwnerWindow(this);
	// ShowCursor(false);
}

void WaylandDisplayWindow::ReleaseMouseCapture()
{
	if (m_ConfinedPointer)
		m_ConfinedPointer.proxy_release();
	backend->SetMouseLockOwnerWindow(nullptr);
	// ShowCursor(true);
}

void WaylandDisplayWindow::Update()
{
	m_NeedsUpdate = true;
}

bool WaylandDisplayWindow::GetKeyState(InputKey key)
{
	return backend->GetKeyState(key);
}

void WaylandDisplayWindow::SetCursor(StandardCursor cursor, std::shared_ptr<CustomCursor> custom)
{
	backend->SetCursor(cursor, custom);
}

Rect WaylandDisplayWindow::GetClientFrame() const
{
	Point globalPos = m_OwnerOffset;

	if (m_XDGPopup && m_owner)
	{
		for (auto cur = m_owner ; cur != nullptr ; cur = cur->m_owner)
		{
			globalPos += cur->m_OwnerOffset;
		}
	}

	// In the window's logical units, as GetClientSize answers and the
	// callers take them (m_WindowSize is pixels)
	return Rect(globalPos, m_ClientSize);
}

Size WaylandDisplayWindow::GetClientSize() const
{
	return m_ClientSize;
}

int WaylandDisplayWindow::GetPixelWidth() const
{
	return m_WindowSize.width;
}

int WaylandDisplayWindow::GetPixelHeight() const
{
	return m_WindowSize.height;
}

double WaylandDisplayWindow::GetDpiScale() const
{
	return m_ScaleFactor;
}

// The frame written into a buffer the compositor holds not, never into one
// it may still read, and shown at the next frame the compositor asks for, or
// at once where none is asked (the first, or the first after a pause)
void WaylandDisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (width <= 0 || height <= 0)
		return;
	std::shared_ptr<ShmBuffer> target = AcquireShmBuffer(width, height);
	if (!target)
		return; // each held: the next frame is shown in its place
	std::memcpy(target->mem->get_mem(), (const void*)pixels, (size_t)width * height * 4);
	target->attached = false;
	m_ShmFront = target;
	if (!m_FramePending)
		DrawSurface();
}

// A buffer of the size the compositor holds not: one it let go again, or a
// new one, three at most, those of another size dropped (a dropped one still
// held is never written again, which leaves the compositor's copy whole)
std::shared_ptr<WaylandDisplayWindow::ShmBuffer> WaylandDisplayWindow::AcquireShmBuffer(int width, int height)
{
	m_ShmBuffers.erase(std::remove_if(m_ShmBuffers.begin(), m_ShmBuffers.end(), [=](const std::shared_ptr<ShmBuffer>& b) { return b->width != width || b->height != height; }), m_ShmBuffers.end());
	if (m_ShmFront && (m_ShmFront->width != width || m_ShmFront->height != height))
		m_ShmFront.reset();
	for (const std::shared_ptr<ShmBuffer>& b : m_ShmBuffers)
	{
		if (!b->busy)
			return b;
	}
	if (m_ShmBuffers.size() >= 3)
		return nullptr;

	auto b = std::make_shared<ShmBuffer>();
	size_t size = (size_t)width * height * 4;
	b->mem = std::make_shared<SharedMemHelper>(size);
	auto pool = backend->m_waylandSHM.create_pool(b->mem->get_fd(), (int32_t)size);
	b->buffer = pool.create_buffer(0, width, height, width * 4, wayland::shm_format::xrgb8888);
	b->width = width;
	b->height = height;
	ShmBuffer* released = b.get();
	b->buffer.on_release() = [released]() { released->busy = false; };
	m_ShmBuffers.push_back(b);
	return b;
}

void WaylandDisplayWindow::SetBorderColor(uint32_t rgba8)
{

}

void WaylandDisplayWindow::SetCaptionColor(uint32_t rgba8)
{

}

void WaylandDisplayWindow::SetCaptionTextColor(uint32_t rgba8)
{

}

// A paste of our text: written whole as the reader takes it, for at most a
// second in all, so a reader that stops reading cannot hold the game, and
// SIGPIPE held back while it writes, so a reader gone before the end (wl-paste
// into head) ends the write and not the process, as SDL's own write holds it
// back
static void WriteSelection(int fd, const std::string& text)
{
	sigset_t pipeSignal, previous;
	sigemptyset(&pipeSignal);
	sigaddset(&pipeSignal, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &pipeSignal, &previous);

	int flags = fcntl(fd, F_GETFL);
	if (flags != -1)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	using namespace std::chrono;
	auto deadline = steady_clock::now() + seconds(1);
	size_t written = 0;
	while (written < text.size())
	{
		ssize_t n = write(fd, text.data() + written, text.size() - written);
		if (n > 0)
		{
			written += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			int wait = (int)duration_cast<milliseconds>(deadline - steady_clock::now()).count();
			pollfd pfd = { fd, POLLOUT, 0 };
			if (wait > 0 && poll(&pfd, 1, wait) > 0 && (pfd.revents & POLLOUT))
				continue;
		}
		break;
	}
	close(fd);

	// The SIGPIPE a gone reader raised, taken here, not delivered
	if (!sigismember(&previous, SIGPIPE))
	{
		timespec now = {};
		while (sigtimedwait(&pipeSignal, nullptr, &now) > 0)
		{
		}
	}
	pthread_sigmask(SIG_SETMASK, &previous, nullptr);
}

std::string WaylandDisplayWindow::GetClipboardText()
{
	return backend->GetClipboardText();
}

void WaylandDisplayWindow::SetClipboardText(const std::string& text)
{
	m_DataSource = backend->m_DataDeviceManager.create_data_source();

	m_DataSource.on_send() = [text] (const std::string& mime_type, const int fd)
	{
		WriteSelection(fd, text);
	};

	m_DataSource.offer("text/plain");
	// Ours until the compositor cancels the source (another client's
	// selection): a paste then answers from the text itself
	m_DataSource.on_cancelled() = [this] () { backend->m_OwnsSelection = false; };
	backend->m_OwnClipboard = text;
	backend->m_OwnsSelection = true;
	backend->GetDataDevice().set_selection(m_DataSource, backend->GetKeyboardSerial());
}

Point WaylandDisplayWindow::MapFromGlobal(const Point& pos) const
{
	return (pos * m_ScaleFactor - GetClientFrame().topLeft()) / m_ScaleFactor;
}

Point WaylandDisplayWindow::MapToGlobal(const Point& pos) const
{
	return (GetClientFrame().topLeft() + pos * m_ScaleFactor) / m_ScaleFactor;
}

void WaylandDisplayWindow::OnXDGToplevelConfigureEvent(int32_t width, int32_t height, const std::vector<wayland::xdg_toplevel_state>& states)
{
	// The states are the window's whole: one left out is off
	isFullscreen = false;
	isMaximized = false;
	for (const auto state : states)
	{
		if (state == wayland::xdg_toplevel_state::activated)
		{
			if (backend->GetMouseLockOwnerWindow() == this)
				ShowCursor(false);

			windowHost->OnWindowActivated();
		}
		// waylandpp's XDG_Toplevel seems to only have states up until tiled_top for the time being.
		// yet later versions of XDGWMBase has more states (like suspended)
		// so to not blow things up when waylandpp adds the missing states, we individually add the relevant states here
		else if (state == wayland::xdg_toplevel_state::fullscreen)
		{
			isFullscreen = true;
		}
		else if (state == wayland::xdg_toplevel_state::maximized)
		{
			isMaximized = true;
		}
		// else if (state == wayland::xdg_toplevel_state::resizing ||
		// 		 state == wayland::xdg_toplevel_state::tiled_bottom ||
		// 		 state == wayland::xdg_toplevel_state::tiled_left ||
		// 		 state == wayland::xdg_toplevel_state::tiled_right ||
		// 		 state == wayland::xdg_toplevel_state::tiled_top)
		// {
		// 	// Rect rect = GetClientFrame();
		// 	// rect.width = width;
		// 	// rect.height = height;
		// 	// SetClientFrame(rect);
		// }
	}

	Rect rect = GetClientFrame();
	rect.width = width;
	rect.height = height;
	SetClientFrame(rect);

	// Maximized where the last configure had it not is Maximize asked of the
	// window -- its title bar's button, a double click on it, its window
	// menu's -- which the host takes or leaves (DisplayWindowHost::
	// OnWindowMaximize), the game's window toggling full screen, unset as it
	// goes (ShowFullscreen). None is read in full screen, nor between an
	// unset asked and its configure
	bool maximized = isMaximized && !isFullscreen;
	bool asked = maximized && !configuredMaximized && !unmaximizing;
	configuredMaximized = maximized;
	if (!isMaximized)
		unmaximizing = false;
	if (asked)
		windowHost->OnWindowMaximize();
}

void WaylandDisplayWindow::OnExportHandleEvent(std::string exportedHandle)
{
	m_windowID = exportedHandle;
}

void WaylandDisplayWindow::OnExitEvent()
{
	windowHost->OnWindowClose();
}

// A bitmap window's newest frame attached, held from its attach until the
// compositor lets it go, and the next frame asked for; with no new frame
// nothing is asked, till PresentBitmap brings one. A device window's frames
// are its device's own
void WaylandDisplayWindow::DrawSurface(uint32_t serial)
{
	m_FramePending = false;
	if (!IsBitmapWindow() || !m_ShmFront || m_ShmFront->attached)
		return;

	m_WindowSurface.attach(m_ShmFront->buffer, 0, 0);
	m_WindowSurface.damage_buffer(0, 0, m_ShmFront->width, m_ShmFront->height);
	m_ShmFront->busy = true;
	m_ShmFront->attached = true;

	m_FrameCallback = m_WindowSurface.frame();
	m_FrameCallback.on_done() = bind_mem_fn(&WaylandDisplayWindow::DrawSurface, this);
	m_FramePending = true;
	m_WindowSurface.commit();
}

// The window's size in surface units, its pixels at the scale: a bitmap
// window's buffers are made at them as it draws (PresentBitmap), a device
// window's EGL window resized to them, and the buffer shown at the size by
// the viewport, or by its whole scale where the compositor has no viewporter
void WaylandDisplayWindow::CreateBuffers(int32_t width, int32_t height)
{
	if (width == 0 || height == 0)
		return;

	const int scaled_width = (int)std::round(width * m_ScaleFactor);
	const int scaled_height = (int)std::round(height * m_ScaleFactor);

	m_ClientSize = Size(width, height);
	m_WindowSize = Size(scaled_width, scaled_height);

	if (m_Viewport)
		m_Viewport.set_destination(width, height);
	else
		m_WindowSurface.set_buffer_scale(std::max((int32_t)std::lround(m_ScaleFactor), 1));

	if (m_EGLWindow)
		m_EGLWindow.resize(scaled_width, scaled_height);
}

void WaylandDisplayWindow::CreateAppIconBuffers(const std::vector<std::shared_ptr<Image>>& images)
{
	if (images.empty())
		return;

	appIconBuffers.clear();
	appIconSharedMems.clear();

	for (auto& image: images)
	{
		const int image_size = image->GetWidth() * image->GetHeight() * 4;
		const int stride = image->GetWidth() * 4;

		auto new_shared_mem = std::make_shared<SharedMemHelper>(image_size);

		auto pool = backend->m_waylandSHM.create_pool(new_shared_mem->get_fd(), image_size);

		auto new_buffer = pool.create_buffer(0, image->GetWidth(), image->GetHeight(), stride, wayland::shm_format::argb8888);

		// Fill the buffer with the icon data
		std::memcpy(new_shared_mem->get_mem(), image->GetData(), image_size);

		appIconSharedMems.push_back(new_shared_mem);
		appIconBuffers.push_back(new_buffer);
	}
}

std::string WaylandDisplayWindow::GetWaylandWindowID()
{
	return m_windowID;
}

void WaylandDisplayWindow::CreateGLContext()
{
	EGLint majorVer, minorVer;

	// OpenGL ES 3.2 (the GL device's ES shaders are GLSL ES 3.20) or desktop
	// 4.2 core (its desktop shaders are GLSL 4.20): ES takes the ES3 bit and
	// no profile mask.
	const bool es = m_renderAPI == RenderAPI::GLES;
	EGLint ctxAttrs[7];
	int ctxAttrCount = 0;
	ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_MAJOR_VERSION;
	ctxAttrs[ctxAttrCount++] = es ? 3 : 4;
	ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_MINOR_VERSION;
	ctxAttrs[ctxAttrCount++] = 2;
	if (!es)
	{
		ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
		ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
	}
	ctxAttrs[ctxAttrCount++] = EGL_NONE;

	EGLint renderableBit = es ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;

	EGLint confAttrs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_CONFORMANT,	renderableBit,
		EGL_RENDERABLE_TYPE, renderableBit,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,

		EGL_NONE
	};

	m_EGLDisplay = eglGetDisplay(backend->s_waylandDisplay);

	if (m_EGLDisplay == EGL_NO_DISPLAY)
		throw std::runtime_error("No EGL Display found");

	if (!eglInitialize(m_EGLDisplay, &majorVer, &minorVer))
		throw std::runtime_error("Error initializing EGL");

	if (majorVer < 1 || (majorVer == 1 && minorVer < 5))
		throw std::runtime_error("EGL version 1.5 or higher required");

	if (!eglBindAPI(es ? EGL_OPENGL_ES_API : EGL_OPENGL_API))
		throw std::runtime_error("EGL cannot bind to the GL API");

	EGLConfig config;
	EGLint config_count;

	if (!eglChooseConfig(m_EGLDisplay, confAttrs, &config, 1, &config_count) || config_count != 1)
		throw std::runtime_error("Cannot choose EGL config. Error code: " + std::to_string(eglGetError()));

	m_EGLContext = eglCreateContext(m_EGLDisplay, config, EGL_NO_CONTEXT, ctxAttrs);
	if (m_EGLContext == EGL_NO_CONTEXT)
		throw std::runtime_error("Failed to create EGL context. Error code: " + std::to_string(eglGetError()));

	m_EGLSurface = eglCreateWindowSurface(m_EGLDisplay, config, m_EGLWindow, nullptr);
	if (m_EGLSurface == EGL_NO_SURFACE)
		throw std::runtime_error("Failed to create EGL surface. Error code: " + std::to_string(eglGetError()));

	// The swap's frames asked of the compositor on a queue of their own, the
	// surface wrapped onto it, so the wait for one dispatches nothing else
	m_SwapQueue = backend->s_waylandDisplay.create_queue();
	m_SwapSurface = m_WindowSurface.proxy_create_wrapper();
	m_SwapSurface.set_queue(m_SwapQueue);
}

void WaylandDisplayWindow::MakeGLContextCurrent()
{
	if (!eglMakeCurrent(m_EGLDisplay, m_EGLSurface, m_EGLSurface, m_EGLContext))
		throw std::runtime_error("Error in eglMakeCurrent(). Error code: " + std::to_string(eglGetError()));

	// EGL's own wait off from the first swap (SetGLSwapInterval)
	if (m_SwapSurface && m_EGLSwapInterval != 0 && eglSwapInterval(m_EGLDisplay, 0))
		m_EGLSwapInterval = 0;
}

// EGL has no late swap: a negative interval is clamped, so it is refused
// here, and the GL device falls back to 1 (as the X11 window's). The wait an
// interval asks is the window's own (SwapGLBuffers), EGL's swap set to wait
// for nothing: Mesa's waits for the compositor's frame without end, and a
// window the compositor hides is sent none, the game held till it shows
bool WaylandDisplayWindow::SetGLSwapInterval(int interval)
{
	if (interval < 0)
		return false;
	int eglInterval = m_SwapSurface ? 0 : interval;
	if (eglInterval != m_EGLSwapInterval)
	{
		if (!eglSwapInterval(m_EGLDisplay, eglInterval))
			return false;
		m_EGLSwapInterval = eglInterval;
	}
	m_SwapInterval = interval;
	return true;
}

// A swap with an interval waits for the frame the compositor last asked for,
// as EGL's would, but at most 50 ms (20 frames a second where none comes),
// then asks for the next with the swap's commit
void WaylandDisplayWindow::SwapGLBuffers()
{
	if (m_SwapSurface && m_SwapInterval > 0)
	{
		WaitSwapFrame();
		m_SwapFrameDone = false;
		m_SwapFrame = m_SwapSurface.frame();
		m_SwapFrame.on_done() = [this](uint32_t) { m_SwapFrameDone = true; };
	}
	eglSwapBuffers(m_EGLDisplay, m_EGLSurface);
}

void WaylandDisplayWindow::WaitSwapFrame()
{
	using namespace std::chrono;
	auto& display = backend->s_waylandDisplay;
	auto deadline = steady_clock::now() + milliseconds(50);
	while (!m_SwapFrameDone)
	{
		// Takes what the queue holds first, the frame among it maybe
		wayland::read_intent intent = display.obtain_queue_read_intent(m_SwapQueue);
		if (m_SwapFrameDone)
			break;
		display.flush();
		int wait = (int)duration_cast<milliseconds>(deadline - steady_clock::now()).count();
		if (wait <= 0)
			break;
		pollfd pfd = { display.get_fd(), POLLIN, 0 };
		if (poll(&pfd, 1, wait) <= 0 || !(pfd.revents & POLLIN))
			break;
		intent.read();
		display.dispatch_queue_pending(m_SwapQueue);
	}
}

DisplayWindow::GLFuncPtr WaylandDisplayWindow::GetGLProcAddress(const char* name)
{
	return (GLFuncPtr)eglGetProcAddress(name);
}

// This is to avoid needing all the Vulkan headers and the volk binding library just for this:
#ifndef VK_VERSION_1_0

#define VKAPI_CALL
#define VKAPI_PTR VKAPI_CALL

typedef uint32_t VkFlags;
typedef enum VkStructureType { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR = 1000006000, VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF } VkStructureType;
typedef enum VkResult { VK_SUCCESS = 0, VK_RESULT_MAX_ENUM = 0x7FFFFFFF } VkResult;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);

#ifndef VK_KHR_wayland_surface

typedef VkFlags VkWaylandSurfaceCreateFlagsKHR;
typedef struct VkWaylandSurfaceCreateInfoKHR
{
	VkStructureType				   sType;
	const void*					   pNext;
	VkWaylandSurfaceCreateFlagsKHR	flags;
	struct wl_display*				display;
	struct wl_surface*				surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateWaylandSurfaceKHR)(VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#endif
#endif

class ZWidgetWaylandVulkanLoader
{
public:
	ZWidgetWaylandVulkanLoader()
	{
		module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);

		if (!module)
			throw std::runtime_error("Could not load vulkan");

		vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
		if (!vkGetInstanceProcAddr)
		{
			dlclose(module);
			throw std::runtime_error("vkGetInstanceProcAddr not found");
		}
	}

	~ZWidgetWaylandVulkanLoader()
	{
		dlclose(module);
	}

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
	void* module = nullptr;
};

VkSurfaceKHR WaylandDisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	static ZWidgetWaylandVulkanLoader loader;

	auto vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)loader.vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
	if (!vkCreateWaylandSurfaceKHR)
		throw std::runtime_error("Could not create vulkan surface");

	VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
	createInfo.display = m_NativeHandle.display;
	createInfo.surface = m_NativeHandle.surface;

	VkSurfaceKHR surface = {};
	VkResult result = vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Could not create vulkan surface");
	return surface;
}

std::vector<std::string> WaylandDisplayWindow::GetVulkanInstanceExtensions()
{
	return { "VK_KHR_surface", "VK_KHR_wayland_surface" };
}
