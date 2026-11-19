
#include "x11_display_window.h"
#include "x11_connection.h"
#include "window/evdev_keys.h"
#include <dxwidgets/core/image.h>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <cstring>
#include <chrono>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <iostream>

X11DisplayWindow::X11DisplayWindow(DisplayWindowHost* windowHost, WidgetType windowType, X11DisplayWindow* owner, RenderAPI renderAPI) : windowHost(windowHost), owner(owner), m_GLIsES(renderAPI == RenderAPI::GLES)
{
	auto connection = GetX11Connection();
	display = connection->display;

	screen = XDefaultScreen(display);
	depth = XDefaultDepth(display, screen);
	visual = XDefaultVisual(display, screen);
	colormap = XDefaultColormap(display, screen);

	dpiScale = connection->dpiScale;

	XSetWindowAttributes attributes = {};
	attributes.backing_store = Always;
	attributes.override_redirect = (windowType == WidgetType::Popup) ? True : False;
	attributes.save_under = (windowType == WidgetType::Popup) ? True : False;
	attributes.colormap = colormap;
	attributes.event_mask =
		KeyPressMask | KeyReleaseMask | 
		EnterWindowMask | LeaveWindowMask | KeymapStateMask |
		ExposureMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;

	if (!connection->XInput2Supported)
	{
		attributes.event_mask |= ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	}
		
	unsigned long mask = CWBackingStore | CWSaveUnder | CWEventMask | CWOverrideRedirect;

	window = XCreateWindow(display, XRootWindow(display, screen), 0, 0, 100, 100, 0, depth, InputOutput, visual, mask, &attributes);
	connection->windows[window] = this;

	if (connection->XInput2Supported)
	{
		unsigned char mask[1] = { 0 };
		XISetMask(mask, XI_ButtonPress);
		XISetMask(mask, XI_ButtonRelease);
		XISetMask(mask, XI_Motion);
		XIEventMask eventmask;
		eventmask.deviceid = connection->MasterPointerID;
		eventmask.mask_len = sizeof(mask);
		eventmask.mask = mask;
		XISelectEvents(display, window, &eventmask, 1);
	}

	if (owner)
	{
		XSetTransientForHint(display, window, owner->window);
	}

	// Tell window manager which process this window came from
	if (connection->GetAtom("_NET_WM_PID") != None)
	{
		int32_t pid = getpid();
		if (pid != 0)
		{
			XChangeProperty(display, window, connection->GetAtom("_NET_WM_PID"), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
		}
	}

	// Tell window manager which machine this window came from
	if (connection->GetAtom("WM_CLIENT_MACHINE") != None)
	{
		std::vector<char> hostname(256);
		if (gethostname(hostname.data(), hostname.size()) >= 0)
		{
			hostname.push_back(0);
			XChangeProperty(display, window, connection->GetAtom("WM_CLIENT_MACHINE"), XA_STRING, 8, PropModeReplace, (unsigned char *)hostname.data(), strlen(hostname.data()));
		}
	}

	// Tell window manager we want to listen to close events
	if (connection->GetAtom("WM_DELETE_WINDOW") != None)
	{
		Atom protocol = connection->GetAtom("WM_DELETE_WINDOW");
		XSetWMProtocols(display, window, &protocol, 1);
	}

	// Tell window manager what type of window we are
	if (connection->GetAtom("_NET_WM_WINDOW_TYPE") != None)
	{
		Atom type = None;
		if (windowType == WidgetType::Popup)
		{
			type = connection->GetAtom("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
			if (type == None)
				type =  connection->GetAtom("_NET_WM_WINDOW_TYPE_POPUP_MENU");
			if (type == None)
				type = connection->GetAtom("_NET_WM_WINDOW_TYPE_COMBO");
		}
		if (type == None)
			type = connection->GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");

		if (type != None)
		{
			XChangeProperty(display, window, connection->GetAtom("_NET_WM_WINDOW_TYPE"), XA_ATOM, 32, PropModeReplace, (unsigned char *)&type, 1);
		}
	}

	// Create input context
	if (connection->xim)
	{
		xic = XCreateIC(
			connection->xim,
			XNInputStyle,
			XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, window,
			XNFocusWindow, window,
			nullptr);
	}
}

X11DisplayWindow::~X11DisplayWindow()
{
	if (hidden_cursor != None)
	{
		XFreeCursor(display, hidden_cursor);
		XFreePixmap(display, cursor_bitmap);
	}

	if (m_EGLContext)
	{
		// The context let go of before it is destroyed: no context takes no
		// surfaces (EGL 3.7.3: EGL_NO_CONTEXT with a surface is EGL_BAD_MATCH,
		// which left the context and surface current, only marked for
		// deletion, under a display terminated and a window destroyed)
		eglMakeCurrent(m_EGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(m_EGLDisplay, m_EGLContext);
		eglDestroySurface(m_EGLDisplay, m_EGLSurface);
		eglTerminate(m_EGLDisplay);

		m_EGLContext = nullptr;
		m_EGLSurface = nullptr;
		m_EGLDisplay = nullptr;
	}

	if (xic)
	{
		XDestroyIC(xic);
		xic = nullptr;
	}

	DestroyBackbuffer();
	XDestroyWindow(display, window);
	GetX11Connection()->windows.erase(GetX11Connection()->windows.find(window));
}

void X11DisplayWindow::SetWindowTitle(const std::string& text)
{
	XSetStandardProperties(display, window, text.c_str(), text.c_str(), None, nullptr, 0, nullptr);
}

void X11DisplayWindow::SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images)
{
	if (images.empty())
		return;

	double dpiscale = GetDpiScale();
	int desiredSize = (int)std::round(32 * dpiscale);

	Image* image = images.front().get();
	for (const auto& i : images)
	{
		int curdist = std::abs(image->GetWidth() - desiredSize);
		int dist = std::abs(i->GetWidth() - desiredSize);
		if (dist < curdist)
			image = i.get();
	}

	int width = image->GetWidth();
	int height = image->GetHeight();
	const uint32_t* s = (const uint32_t*)image->GetData();

	unsigned int size = (width * height) + 2; // header is 2 ints
	unsigned long* data = (unsigned long*)malloc(size * sizeof(unsigned long));

	// set header
	data[0] = width;
	data[1] = height;

	// on 64bit systems, the destination buffer is 64 bit per pixel
	// thus, we have to copy each pixel individually (no memcpy)

	// icon data is expected as ARGB

	if (image->GetFormat() == ImageFormat::R8G8B8A8)
	{
		for (int y = 0; y < height; ++y)
		{
			const uint32_t* src = s + y * width;
			unsigned long* dst = &data[2 + (y * width)];
			for (int x = 0; x < width; ++x)
			{
				uint32_t r = src[x] & 0xff;
				uint32_t g = (src[x] >> 8) & 0xff;
				uint32_t b = (src[x] >> 16) & 0xff;
				uint32_t a = (src[x] >> 24) & 0xff;
				dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
			}
		}
	}
	else if (image->GetFormat() == ImageFormat::B8G8R8A8)
	{
		for (int y = 0; y < height; ++y)
		{
			const uint32_t* src = s + y * width;
			unsigned long* dst = &data[2 + (y * width)];
			for (int x = 0; x < width; ++x)
			{
				dst[x] = src[x];
			}
		}
	}
	else
	{
		free(data);
		return;
	}

	// set icon geometry
	unsigned long* geom = (unsigned long*)malloc(4 * sizeof(unsigned long));
	geom[0] = geom[1] = 0; // x, y
	geom[2] = width;
	geom[3] = height;

	Atom propertyGeom = XInternAtom(display, "_NET_WM_ICON_GEOMETRY", 0);
	XChangeProperty(display, window, propertyGeom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)geom, 4);

	// set icon data
	Atom property = XInternAtom(display, "_NET_WM_ICON", 0);
	XChangeProperty(display, window, property, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)data, size);

	// XChangeProperty copies both
	free(geom);
	free(data);
}

void X11DisplayWindow::SetClientFrame(const Rect& box)
{
	double dpiscale = GetDpiScale();
	int x = (int)std::round(box.x * dpiscale);
	int y = (int)std::round(box.y * dpiscale);
	int width = (int)std::round(box.width * dpiscale);
	int height = (int)std::round(box.height * dpiscale);

	// A size of nothing is not asked for: the server refuses it (BadValue),
	// and Xlib's default handler ends the process. The engine's full screen
	// at the desktop's size passes 0 by 0 for the frame.
	XWindowChanges changes = {};
	changes.x = x;
	changes.y = y;
	unsigned int mask = CWX | CWY;
	if (width > 0 && height > 0)
	{
		changes.width = width;
		changes.height = height;
		mask |= CWWidth | CWHeight;
	}

	XConfigureWindow(display, window, mask, &changes);

	// A place asked for is the program's (ICCCM's USPosition and
	// PPosition): a window manager keeps it for a window it maps, where it
	// would place one itself (the property windows at the screen's corner,
	// as Windows keeps the original's)
	XSizeHints* size_hints = XAllocSizeHints();
	long user_hints;
	XGetWMNormalHints(display, window, size_hints, &user_hints);
	size_hints->flags |= USPosition | PPosition;
	size_hints->x = x;
	size_hints->y = y;
	XSetWMNormalHints(display, window, size_hints);
	XFree(size_hints);
}

void X11DisplayWindow::SetClientSize(const Size& size)
{
	double dpiscale = GetDpiScale();
	int width = (int)std::round(size.width * dpiscale);
	int height = (int)std::round(size.height * dpiscale);
	if (width <= 0 || height <= 0)
		return;   // none asked for, as SetClientFrame's
	XWindowChanges changes = {};
	changes.width = width;
	changes.height = height;
	XConfigureWindow(display, window, CWWidth | CWHeight, &changes);
}

void X11DisplayWindow::Show()
{
	if (!isMapped)
	{
		XMapRaised(display, window);
		isMapped = true;
	}
}

// A mapped window's state changes only by a _NET_WM_STATE client message to
// the root (EWMH); the property set before mapping is taken as the window
// maps
void X11DisplayWindow::SetFullscreenState(bool fullscreen)
{
	auto connection = GetX11Connection();
	Atom stateAtom = connection->GetAtom("_NET_WM_STATE");
	Atom fullscreenAtom = connection->GetAtom("_NET_WM_STATE_FULLSCREEN");
	if (stateAtom == None || fullscreenAtom == None)
		return;
	if (isMapped)
	{
		XEvent message = {};
		message.xclient.type = ClientMessage;
		message.xclient.window = window;
		message.xclient.message_type = stateAtom;
		message.xclient.format = 32;
		message.xclient.data.l[0] = fullscreen ? 1 : 0; // _NET_WM_STATE_ADD, _NET_WM_STATE_REMOVE
		message.xclient.data.l[1] = fullscreenAtom;
		message.xclient.data.l[2] = 0;
		message.xclient.data.l[3] = 1; // a normal application's request
		XSendEvent(display, RootWindow(display, screen), False, SubstructureRedirectMask | SubstructureNotifyMask, &message);
	}
	else if (fullscreen)
	{
		XChangeProperty(display, window, stateAtom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&fullscreenAtom, 1);
	}
	else
	{
		XDeleteProperty(display, window, stateAtom);
	}
	isFullscreen = fullscreen;
}

// Out of the window manager's maximize, asked as a mapped window's state is;
// the state read until the answer is the old one (OnPropertyNotify)
void X11DisplayWindow::Unmaximize()
{
	auto connection = GetX11Connection();
	Atom stateAtom = connection->GetAtom("_NET_WM_STATE");
	Atom vertAtom = connection->GetAtom("_NET_WM_STATE_MAXIMIZED_VERT");
	Atom horzAtom = connection->GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ");
	if (!isMapped || stateAtom == None || vertAtom == None || horzAtom == None)
		return;
	XEvent message = {};
	message.xclient.type = ClientMessage;
	message.xclient.window = window;
	message.xclient.message_type = stateAtom;
	message.xclient.format = 32;
	message.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
	message.xclient.data.l[1] = vertAtom;
	message.xclient.data.l[2] = horzAtom;
	message.xclient.data.l[3] = 1; // a normal application's request
	XSendEvent(display, RootWindow(display, screen), False, SubstructureRedirectMask | SubstructureNotifyMask, &message);
	unmaximizing = true;
}

// A maximized window is un-maximized first: one maximized under full screen
// may come back from it maximized, which the game's window would take as
// Maximize again (OnPropertyNotify)
void X11DisplayWindow::ShowFullscreen()
{
	if (isMaximized)
		Unmaximize();
	SetFullscreenState(true);
	Show();
}

bool X11DisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void X11DisplayWindow::ShowMaximized()
{
	Show();
}

void X11DisplayWindow::ShowMinimized()
{
	if (!isMinimized)
	{
		Show(); // To do: can this be avoided? WMHints has an initial state that can make it show minimized
		XIconifyWindow(display, window, screen);
		isMinimized = true;
	}
}

// Out of full screen and not maximized: a window manager that marks a full
// screen window maximized may leave it so (SDL asks the same as it leaves)
void X11DisplayWindow::ShowNormal()
{
	bool wasFullscreen = isFullscreen;
	if (isFullscreen)
		SetFullscreenState(false);
	if (wasFullscreen || isMaximized)
		Unmaximize();
	Show();
}

void X11DisplayWindow::SetWindowResizable(bool enable)
{
	XSizeHints* size_hints = XAllocSizeHints();
	long user_hints;
	XGetWMNormalHints(display, window, size_hints, &user_hints);
	if (enable)
	{
		size_hints->flags &= ~(PMinSize | PMaxSize);
		size_hints->min_width = minWindowWidth;
		size_hints->max_width = maxWindowWidth;
		size_hints->min_height = minWindowHeight;
		size_hints->max_height = maxWindowHeight;
	}
	else
	{
		size_hints->flags |= PMinSize | PMaxSize;
		size_hints->min_width = GetPixelWidth();
		size_hints->max_width = GetPixelWidth();
		size_hints->min_height = GetPixelHeight();
		size_hints->max_height = GetPixelHeight();
	}
	XSetWMNormalHints(display, window, size_hints);
	XFree(size_hints);
}

void X11DisplayWindow::SetWindowMinSize(int width, int height)
{
	XSizeHints* size_hints = XAllocSizeHints();
	long user_hints;
	XGetWMNormalHints(display, window, size_hints, &user_hints);
	size_hints->flags |= PMinSize;
	size_hints->min_width = width;
	size_hints->min_height = height;

	XSetWMNormalHints(display, window, size_hints);
	XFree(size_hints);

	minWindowWidth = width;
	minWindowHeight = height;
}

void X11DisplayWindow::SetWindowMaxSize(int width, int height)
{
	XSizeHints* size_hints = XAllocSizeHints();
	long user_hints;
	XGetWMNormalHints(display, window, size_hints, &user_hints);
	size_hints->flags |= PMaxSize;
	size_hints->max_width = width;
	size_hints->max_height = height;

	XSetWMNormalHints(display, window, size_hints);
	XFree(size_hints);

	maxWindowWidth = width;
	maxWindowHeight = height;
}

void X11DisplayWindow::Hide()
{
	if (isMapped)
	{
		XUnmapWindow(display, window);
		isMapped = false;
	}
}

void X11DisplayWindow::Activate()
{
	XRaiseWindow(display, window);
}

void X11DisplayWindow::ShowCursor(bool enable)
{
	if (isCursorEnabled != enable)
	{
		isCursorEnabled = enable;
		UpdateCursor();
	}
}

// Typed text off in play, as the engine asks (SDL3's SDL_StopTextInput): the
// keys still come as keys, the input method's focus taken from the window
// so that it filters none (a key that is a dead key on the layout, the one
// below Escape on a German one, reaches the game), and given back as the
// typing comes back while the window has the focus
void X11DisplayWindow::LockKeyboard()
{
	if (!keyboardLocked && xic)
		XUnsetICFocus(xic);
	keyboardLocked = true;
}

void X11DisplayWindow::UnlockKeyboard()
{
	if (keyboardLocked && xic && RawInput.Focused)
		XSetICFocus(xic);
	keyboardLocked = false;
}

void X11DisplayWindow::LockCursor()
{
	ShowCursor(false);
	GrabCursor(RawInput.Focused);
}

void X11DisplayWindow::UnlockCursor()
{
	ShowCursor(true);
	GrabCursor(false);
}

// The pointer held in the window while the game locks it and the window has
// the focus, as the original clips it to its client area and lets it go with
// the focus (dx-reverse-info/windrv-dll.md, the keys): the pointer grabbed,
// confined to the window, where the server sends raw moves through a grab
// (XInput 2.1); the warp to the middle (OnXInputEvent) follows its moves. A
// grab the server refuses (another program's held) is asked again at the
// next lock, the engine locking each frame.
void X11DisplayWindow::GrabCursor(bool grab)
{
	if (grab && !cursorGrabbed && GetX11Connection()->RawEventsInGrabs)
	{
		cursorGrabbed = XGrabPointer(display, window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync, GrabModeAsync, window, None, CurrentTime) == GrabSuccess;
	}
	else if (!grab && cursorGrabbed)
	{
		XUngrabPointer(display, CurrentTime);
		cursorGrabbed = false;
	}
}

// A widget's drag (a scrollbar's thumb) follows the pointer past the window:
// the pointer grabbed, still shown. Hiding it is the game's raw-motion mode,
// which warps it to the centre and passes no move on
void X11DisplayWindow::CaptureMouse()
{
	XGrabPointer(display, window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

void X11DisplayWindow::ReleaseMouseCapture()
{
	XUngrabPointer(display, CurrentTime);
}

void X11DisplayWindow::Update()
{
	needsUpdate = true;
}

bool X11DisplayWindow::GetKeyState(InputKey key)
{
	auto it = keyState.find(key);
	return it != keyState.end() ? it->second : false;
}

void X11DisplayWindow::SetCursor(StandardCursor newcursor, std::shared_ptr<CustomCursor> newcustom)
{
	if (cursor != newcursor)
	{
		cursor = newcursor;
		UpdateCursor();
	}
}

void X11DisplayWindow::UpdateCursor()
{
	if (isCursorEnabled)
	{
		Cursor& x11cursor = GetX11Connection()->standardCursors[cursor];
		if (x11cursor == None)
		{
			unsigned int index = XC_left_ptr;
			switch (cursor)
			{
			default:
			case StandardCursor::arrow: index = XC_left_ptr; break;
			case StandardCursor::appstarting: index = XC_watch; break;
			case StandardCursor::cross: index = XC_cross; break;
			case StandardCursor::hand: index = XC_hand2; break;
			case StandardCursor::ibeam: index = XC_xterm; break;
			case StandardCursor::size_all: index = XC_fleur; break;
			case StandardCursor::size_ns: index = XC_double_arrow; break;
			case StandardCursor::size_we: index = XC_sb_h_double_arrow; break;
			case StandardCursor::uparrow: index = XC_sb_up_arrow; break;
			case StandardCursor::wait: index = XC_watch; break;
			case StandardCursor::no: index = XC_X_cursor; break;
			case StandardCursor::size_nesw: break; // To do: need to map this
			case StandardCursor::size_nwse: break;
			}
			x11cursor = XCreateFontCursor(display, index);
		}
		XDefineCursor(display, window, x11cursor);
	}
	else
	{
		if (hidden_cursor == None)
		{
			char data[64] = {};
			XColor black_color = {};
			cursor_bitmap = XCreateBitmapFromData(display, window, data, 8, 8);
			hidden_cursor = XCreatePixmapCursor(display, cursor_bitmap, cursor_bitmap, &black_color, &black_color, 0, 0);
		}
		XDefineCursor(display, window, hidden_cursor);
	}
}

Rect X11DisplayWindow::GetClientFrame() const
{
	double dpiscale = GetDpiScale();

	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	/*Status status =*/ XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);

	// The place from the root, as SetClientFrame sets it (ICCCM 4.1.5), not
	// from the parent, which under a reparenting manager is the frame
	Window child = None;
	XTranslateCoordinates(display, window, root, 0, 0, &x, &y, &child);

	return Rect::xywh(x / dpiscale, y / dpiscale, width / dpiscale, height / dpiscale);
}

Size X11DisplayWindow::GetClientSize() const
{
	double dpiscale = GetDpiScale();

	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	/*Status status =*/ XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);

	return Size(width / dpiscale, height / dpiscale);
}

int X11DisplayWindow::GetPixelWidth() const
{
	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	/*Status status =*/ XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);
	return width;
}

int X11DisplayWindow::GetPixelHeight() const
{
	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	/*Status status =*/ XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);
	return height;
}

double X11DisplayWindow::GetDpiScale() const
{
	return dpiScale;
}

namespace
{
	// XShmAttach's refusal, which comes as an error: a server on another
	// machine, or in another IPC namespace
	bool ShmAttachFailed = false;
	int OnShmAttachError(Display*, XErrorEvent*)
	{
		ShmAttachFailed = true;
		return 0;
	}
}

void X11DisplayWindow::CreateBackbuffer(int width, int height)
{
	// The software device's frame is 8-bit BGRA, which a visual of 8:8:8
	// takes as it is and one of 10:10:10 at a depth of 30 (a 10-bit desktop)
	// takes packed at the present; one of other masks (16-bit's) would show
	// its bytes as something else, so it is refused with what it is
	Visual* defaultVisual = DefaultVisual(display, screen);
	backbuffer.packed10 = depth == 30 && defaultVisual->red_mask == 0x3ff00000 && defaultVisual->green_mask == 0xffc00 && defaultVisual->blue_mask == 0x3ff;
	if (!backbuffer.packed10 && !((depth == 24 || depth == 32) && defaultVisual->red_mask == 0xff0000 && defaultVisual->green_mask == 0xff00 && defaultVisual->blue_mask == 0xff))
		throw std::runtime_error("The X11 display's visual is not 8-bit BGRA (depth " + std::to_string(depth) + ", masks " + std::to_string(defaultVisual->red_mask) + "/" + std::to_string(defaultVisual->green_mask) + "/" + std::to_string(defaultVisual->blue_mask) + "): the software device cannot draw to it");

	backbuffer.pixmap = XCreatePixmap(display, window, width, height, depth);
	backbuffer.width = width;
	backbuffer.height = height;

	// In memory shared with the server where it takes it (MIT-SHM): a frame
	// then reaches it without going through the socket
	if (XShmQueryExtension(display))
	{
		XImage* image = XShmCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, nullptr, &backbuffer.shm, width, height);
		if (image && image->bytes_per_line == width * (int)sizeof(uint32_t))
		{
			backbuffer.shm.shmid = shmget(IPC_PRIVATE, (size_t)image->bytes_per_line * height, IPC_CREAT | 0600);
			if (backbuffer.shm.shmid >= 0)
			{
				backbuffer.shm.shmaddr = (char*)shmat(backbuffer.shm.shmid, nullptr, 0);
				backbuffer.shm.readOnly = False;
				bool attached = false;
				if (backbuffer.shm.shmaddr != (char*)-1)
				{
					XSync(display, False);
					ShmAttachFailed = false;
					auto previous = XSetErrorHandler(OnShmAttachError);
					XShmAttach(display, &backbuffer.shm);
					XSync(display, False);
					XSetErrorHandler(previous);
					attached = !ShmAttachFailed;
					if (!attached)
						shmdt(backbuffer.shm.shmaddr);
				}
				// Gone once the server and this process have both let it go
				shmctl(backbuffer.shm.shmid, IPC_RMID, nullptr);
				if (attached)
				{
					image->data = backbuffer.shm.shmaddr;
					backbuffer.image = image;
					backbuffer.pixels = image->data;
					backbuffer.shared = true;
					return;
				}
			}
		}
		if (image)
			XDestroyImage(image); // no data: XShmCreateImage allocates none
		backbuffer.shm = {};
	}

	backbuffer.pixels = malloc(width * height * sizeof(uint32_t));
	backbuffer.image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char*)backbuffer.pixels, width, height, 32, 0);
}

void X11DisplayWindow::DestroyBackbuffer()
{
	if (backbuffer.width > 0 && backbuffer.height > 0)
	{
		if (backbuffer.shared)
		{
			// The server's last reading done, then the memory let go
			XShmDetach(display, &backbuffer.shm);
			XSync(display, False);
			backbuffer.image->data = nullptr;
			XDestroyImage(backbuffer.image);
			shmdt(backbuffer.shm.shmaddr);
			backbuffer.shm = {};
			backbuffer.shared = false;
			backbuffer.pending = false;
		}
		else
		{
			XDestroyImage(backbuffer.image);
		}
		XFreePixmap(display, backbuffer.pixmap);
		backbuffer.width = 0;
		backbuffer.height = 0;
		backbuffer.pixmap = None;
		backbuffer.image = nullptr;
		backbuffer.pixels = nullptr;
	}
}

void X11DisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (backbuffer.width != width || backbuffer.height != height)
	{
		DestroyBackbuffer();
		if (width > 0 && height > 0)
			CreateBackbuffer(width, height);
	}

	if (backbuffer.width == width && backbuffer.height == height)
	{
		// The server done with the last frame's shared pixels before they
		// are written over
		if (backbuffer.pending)
		{
			XSync(display, False);
			backbuffer.pending = false;
		}
		if (backbuffer.packed10)
		{
			// Each 8-bit channel to 10 bits, its top two bits repeated below
			// it, so that 255 is 1023
			uint32_t* dest = (uint32_t*)backbuffer.pixels;
			for (size_t i = 0, count = (size_t)width * height; i < count; i++)
			{
				uint32_t c = pixels[i];
				uint32_t r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
				dest[i] = (((r << 2) | (r >> 6)) << 20) | (((g << 2) | (g >> 6)) << 10) | ((b << 2) | (b >> 6));
			}
		}
		else
		{
			memcpy(backbuffer.pixels, pixels, width * height * sizeof(uint32_t));
		}
		GC gc = XDefaultGC(display, screen);
		if (backbuffer.shared)
		{
			XShmPutImage(display, backbuffer.pixmap, gc, backbuffer.image, 0, 0, 0, 0, width, height, False);
			backbuffer.pending = true;
		}
		else
		{
			XPutImage(display, backbuffer.pixmap, gc, backbuffer.image, 0, 0, 0, 0, width, height);
		}
		XCopyArea(display, backbuffer.pixmap, window, gc, 0, 0, width, height, 0, 0);
		XFlush(display);
	}
}

void X11DisplayWindow::SetBorderColor(uint32_t rgba8)
{
}

void X11DisplayWindow::SetCaptionColor(uint32_t rgba8)
{
}

void X11DisplayWindow::SetCaptionTextColor(uint32_t rgba8)
{
}

std::vector<uint8_t> X11DisplayWindow::GetWindowProperty(Atom property, Atom &actual_type, int &actual_format, unsigned long &item_count)
{
	// The property's size first, from a read of no length, then the whole of
	// it, the length in 32-bit units. A loop that asked for no data never
	// ended on a property that had any, and a paste hung the game.
	unsigned long _item_count = 0;
	unsigned long bytes_remaining = 0;
	unsigned char *read_data = nullptr;
	int result = XGetWindowProperty(
		display, window, property, 0L, 0L,
		False, AnyPropertyType, &actual_type, &actual_format,
		&_item_count, &bytes_remaining, &read_data);
	if (read_data)
	{
		XFree(read_data);
		read_data = nullptr;
	}
	if (result == Success && bytes_remaining > 0)
	{
		result = XGetWindowProperty(
			display, window, property, 0L, (long)((bytes_remaining + 3) / 4),
			False, AnyPropertyType, &actual_type, &actual_format,
			&_item_count, &bytes_remaining, &read_data);
	}
	if (result != Success)
	{
		actual_type = None;
		actual_format = 0;
		item_count = 0;
		return {};
	}

	item_count = _item_count;
	if (!read_data)
		return {};
	// Xlib gives 32-bit items as longs, 16-bit ones as shorts.
	size_t unit = actual_format == 32 ? sizeof(long) : actual_format == 16 ? sizeof(short) : 1;
	std::vector<uint8_t> buffer(read_data, read_data + _item_count * unit);
	XFree(read_data);
	return buffer;
}

// A PropertyNotify of a new value of the property an INCR transfer comes in
// (GetClipboardText)
static Bool IsIncrChunk(Display* display, XEvent* event, XPointer arg)
{
	const XPropertyEvent* wanted = (const XPropertyEvent*)arg;
	return event->type == PropertyNotify && event->xproperty.window == wanted->window &&
		event->xproperty.atom == wanted->atom && event->xproperty.state == PropertyNewValue;
}

// The clipboard's text as UTF-8: asked for as UTF8_STRING, then as STRING,
// Latin-1 (ICCCM), from an owner that refuses UTF-8. This program's own
// selection is its text as it is: its request to itself goes unanswered
// while this waits. The property read is deleted, as ICCCM asks; a text
// larger than the owner sends at once comes by INCR, in chunks, each
// deleted to ask for the next, the last one empty. Each answer is waited
// for 500 ms at most, whatever other events come meanwhile.
std::string X11DisplayWindow::GetClipboardText()
{
	auto connection = GetX11Connection();
	Atom clipboard = connection->GetAtom("CLIPBOARD");
	if (clipboard == None)
		return {};

	auto ours = connection->windows.find(XGetSelectionOwner(display, clipboard));
	if (ours != connection->windows.end())
		return ours->second->clipboardText;

	Atom utf8Atom = XInternAtom(display, "UTF8_STRING", False);
	Atom incrAtom = XInternAtom(display, "INCR", False);
	using Clock = std::chrono::steady_clock;
	auto wait = [&](Clock::time_point deadline)
	{
		auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
		if (left <= 0)
			return false;
		connection->WaitForEvents((int)left);
		return true;
	};
	for (Atom target : { utf8Atom, (Atom)XA_STRING })
	{
		XConvertSelection(display, clipboard, target, clipboard, window, CurrentTime);
		XFlush(display);

		// Wait 500 ms for a response
		XEvent event = {};
		auto deadline = Clock::now() + std::chrono::milliseconds(500);
		while (true)
		{
			if (XCheckTypedWindowEvent(display, window, SelectionNotify, &event))
				break;
			if (!wait(deadline))
				return {};
		}
		if (event.xselection.property == None)
			continue; // refused

		// The property's news so far, from before the answer, passed over:
		// an INCR transfer's chunks are told by the news after it
		XPropertyEvent wanted = {};
		wanted.window = window;
		wanted.atom = clipboard;
		XEvent stale = {};
		while (XCheckIfEvent(display, &stale, IsIncrChunk, (XPointer)&wanted))
		{
		}

		Atom type = None;
		int format = 0;
		unsigned long count = 0;
		std::vector<uint8_t> data = GetWindowProperty(clipboard, type, format, count);
		XDeleteProperty(display, window, clipboard);
		XFlush(display);
		if (type == incrAtom)
		{
			// The delete above asks for the first chunk
			data.clear();
			deadline = Clock::now() + std::chrono::milliseconds(500);
			while (true)
			{
				XEvent chunkEvent = {};
				if (!XCheckIfEvent(display, &chunkEvent, IsIncrChunk, (XPointer)&wanted))
				{
					if (!wait(deadline))
						return {};
					continue;
				}
				Atom chunkType = None;
				int chunkFormat = 0;
				unsigned long chunkCount = 0;
				std::vector<uint8_t> chunk = GetWindowProperty(clipboard, chunkType, chunkFormat, chunkCount);
				XDeleteProperty(display, window, clipboard);
				XFlush(display);
				if (chunk.empty())
					break;
				type = chunkType;
				format = chunkFormat;
				data.insert(data.end(), chunk.begin(), chunk.end());
				deadline = Clock::now() + std::chrono::milliseconds(500);
			}
			count = data.size();
		}
		if ((type != utf8Atom && type != XA_STRING) || format != 8 || count <= 0 || data.empty())
			return {};

		data.push_back(0);
		if (type == utf8Atom)
			return (char*)data.data();
		std::string text;
		for (size_t i = 0; data[i] != 0; i++)
		{
			unsigned char c = data[i];
			if (c < 0x80)
				text.push_back((char)c);
			else
			{
				text.push_back((char)(0xC0 | (c >> 6)));
				text.push_back((char)(0x80 | (c & 0x3F)));
			}
		}
		return text;
	}
	return {};
}

void X11DisplayWindow::SetClipboardText(const std::string& text)
{
	clipboardText = text;

	auto connection = GetX11Connection();
	Atom clipboard = connection->GetAtom("CLIPBOARD");
	if (clipboard == None)
		return;

	XSetSelectionOwner(display, XA_PRIMARY, window, CurrentTime);
	XSetSelectionOwner(display, clipboard, window, CurrentTime);
}

Point X11DisplayWindow::MapFromGlobal(const Point& pos) const
{
	double dpiscale = GetDpiScale();
	Window root = XRootWindow(display, screen);
	Window child = {};
	int srcx = (int)std::round(pos.x * dpiscale);
	int srcy = (int)std::round(pos.y * dpiscale);
	int destx = 0;
	int desty = 0;
	/*Bool result =*/ XTranslateCoordinates(display, root, window, srcx, srcy, &destx, &desty, &child);
	return Point(destx / dpiscale, desty / dpiscale);
}

Point X11DisplayWindow::MapToGlobal(const Point& pos) const
{
	double dpiscale = GetDpiScale();
	Window root = XRootWindow(display, screen);
	Window child = {};
	int srcx = (int)std::round(pos.x * dpiscale);
	int srcy = (int)std::round(pos.y * dpiscale);
	int destx = 0;
	int desty = 0;
	/*Bool result =*/ XTranslateCoordinates(display, window, root, srcx, srcy, &destx, &desty, &child);
	return Point(destx / dpiscale, desty / dpiscale);
}

void* X11DisplayWindow::GetNativeHandle()
{
	return reinterpret_cast<void*>(window);
}

void X11DisplayWindow::OnEvent(XEvent* event)
{
	if (event->type == ConfigureNotify)
		OnConfigureNotify(event);
	else if (event->type == ClientMessage)
		OnClientMessage(event);
	else if (event->type == Expose)
		OnExpose(event);
	else if (event->type == FocusIn)
		OnFocusIn(event);
	else if (event->type == FocusOut)
	{
		GrabCursor(false);
		OnFocusOut(event);
	}
	else if (event->type == PropertyNotify)
		OnPropertyNotify(event);
	else if (event->type == KeyPress)
		OnKeyPress(event);
	else if (event->type == KeyRelease)
		OnKeyRelease(event);
	else if (event->type == ButtonPress)
		OnButtonPress(event);
	else if (event->type == ButtonRelease)
		OnButtonRelease(event);
	else if (event->type == MotionNotify)
		OnMotionNotify(event);
	else if (event->type == LeaveNotify)
		OnLeaveNotify(event);
	else if (event->type == SelectionClear)
		OnSelectionClear(event);
	else if (event->type == SelectionNotify)
		OnSelectionNotify(event);
	else if (event->type == SelectionRequest)
		OnSelectionRequest(event);
}

void X11DisplayWindow::OnConfigureNotify(XEvent* event)
{
	ClientSizeX = event->xconfigure.width;
	ClientSizeY = event->xconfigure.height;
	windowHost->OnWindowGeometryChanged();
}

void X11DisplayWindow::OnClientMessage(XEvent* event)
{
	auto connection = GetX11Connection();
	Atom protocolsAtom = connection->GetAtom("WM_PROTOCOLS");
	if (protocolsAtom != None && event->xclient.message_type == protocolsAtom)
	{
		Atom deleteAtom = connection->GetAtom("WM_DELETE_WINDOW");
		Atom pingAtom = connection->GetAtom("_NET_WM_PING");

		Atom protocol = event->xclient.data.l[0];
		if (deleteAtom != None && protocol == deleteAtom)
		{
			windowHost->OnWindowClose();
		}
		else if (pingAtom != None && protocol == pingAtom)
		{
			XSendEvent(display, RootWindow(display, screen), False, SubstructureNotifyMask | SubstructureRedirectMask, event);
		}
	}
}

void X11DisplayWindow::OnExpose(XEvent* event)
{
	windowHost->OnWindowPaint();
}

void X11DisplayWindow::OnFocusIn(XEvent* event)
{
	if (xic && !keyboardLocked)
		XSetICFocus(xic);

	RawInput.Focused = true;

	// No KeyPress comes for a key held as the focus comes, nor a ButtonPress
	// for a button: each is read from the server, so that the host finds it
	// held (GetKeyState), as the original's poll finds it in Windows' state
	// (Engine::PressHeldKeys; bugs.md ENG.40), named as a KeyPress would
	// name it now, by the keyboard's modifiers (Num Lock) and layout group
	unsigned int state = 0;
	XkbStateRec xkbState = {};
	if (XkbGetState(display, XkbUseCoreKbd, &xkbState) == Success)
		state = XkbBuildCoreState(xkbState.lookup_mods, xkbState.group);
	char keys[32] = {};
	XQueryKeymap(display, keys);
	for (int code = 8; code < 256; code++)
	{
		if (keys[code / 8] & (1 << (code % 8)))
		{
			XEvent press = {};
			press.type = KeyPress;
			press.xkey.display = display;
			press.xkey.keycode = code;
			press.xkey.state = state;
			InputKey key = GetInputKey(&press);
			pressedAs[code] = key;
			keyState[key] = true;
		}
	}
	Window root = 0, child = 0;
	int rootX = 0, rootY = 0, x = 0, y = 0;
	unsigned int buttons = 0;
	if (XQueryPointer(display, window, &root, &child, &rootX, &rootY, &x, &y, &buttons))
	{
		keyState[InputKey::LeftMouse] = (buttons & Button1Mask) != 0;
		keyState[InputKey::MiddleMouse] = (buttons & Button2Mask) != 0;
		keyState[InputKey::RightMouse] = (buttons & Button3Mask) != 0;
	}

	windowHost->OnWindowActivated();
}

void X11DisplayWindow::OnFocusOut(XEvent* event)
{
	RawInput.Focused = false;

	// The host is told, which resets its input there, and the keys held are
	// forgotten with no release made up for them: no KeyRelease reaches an
	// unfocused window, and none reaches the original's at a focus loss
	// (bugs.md ENG.40). A button's ButtonRelease comes while the pointer is
	// over the window, as Windows sends the original's its button messages.
	windowHost->OnWindowDeactivated();
	for (auto& it : keyState)
		if (it.first != InputKey::LeftMouse && it.first != InputKey::MiddleMouse && it.first != InputKey::RightMouse)
			it.second = false;
}

// The window manager's state: maximized both ways where it was not is
// Maximize asked of the window -- its title bar's button, a double click on
// it, its window menu's -- which the host takes or leaves (DisplayWindowHost::
// OnWindowMaximize), the game's window toggling full screen, un-maximized as
// it goes (ShowFullscreen). None is read in full screen, ours or the window
// manager's (some mark a full screen window maximized), nor between an
// un-maximize asked and its answer
void X11DisplayWindow::OnPropertyNotify(XEvent* event)
{
	auto connection = GetX11Connection();
	Atom stateAtom = connection->GetAtom("_NET_WM_STATE");
	if (stateAtom == None || event->xproperty.atom != stateAtom)
		return;

	Atom vertAtom = connection->GetAtom("_NET_WM_STATE_MAXIMIZED_VERT");
	Atom horzAtom = connection->GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ");
	Atom fullscreenAtom = connection->GetAtom("_NET_WM_STATE_FULLSCREEN");
	bool vert = false, horz = false, fullscreen = false;
	Atom type = None;
	int format = 0;
	unsigned long count = 0;
	std::vector<uint8_t> state = GetWindowProperty(stateAtom, type, format, count);
	if (type == XA_ATOM && format == 32)
	{
		const long* atoms = reinterpret_cast<const long*>(state.data());
		for (unsigned long i = 0; i < count; i++)
		{
			Atom atom = (Atom)atoms[i];
			vert = vert || (vertAtom != None && atom == vertAtom);
			horz = horz || (horzAtom != None && atom == horzAtom);
			fullscreen = fullscreen || (fullscreenAtom != None && atom == fullscreenAtom);
		}
	}

	if (fullscreen)
		return;
	if (!vert || !horz)
	{
		isMaximized = false;
		unmaximizing = false;
		return;
	}
	bool asked = !isMaximized && !unmaximizing && !isFullscreen;
	isMaximized = true;
	if (asked)
		windowHost->OnWindowMaximize();
}

// A key by its place and the layout in force, as the SDL3 window names it
// (EvdevKeyName): the X key code is the evdev code less 8, the layout the
// event's group, a key that group has no symbol for named by the first
// group's, and the keypad by the event's Num Lock
InputKey X11DisplayWindow::GetInputKey(XEvent* event)
{
	if (event->type == KeyPress || event->type == KeyRelease)
	{
		if (event->xkey.keycode < 8 || event->xkey.keycode > 255)
			return InputKey{};
		int group = XkbGroupForCoreState(event->xkey.state);
		auto levelZero = [&](uint32_t code)
		{
			KeySym keysym = XkbKeycodeToKeysym(display, (KeyCode)(code + 8), group, 0);
			if (keysym == NoSymbol && group != 0)
				keysym = XkbKeycodeToKeysym(display, (KeyCode)(code + 8), 0, 0);
			return (uint32_t)keysym;
		};
		unsigned int numLockMask = XkbKeysymToModifiers(display, XK_Num_Lock);
		bool numLock = numLockMask != 0 && (event->xkey.state & numLockMask) != 0;
		return EvdevKeyName(event->xkey.keycode - 8, numLock, levelZero);
	}
	else if (event->type == ButtonPress || event->type == ButtonRelease)
	{
		switch (event->xbutton.button)
		{
		case 1: return InputKey::LeftMouse;
		case 2: return InputKey::MiddleMouse;
		case 3: return InputKey::RightMouse;
		case 4: return InputKey::MouseWheelUp;
		case 5: return InputKey::MouseWheelDown;
		// case 6: return InputKey::XButton1;
		// case 7: return InputKey::XButton2;
		default: break;
		}
	}
	return {};
}

Point X11DisplayWindow::GetMousePos(XEvent* event)
{
	double dpiScale = GetDpiScale();
	int x = event->xbutton.x;
	int y = event->xbutton.y;
	return Point(x / dpiScale, y / dpiScale);
}

void X11DisplayWindow::OnKeyPress(XEvent* event)
{
	// If we ever want to track keypress repeat:
	// char keyboard_state[32];
	// XQueryKeymap(display, keyboard_state);
	// unsigned int keycode = event->xkey.keycode;
	// bool isrepeat = event->type == KeyPress && keyboard_state[keycode / 8] & (1 << keycode % 8);

	// A key's repeats and its release are the name it was pressed as,
	// whatever Num Lock or the layout became meanwhile, as the SDL3 window's
	unsigned int code = event->xkey.keycode & 0xff;
	InputKey key = pressedAs[code];
	if (key == InputKey{} || !keyState[key])
		pressedAs[code] = key = GetInputKey(event);
	keyState[key] = true;
	windowHost->OnWindowKeyDown(key);

	std::string text;
	if (xic && !keyboardLocked) // utf-8 text input, off in play
	{
		Status status = {};
		KeySym keysym = NoSymbol;
		char buffer[32] = {};
		Xutf8LookupString(xic, &event->xkey, buffer, sizeof(buffer) - 1, &keysym, &status);
		if (status == XLookupChars || status == XLookupBoth)
			text = buffer;
	}
	else // latin-1 input fallback
	{
		const int buff_size = 16;
		char buff[buff_size];
		int result = XLookupString(&event->xkey, buff, buff_size - 1, nullptr, nullptr);
		if (result < 0) result = 0;
		if (result > (buff_size - 1)) result = buff_size - 1;
		buff[result] = 0;
		text = std::string(buff, result);

		// Lazy way to convert to utf-8
		for (char& c : text)
		{
			if (c < 0)
				c = '?';
		}
	}

	// Delete types nothing, which Xlib gives DEL (0x7F): Windows sends no
	// WM_CHAR for VK_DELETE (dx-reverse-info/windrv-dll.md, the keys), and
	// the SDL3 window types none
	text.erase(std::remove(text.begin(), text.end(), '\x7f'), text.end());

	if (!text.empty())
		windowHost->OnWindowKeyChar(std::move(text));
}

void X11DisplayWindow::OnKeyRelease(XEvent* event)
{
	unsigned int code = event->xkey.keycode & 0xff;
	InputKey key = pressedAs[code];
	if (key == InputKey{})
		key = GetInputKey(event);
	pressedAs[code] = InputKey{};
	keyState[key] = false;
	windowHost->OnWindowKeyUp(key);
}

void X11DisplayWindow::OnButtonPress(XEvent* event)
{
	// Note: this only gets called if XInput is not available

	InputKey key = GetInputKey(event);
	if (key == InputKey::MouseWheelUp || key == InputKey::MouseWheelDown)
	{
		// A wheel's notch, as OnXInputEvent takes it
		windowHost->OnWindowMouseWheel(GetMousePos(event), key);
		return;
	}
	keyState[key] = true;
	if (IsDoubleClick(key, event->xbutton.time, event->xbutton.x, event->xbutton.y))
		windowHost->OnWindowMouseDoubleclick(GetMousePos(event), key);
	else
		windowHost->OnWindowMouseDown(GetMousePos(event), key);
}

void X11DisplayWindow::OnButtonRelease(XEvent* event)
{
	// Note: this only gets called if XInput is not available

	InputKey key = GetInputKey(event);
	if (key == InputKey::MouseWheelUp || key == InputKey::MouseWheelDown)
		return; // the notch was its press's (OnButtonPress)
	keyState[key] = false;
	windowHost->OnWindowMouseUp(GetMousePos(event), key);
}

void X11DisplayWindow::OnMotionNotify(XEvent* event)
{
	// Note: this only gets called if XInput is not available

	double dpiScale = GetDpiScale();
	int x = event->xmotion.x;
	int y = event->xmotion.y;
	if (isCursorEnabled)
	{
		windowHost->OnWindowMouseMove(Point(x / dpiScale, y / dpiScale));
	}
	else
	{
		int dx = x - RawInput.LastX;
		int dy = y - RawInput.LastY;
		RawInput.LastX = x;
		RawInput.LastY = y;
		int centerX = ClientSizeX / 2;
		int centerY = ClientSizeY / 2;
		if (x != centerX || y != centerY)
		{
			// We still have to contain the mouse even if we are using XInput.
			// Maybe we can do that in a simpler way if XInput is available?
			XWarpPointer(display, window, window, 0, 0, ClientSizeX, ClientSizeY, centerX, centerY);

			// Use the accelerated mouse cursor if there's no XInput 2.0 support
			if (!GetX11Connection()->XInput2Supported)
			{
				if (dx != 0 || dy != 0)
					windowHost->OnWindowRawMouseMove(dx, dy);
			}
		}
	}
}

bool X11DisplayWindow::OnXInputEvent(XEvent* event)
{
	// This API is so horrible it makes Win32 look attractive!
	if (event->xcookie.evtype == XI_ButtonPress || event->xcookie.evtype == XI_ButtonRelease)
	{
		auto deviceEvent = (XIDeviceEvent*)event->xcookie.data;
		if (deviceEvent->event != window)
			return false;

		InputKey key = {};
		switch (deviceEvent->detail)
		{
		case 1: key = InputKey::LeftMouse; break;
		case 2: key = InputKey::MiddleMouse; break;
		case 3: key = InputKey::RightMouse; break;
		case 4: key = InputKey::MouseWheelUp; break;
		case 5: key = InputKey::MouseWheelDown; break;
		// case 6: key = InputKey::XButton1; break;
		// case 7: key = InputKey::XButton2; break;
		default: return false;
		}

		double dpiScale = GetDpiScale();
		int x = (int)std::round(deviceEvent->event_x);
		int y = (int)std::round(deviceEvent->event_y);
		Point mousePos(x / dpiScale, y / dpiScale);

		if (!isCursorEnabled && !GetX11Connection()->RawEventsInGrabs)
		{
			// Raw input gets blocked until we ungrab the pointer (the press's
			// own grab, under XInput 2.0). Worst design EVER.
			XUngrabPointer(display, deviceEvent->time);
		}

		// A wheel's notch comes as a press and release of button 4 or 5, the
		// server making a high-resolution wheel's and a touchpad's scroll
		// into notches (its emulated buttons): the press is the host's wheel,
		// which gives MouseW its delta and then presses and releases the
		// notch, as the original's WM_MOUSEWHEEL (WinDrv.dll 0x1110b2bd), and
		// the release is nothing
		if (key == InputKey::MouseWheelUp || key == InputKey::MouseWheelDown)
		{
			if (event->xcookie.evtype == XI_ButtonPress)
				windowHost->OnWindowMouseWheel(mousePos, key);
			return true;
		}

		if (event->xcookie.evtype == XI_ButtonPress)
		{
			keyState[key] = true;
			if (IsDoubleClick(key, deviceEvent->time, x, y))
				windowHost->OnWindowMouseDoubleclick(mousePos, key);
			else
				windowHost->OnWindowMouseDown(mousePos, key);
		}
		else
		{
			keyState[key] = false;
			windowHost->OnWindowMouseUp(mousePos, key);
		}

		return true;
	}
	else if (event->xcookie.evtype == XI_Motion)
	{
		auto deviceEvent = (XIDeviceEvent*)event->xcookie.data;
		if (deviceEvent->event != window)
			return false;

		if (isCursorEnabled)
		{
			double dpiScale = GetDpiScale();
			int x = (int)std::round(deviceEvent->event_x);
			int y = (int)std::round(deviceEvent->event_y);
			Point mousePos(x / dpiScale, y / dpiScale);
			windowHost->OnWindowMouseMove(Point(x / dpiScale, y / dpiScale));
		}
		else if (RawInput.Focused)
		{
			int x = (int)std::round(deviceEvent->event_x);
			int y = (int)std::round(deviceEvent->event_y);
			int centerX = ClientSizeX / 2;
			int centerY = ClientSizeY / 2;
			if (x != centerX || y != centerY)
			{
				// A device whose x and y are a place, whose raw values are no
				// move (XI_RawMotion), moves by the pointer's move from the
				// middle, as the original's window moves by every pointer's
				// (WM_MOUSEMOVE; dx-reverse-info/windrv-dll.md, the keys)
				if (GetX11Connection()->IsAbsolutePointer(deviceEvent->sourceid))
					windowHost->OnWindowRawMouseMove(x - centerX, y - centerY);

				// We still have to contain the mouse even if we are using XInput.
				// Maybe we can do that in a simpler way?
				XWarpPointer(display, window, window, 0, 0, ClientSizeX, ClientSizeY, centerX, centerY);
			}
		}

		return true;
	}
	else if (!isCursorEnabled && RawInput.Focused && event->xcookie.evtype == XI_RawMotion)
	{
		// The pointer's own move alone, as the original moves by the pointer's
		// moves (dx-reverse-info/windrv-dll.md, the keys): valuators 0 and 1,
		// x and y, each by its mask bit, as SDL3 reads them, one the event
		// leaves out not moved; the others (a touchpad's and a wheel's
		// scroll) move nothing. A device whose x and y are a place (a
		// tablet's pen, a virtual machine's pointer) moves by the pointer
		// (XI_Motion)
		auto rawEvent = (XIRawEvent*)event->xcookie.data;
		if (GetX11Connection()->IsAbsolutePointer(rawEvent->sourceid))
			return true;

		double values[2] = {};
		const double* rawValuator = rawEvent->raw_values;
		for (int i = 0; i < 2 && i < rawEvent->valuators.mask_len * 8; i++)
		{
			if (XIMaskIsSet(rawEvent->valuators.mask, i))
				values[i] = *rawValuator++;
		}

		// Values seems to be integers for my mouse. Is that always the case?
		int dx = (int)std::round(values[0]);
		int dy = (int)std::round(values[1]);
		if (dx != 0 || dy != 0)
			windowHost->OnWindowRawMouseMove(dx, dy);

		return true;
	}
	return false;
}

// X11 counts no clicks: a press is a double click where Windows' would be
// one, the second press of a button within Windows' default double-click
// time (500 ms) and rectangle (4 pixels square about the first press), a
// third a press again, as Windows sends WM_LBUTTONDBLCLK in place of the
// second WM_LBUTTONDOWN (the Win32 window's). The game's window takes a
// double click as a press (Engine::OnWindowMouseDoubleclick), the Advanced
// Options' and the file dialogs' trees and lists theirs.
bool X11DisplayWindow::IsDoubleClick(InputKey key, Time time, int x, int y)
{
	bool isDouble = LastClick.open && LastClick.key == key && time - LastClick.time <= 500 &&
		std::abs(x - LastClick.x) <= 2 && std::abs(y - LastClick.y) <= 2;
	LastClick.key = key;
	LastClick.time = time;
	LastClick.x = x;
	LastClick.y = y;
	LastClick.open = !isDouble;
	return isDouble;
}

void X11DisplayWindow::OnLeaveNotify(XEvent* event)
{
	windowHost->OnWindowMouseLeave();
}

void X11DisplayWindow::OnSelectionClear(XEvent* event)
{
	clipboardText.clear();
}

void X11DisplayWindow::OnSelectionNotify(XEvent* event)
{
	// This is handled in GetClipboardText
}

// The selection served as ICCSM has it: TARGETS as atoms (TARGETS,
// UTF8_STRING, STRING), the text as UTF-8 for UTF8_STRING and as Latin-1
// for STRING (what Latin-1 lacks a ?), a MULTIPLE request's pairs read from
// the requestor's own property. A paste into the game of what it copied
// asks nothing: it takes the text as it is (GetClipboardText).
void X11DisplayWindow::OnSelectionRequest(XEvent* event)
{
	Window requestor = event->xselectionrequest.requestor;

	X11Connection* connection = GetX11Connection();
	Atom targetsAtom = connection->GetAtom("TARGETS");
	Atom multipleAtom = connection->GetAtom("MULTIPLE");
	// Interned here: GetAtom takes only atoms the server has, and a fresh
	// server may lack these two
	Atom utf8Atom = XInternAtom(display, "UTF8_STRING", False);
	Atom atomPairAtom = XInternAtom(display, "ATOM_PAIR", False);

	struct Request { Atom target; Atom property; };
	std::vector<Request> requests;

	bool multiple = event->xselectionrequest.target == multipleAtom;
	if (multiple)
	{
		// The pairs of target and property in the requestor's property
		Atom actualType = None;
		int actualFormat = 0;
		unsigned long itemCount = 0;
		unsigned long bytesRemaining = 0;
		unsigned char* data = nullptr;
		if (event->xselectionrequest.property != None &&
			XGetWindowProperty(display, requestor, event->xselectionrequest.property, 0L, 65536L, False, AnyPropertyType, &actualType, &actualFormat, &itemCount, &bytesRemaining, &data) == Success)
		{
			if (data && actualFormat == 32)
			{
				unsigned long* atoms = (unsigned long*)data;
				for (unsigned long i = 0; i + 1 < itemCount; i += 2)
					requests.push_back({ (Atom)atoms[i], (Atom)atoms[i + 1] });
			}
			if (data)
				XFree(data);
		}
	}
	else
	{
		requests.push_back({ event->xselectionrequest.target, event->xselectionrequest.property });
	}

	// Latin-1 for STRING: a character past it a ?
	auto latin1 = [](const std::string& utf8)
	{
		std::string out;
		for (size_t i = 0; i < utf8.size(); )
		{
			unsigned char c = (unsigned char)utf8[i];
			if (c < 0x80) { out.push_back((char)c); i++; }
			else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size())
			{
				unsigned int code = ((c & 0x1F) << 6) | ((unsigned char)utf8[i + 1] & 0x3F);
				out.push_back(code < 0x100 ? (char)code : '?');
				i += 2;
			}
			else
			{
				out.push_back('?');
				i++;
				while (i < utf8.size() && ((unsigned char)utf8[i] & 0xC0) == 0x80)
					i++;
			}
		}
		return out;
	};

	for (Request& request : requests)
	{
		Atom xtarget = request.target;
		Atom xproperty = request.property != None ? request.property : xtarget;

		if (xtarget == targetsAtom)
		{
			Atom targets[3] = { targetsAtom, utf8Atom, XA_STRING };
			XChangeProperty(display, requestor, xproperty, XA_ATOM, 32, PropModeReplace, (unsigned char*)targets, 3);
		}
		else if (xtarget == utf8Atom)
		{
			XChangeProperty(display, requestor, xproperty, utf8Atom, 8, PropModeReplace, (const unsigned char*)clipboardText.c_str(), (int)clipboardText.size());
		}
		else if (xtarget == XA_STRING)
		{
			std::string text = latin1(clipboardText);
			XChangeProperty(display, requestor, xproperty, XA_STRING, 8, PropModeReplace, (const unsigned char*)text.c_str(), (int)text.size());
		}
		else
		{
			request.property = None; // not served
		}
	}

	if (multiple)
	{
		// The pairs written back, a refused one's property None
		std::vector<unsigned long> pairs;
		for (const Request& request : requests)
		{
			pairs.push_back(request.target);
			pairs.push_back(request.property);
		}
		if (event->xselectionrequest.property != None)
			XChangeProperty(display, requestor, event->xselectionrequest.property, atomPairAtom != None ? atomPairAtom : XA_ATOM, 32, PropModeReplace, (unsigned char*)pairs.data(), (int)pairs.size());
	}

	XEvent response = {};
	response.xselection.type = SelectionNotify;
	response.xselection.display = event->xselectionrequest.display;
	response.xselection.requestor = requestor;
	response.xselection.selection = event->xselectionrequest.selection;
	response.xselection.target = event->xselectionrequest.target;
	response.xselection.property = multiple ? event->xselectionrequest.property : (requests.empty() ? None : requests.front().property);
	response.xselection.time = event->xselectionrequest.time;
	XSendEvent(display, requestor, False, 0, &response);
}

void X11DisplayWindow::CreateGLContext()
{
	EGLint majorVer, minorVer;

	// OpenGL ES 3.1 (the GL device's ES shaders are GLSL ES 3.10; a 3.2
	// driver gives its 3.2 context) or desktop 4.2 core (its desktop shaders
	// are GLSL 4.20): ES takes the ES3 bit and no profile mask.
	EGLint ctxAttrs[7];
	int ctxAttrCount = 0;
	ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_MAJOR_VERSION;
	ctxAttrs[ctxAttrCount++] = m_GLIsES ? 3 : 4;
	ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_MINOR_VERSION;
	ctxAttrs[ctxAttrCount++] = m_GLIsES ? 1 : 2;
	if (!m_GLIsES)
	{
		ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
		ctxAttrs[ctxAttrCount++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
	}
	ctxAttrs[ctxAttrCount++] = EGL_NONE;

	EGLint renderableBit = m_GLIsES ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;

	EGLint confAttrs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_CONFORMANT,	renderableBit,
		EGL_RENDERABLE_TYPE, renderableBit,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

		// No depth or stencil: the GL device draws its scene into buffers of
		// its own, the window's taking only the present
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,

		EGL_NONE
	};

	if (!eglBindAPI(m_GLIsES ? EGL_OPENGL_ES_API : EGL_OPENGL_API))
		throw std::runtime_error("EGL cannot bind to the GL API");

	// EGL 1.5's call, looked up: a library of 1.4 lacks it, and is refused
	// as the version test below refuses it, not called into
	auto getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYPROC)eglGetProcAddress("eglGetPlatformDisplay");
	if (!getPlatformDisplay)
		throw std::runtime_error("EGL version 1.5 or higher required");
	m_EGLDisplay = getPlatformDisplay(EGL_PLATFORM_X11_KHR, display, nullptr);

	if (m_EGLDisplay == EGL_NO_DISPLAY)
		throw std::runtime_error("No EGL Display found");

	if (!eglInitialize(m_EGLDisplay, &majorVer, &minorVer))
		throw std::runtime_error("Error initializing EGL");

	if (majorVer < 1 || (majorVer == 1 && minorVer < 5))
		throw std::runtime_error("EGL version 1.5 or higher required");

	EGLConfig configs[32];
	EGLint config_count = 32;

	if (!eglChooseConfig(m_EGLDisplay, confAttrs, configs, config_count, &config_count) || config_count == 0)
		throw std::runtime_error("Cannot choose EGL config. Error code: " + std::to_string(eglGetError()));

	EGLConfig surfaceConfig = nullptr;
	for (EGLint i = 0 ; i < config_count ; i++)
	{
		m_EGLSurface = eglCreateWindowSurface(m_EGLDisplay, configs[i], window, nullptr);
		if (m_EGLSurface != EGL_NO_SURFACE)
		{
			surfaceConfig = configs[i];
			break;
		}
	}

	if (m_EGLSurface == EGL_NO_SURFACE)
		throw std::runtime_error("Failed to create EGL surface. Error code: " + std::to_string(eglGetError()));

	// A context of no config where the display takes one
	// (EGL_KHR_no_config_context), else the surface's
	const char* extensions = eglQueryString(m_EGLDisplay, EGL_EXTENSIONS);
	bool noConfig = extensions && strstr(extensions, "EGL_KHR_no_config_context");
	m_EGLContext = eglCreateContext(m_EGLDisplay, noConfig ? EGL_NO_CONFIG_KHR : surfaceConfig, EGL_NO_CONTEXT, ctxAttrs);
	if (m_EGLContext == EGL_NO_CONTEXT)
	{
		// The surface made first let go of: the destructor, which tests
		// for a context, would leave it
		EGLint error = eglGetError();
		eglDestroySurface(m_EGLDisplay, m_EGLSurface);
		m_EGLSurface = EGL_NO_SURFACE;
		throw std::runtime_error("Failed to create EGL context. Error code: " + std::to_string(error));
	}
}

void X11DisplayWindow::MakeGLContextCurrent()
{
	if (!eglMakeCurrent(m_EGLDisplay, m_EGLSurface, m_EGLSurface, m_EGLContext))
		throw std::runtime_error("Error in eglMakeCurrent(). Error code: " + std::to_string(eglGetError()));
}

bool X11DisplayWindow::SetGLSwapInterval(int interval)
{
	// EGL has no late swap: a negative interval is clamped to the config's
	// least (0 on Mesa unless vblank_mode forces sync), so it is refused
	// here, and the GL device falls back to 1
	if (interval < 0)
		return false;
	return eglSwapInterval(m_EGLDisplay, interval);
}

void X11DisplayWindow::SwapGLBuffers()
{
	eglSwapBuffers(m_EGLDisplay, m_EGLSurface);
}

DisplayWindow::GLFuncPtr X11DisplayWindow::GetGLProcAddress(const char* name)
{
	return (GLFuncPtr)eglGetProcAddress(name);
}

// This is to avoid needing all the Vulkan headers and the volk binding library just for this:
#ifndef VK_VERSION_1_0

#define VKAPI_CALL
#define VKAPI_PTR VKAPI_CALL

typedef uint32_t VkFlags;
typedef enum VkStructureType { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR = 1000004000, VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF } VkStructureType;
typedef enum VkResult { VK_SUCCESS = 0, VK_RESULT_MAX_ENUM = 0x7FFFFFFF } VkResult;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);

#ifndef VK_KHR_xlib_surface

typedef VkFlags VkXlibSurfaceCreateFlagsKHR;
typedef struct VkXlibSurfaceCreateInfoKHR
{
    VkStructureType                sType;
    const void*                    pNext;
    VkXlibSurfaceCreateFlagsKHR    flags;
    Display*                       dpy;
    Window                         window;
} VkXlibSurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateXlibSurfaceKHR)(VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#endif
#endif

class ZWidgetX11VulkanLoader
{
public:
	ZWidgetX11VulkanLoader()
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

	~ZWidgetX11VulkanLoader()
	{
		dlclose(module);
	}

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
	void* module = nullptr;
};

VkSurfaceKHR X11DisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	static ZWidgetX11VulkanLoader loader;

	auto vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)loader.vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
	if (!vkCreateXlibSurfaceKHR)
		throw std::runtime_error("Could not create vulkan surface");

	VkXlibSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
	createInfo.dpy = display;
	createInfo.window = window;

	VkSurfaceKHR surface = {};
	VkResult result = vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Could not create vulkan surface");
	return surface;
}

std::vector<std::string> X11DisplayWindow::GetVulkanInstanceExtensions()
{
	return { "VK_KHR_surface", "VK_KHR_xlib_surface" };
}
