
#include "x11_connection.h"
#include "x11_display_window.h"
#include <stdexcept>
#include <chrono>
#include <clocale>
#include <string>

X11Connection* GetX11Connection()
{
	static X11Connection connection;
	return &connection;
}

// Xlib's own handler, which reports an error and ends the program, and the
// windows of this program, which a window's error names (OnXError)
static XErrorHandler XlibErrorHandler = nullptr;
static std::map<Window, X11DisplayWindow*>* OwnWindows = nullptr;

// A guard of ours for a platform matter (bugs.md WIN.53): a request on
// another program's window that has gone meanwhile -- a clipboard request
// answered after its requestor's window is destroyed -- is passed over, as
// such a window is no fault of the game's; any other error goes to Xlib's
// handler. The MIT-SHM attach sets a handler of its own over this one.
static int OnXError(Display* display, XErrorEvent* error)
{
	if (error->error_code == BadWindow && OwnWindows && OwnWindows->find(error->resourceid) == OwnWindows->end())
		return 0;
	return XlibErrorHandler ? XlibErrorHandler(display, error) : 0;
}

X11Connection::X11Connection()
{
	// This is required by vulkan
	XInitThreads();

	display = XOpenDisplay(nullptr);
	if (!display)
		throw std::runtime_error("Could not open X11 display");

	OwnWindows = &windows;
	XlibErrorHandler = XSetErrorHandler(OnXError);

	// Make auto-repeat keys detectable
	Bool supports_detectable_autorepeat = {};
	XkbSetDetectableAutoRepeat(display, True, &supports_detectable_autorepeat);

	// The scale the windows and the screen's size share: Xft.dpi's
	if (char* value = XGetDefault(display, "Xft", "dpi"))
	{
		int dpi = std::atoi(value);
		if (dpi != 0)
			dpiScale = dpi / 96.0;
	}

	// The input method opened in the player's locale, as SDL opens its own:
	// Xlib's compose sequences and an XIM server's text are the locale's (in
	// the C locale only Latin-1's compose). The program's own locale is put
	// back after, the engine reading and writing numbers in the C locale's
	// form; the input method keeps the one it was opened in. A locale Xlib
	// does not support leaves the program's.
	std::string programLocale = setlocale(LC_CTYPE, nullptr) ? setlocale(LC_CTYPE, nullptr) : "C";
	if (setlocale(LC_CTYPE, "") && !XSupportsLocale())
		setlocale(LC_CTYPE, programLocale.c_str());

	// Loads the XMODIFIERS environment variable to see what IME to use
	XSetLocaleModifiers("");
	xim = XOpenIM(display, 0, 0, 0);
	if (!xim)
	{
		// fallback to internal input method
		XSetLocaleModifiers("@im=none");
		xim = XOpenIM(display, 0, 0, 0);
	}
	setlocale(LC_CTYPE, programLocale.c_str());

	// Look for XInput support
	int event = 0, error = 0;
	if (XQueryExtension(display, "XInputExtension", &XInputOpcode, &event, &error))
	{
		// We need XInput 2.0 support; 2.1 asked for, which sends raw events
		// through a grab (the window's lock holds the pointer by one)
		int major = 2, minor = 1;
		if (XIQueryVersion(display, &major, &minor) != BadRequest)
		{
			RawEventsInGrabs = major > 2 || (major == 2 && minor >= 1);
			// And we need a master pointer ID
			int ndevices = 0;
			XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &ndevices);
			if (devices)
			{
				for (int i = 0; i < ndevices; i++)
				{
					if (devices[i].use == XIMasterPointer)
					{
						MasterPointerID = devices[i].deviceid;
						XInput2Supported = true;
						break;
					}
				}
				XIFreeDeviceInfo(devices);
			}
		}
	}

	// This XInput API is the gift that just keeps on giving. Instead of routing events to the
	// focus window it sends it to the root window. Thanks!
	if (XInput2Supported)
	{
		unsigned char mask[3] = { 0 };
		XISetMask(mask, XI_RawMotion);
		XIEventMask eventmask;
		eventmask.deviceid = MasterPointerID;
		eventmask.mask_len = sizeof(mask);
		eventmask.mask = mask;
		Window root = XRootWindow(display, XDefaultScreen(display));
		XISelectEvents(display, root, &eventmask, 1);
	}
}

X11Connection::~X11Connection()
{
	XSetErrorHandler(XlibErrorHandler);
	OwnWindows = nullptr;
	for (auto& it : standardCursors)
		XFreeCursor(display, it.second);
	if (xim)
		XCloseIM(xim);
	XCloseDisplay(display);
}

Atom X11Connection::GetAtom(const std::string& name)
{
	auto it = atoms.find(name);
	if (it != atoms.end())
		return it->second;
	
	Atom atom = XInternAtom(display, name.c_str(), True);
	atoms[name] = atom;
	return atom;
}

// Whether a device's x and y valuators (0 and 1) are a place, not a move:
// a tablet's pen, a touchscreen. Asked of the server once a device.
bool X11Connection::IsAbsolutePointer(int deviceid)
{
	auto it = absolutePointers.find(deviceid);
	if (it != absolutePointers.end())
		return it->second;

	bool absolute = false;
	int count = 0;
	XIDeviceInfo* info = XIQueryDevice(display, deviceid, &count);
	if (info)
	{
		for (int i = 0; i < count; i++)
		{
			for (int c = 0; c < info[i].num_classes; c++)
			{
				if (info[i].classes[c]->type != XIValuatorClass)
					continue;
				auto valuator = (XIValuatorClassInfo*)info[i].classes[c];
				if (valuator->number == 0 || valuator->number == 1)
					absolute = absolute || valuator->mode == XIModeAbsolute;
			}
		}
		XIFreeDeviceInfo(info);
	}
	absolutePointers[deviceid] = absolute;
	return absolute;
}

void X11Connection::ProcessEvents()
{
	CheckNeedsUpdate();
	while (XPending(display) > 0)
	{
		XEvent event = {};
		XNextEvent(display, &event);
		// The input method takes its keys first (a dead key's compose, an
		// XIM's input): what it keeps goes no further
		if (XFilterEvent(&event, None))
			continue;
		DispatchEvent(&event);
	}
	CheckTimers();
}

void X11Connection::RunLoop()
{
	X11Connection* connection = GetX11Connection();
	while (!connection->ExitRunLoop && !connection->windows.empty())
	{
		ProcessEvents();
		WaitForEvents(GetTimerTimeout());
	}
	connection->ExitRunLoop = false; // So that closing a dialog doesn't close everything else
}

void X11Connection::ExitLoop()
{
	X11Connection* connection = GetX11Connection();
	connection->ExitRunLoop = true;
}

bool X11Connection::WaitForEvents(int timeout)
{
	int fd = XConnectionNumber(display);

	// The timeout in milliseconds, to seconds and microseconds; 0 is a poll.
	// The microseconds were divided, not multiplied, so a wait under a
	// second was none (a paste's 500 ms), and a 0 passed select garbage.
	struct timeval tv = {};
	if (timeout > 0)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	int result = select(fd + 1, &rfds, nullptr, nullptr, timeout >= 0 ? &tv : nullptr);
	return result > 0 && FD_ISSET(fd, &rfds);
}

void X11Connection::CheckNeedsUpdate()
{
	for (auto& it : windows)
	{
		if (it.second->needsUpdate)
		{
			it.second->needsUpdate = false;
			it.second->windowHost->OnWindowPaint();
		}
	}
}

void X11Connection::DispatchEvent(XEvent* event)
{
	if (XInput2Supported)
	{
		// XInput sends all raw input to the root window. We want it routed to the focused window
		if (event->xcookie.type == GenericEvent &&
			event->xcookie.extension == XInputOpcode &&
			XGetEventData(display, &event->xcookie))
		{
			for (auto& it : windows)
			{
				X11DisplayWindow* window = it.second;
				if (window->OnXInputEvent(event))
					break;
			}
			XFreeEventData(display, &event->xcookie);
		}
	}

	auto it = windows.find(event->xany.window);
	if (it != windows.end())
	{
		X11DisplayWindow* window = it->second;
		window->OnEvent(event);
	}
}

Size X11Connection::GetScreenSize()
{
	int screen = XDefaultScreen(display);

	int disp_width_px = XDisplayWidth(display, screen);
	int disp_height_px = XDisplayHeight(display, screen);
	// By the windows' scale (Xft.dpi), not the monitor's physical DPI: the
	// engine reads this size against the windows' sizes
	double dpiScale = this->dpiScale;

	return Size(disp_width_px / dpiScale, disp_height_px / dpiScale);
}

Size X11Connection::GetScreenPixelSize()
{
	int screen = XDefaultScreen(display);
	return Size(XDisplayWidth(display, screen), XDisplayHeight(display, screen));
}

static int64_t GetTimePoint()
{
	using namespace std::chrono;
	return (int64_t)(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void* X11Connection::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	timeoutMilliseconds = std::max(timeoutMilliseconds, 1);
	timers.push_back(std::make_shared<X11Timer>(timeoutMilliseconds, onTimer, GetTimePoint() + timeoutMilliseconds));
	return timers.back().get();
}

void X11Connection::StopTimer(void* timerID)
{
	for (auto it = timers.begin(); it != timers.end(); ++it)
	{
		if (it->get() == timerID)
		{
			timers.erase(it);
			return;
		}
	}
}

void X11Connection::CheckTimers()
{
	int64_t now = GetTimePoint();

	// The callback may stop timers. Iterators might invalidate.
	while (true)
	{
		std::shared_ptr<X11Timer> foundTimer;
		for (auto& timer : timers)
		{
			if (timer->nextTime < now)
			{
				foundTimer = timer;
				break;
			}
		}

		if (!foundTimer)
			break;

		// Not very precise, but these aren't high precision timers
		foundTimer->nextTime = now + foundTimer->timeoutMilliseconds;
		foundTimer->onTimer();
	}
}

int X11Connection::GetTimerTimeout()
{
	// No timer: the loop waits for an event, as Wayland's does (0, a poll,
	// spun a core while the error window was up)
	if (timers.empty())
		return -1;

	int64_t nextTime = timers.front()->nextTime;
	for (auto& timer : timers)
	{
		nextTime = std::min(nextTime, timer->nextTime);
	}

	int64_t now = GetTimePoint();
	return (int)std::max(nextTime - now, (int64_t)1);
}
