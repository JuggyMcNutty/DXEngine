#!/usr/bin/env python3
"""Keys or a mouse button held over a focus change, for FocusLossConsole (bugs.md ENG):
a uinput keyboard holds Left Shift and X, or a uinput mouse its left button, in the game's
window; the focus is given to a plain X window of this tool's own for <away> s and then back;
the keys go up <after> s later. It waits <delay> s first, then for the game's window to have
the focus, and stops, saying so, if another window has it. A mouse button goes to whatever
window the desktop's pointer is over: run the game full screen (DXCAP_FULLSCREEN=1) for
`mouse`. /dev/uinput must be writable; xdotool gives and takes the focus.

  focusheld.py keys|mouse [<delay> [<away> [<after>]]]   # 10 s, 4 s, 6 s"""
import ctypes, fcntl, os, struct, subprocess, sys, time

EV_SYN, EV_KEY, EV_REL, SYN_REPORT, BUS_USB = 0, 1, 2, 0, 3
KEY_LEFTSHIFT, KEY_X, BTN_LEFT, BTN_RIGHT, REL_X, REL_Y = 42, 45, 0x110, 0x111, 0, 1

def _IOC(d, t, nr, size):
    return (d << 30) | (size << 16) | (ord(t) << 8) | nr

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT = _IOC(1, 'U', 100, 4), _IOC(1, 'U', 101, 4), _IOC(1, 'U', 102, 4)
SETUP = struct.Struct('HHHH80sI')          # struct uinput_setup: input_id, name, ff_effects_max
UI_DEV_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = _IOC(1, 'U', 3, SETUP.size), _IOC(0, 'U', 1, 0), _IOC(0, 'U', 2, 0)
EVENT = struct.Struct('llHHi')             # struct input_event: time, type, code, value
ENV = dict(os.environ, DISPLAY=os.environ.get('DISPLAY', ':0'))

def xdo(*args):
    try:
        return subprocess.run(('xdotool',) + args, capture_output=True, text=True, timeout=3, env=ENV).stdout.strip()
    except Exception:
        return ''

def game_has_focus():
    # Either engine's window title starts with "Deus Ex"
    return xdo('getactivewindow', 'getwindowname').startswith('Deus Ex')

def emit(fd, kind, code, value):
    os.write(fd, EVENT.pack(0, 0, kind, code, value))
    os.write(fd, EVENT.pack(0, 0, EV_SYN, SYN_REPORT, 0))

def device(mouse):
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    if mouse:
        for b in (BTN_LEFT, BTN_RIGHT):
            fcntl.ioctl(fd, UI_SET_KEYBIT, b)
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_REL)
        for r in (REL_X, REL_Y):
            fcntl.ioctl(fd, UI_SET_RELBIT, r)
    else:
        for k in (KEY_LEFTSHIFT, KEY_X):
            fcntl.ioctl(fd, UI_SET_KEYBIT, k)
    name = b'dxcap focus mouse' if mouse else b'dxcap focus keys'
    fcntl.ioctl(fd, UI_DEV_SETUP, SETUP.pack(BUS_USB, 0x1209, 0xd0d7 if mouse else 0xd0d8, 1, name, 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    return fd

class OtherWindow:
    """A plain X window that takes the focus: Xlib by ctypes, nothing to install."""
    def __init__(self):
        x = self.x = ctypes.cdll.LoadLibrary('libX11.so.6')
        x.XOpenDisplay.restype = ctypes.c_void_p
        x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x.XDefaultRootWindow.restype = ctypes.c_ulong
        x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        x.XCreateSimpleWindow.restype = ctypes.c_ulong
        x.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.c_uint,
                                          ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_ulong]
        for f in ('XStoreName',):
            getattr(x, f).argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_char_p]
        for f in ('XMapWindow', 'XDestroyWindow'):
            getattr(x, f).argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        for f in ('XFlush', 'XCloseDisplay'):
            getattr(x, f).argtypes = [ctypes.c_void_p]
        self.d = x.XOpenDisplay(ENV['DISPLAY'].encode())
        if not self.d:
            sys.exit('no X display at ' + ENV['DISPLAY'])
        self.w = x.XCreateSimpleWindow(self.d, x.XDefaultRootWindow(self.d), 40, 40, 300, 200, 0, 0, 0x808080)
        x.XStoreName(self.d, self.w, b'dxcap other window')
        x.XMapWindow(self.d, self.w)
        x.XFlush(self.d)
    def close(self):
        self.x.XDestroyWindow(self.d, self.w)
        self.x.XFlush(self.d)
        self.x.XCloseDisplay(self.d)

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('keys', 'mouse'):
        sys.exit(__doc__)
    mouse = sys.argv[1] == 'mouse'
    delay, away, after = (float(sys.argv[i]) if len(sys.argv) > i else d for i, d in ((2, 10.0), (3, 4.0), (4, 6.0)))
    fd = device(mouse)
    codes = (BTN_LEFT,) if mouse else (KEY_LEFTSHIFT, KEY_X)
    t0 = time.time()
    def say(text):
        print('focusheld %6.2f: %s' % (time.time() - t0, text), flush=True)
    held = False
    other = None
    try:
        time.sleep(delay)
        end = time.time() + 30
        while not game_has_focus() and time.time() < end:
            time.sleep(0.25)
        if not game_has_focus():
            say('the game has not the focus (%r): nothing held' % xdo('getactivewindow', 'getwindowname'))
            return
        game = xdo('getactivewindow')
        for c in codes:
            emit(fd, EV_KEY, c, 1)
        held = True
        say('held: ' + ('the left button' if mouse else 'Left Shift and X'))
        time.sleep(4.0)
        other = OtherWindow()
        time.sleep(1.0)
        xdo('windowactivate', str(other.w))
        time.sleep(0.3)
        say('the focus away: %r' % xdo('getactivewindow', 'getwindowname'))
        time.sleep(away)
        xdo('windowactivate', game)
        time.sleep(0.3)
        say('the focus back: %r' % xdo('getactivewindow', 'getwindowname'))
        time.sleep(after)
    finally:
        if held:
            for c in codes:
                emit(fd, EV_KEY, c, 0)
            say('released')
        if other:
            other.close()
        time.sleep(0.2)
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)

if __name__ == '__main__':
    main()
