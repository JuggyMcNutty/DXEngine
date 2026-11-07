#!/usr/bin/env python3
"""Maximize asked of a window from outside, as its title bar's button asks it
(bugs.md WIN.15), beside a dxcap run: the EWMH _NET_WM_STATE request to the
root window, which the window manager takes (KWin, for Xwayland's windows).

    maximize.py <display> <window name pattern> [<delay> [<watch>]] [--prop] [--size <w>x<h>]

It waits <delay> s (15), finds the window as the harness does (xdotool's
search, the last of the name), asks it maximized both ways, then prints the
window's _NET_WM_STATE and size every half second for <watch> s (12). With
--prop it sets the window's _NET_WM_STATE itself, the window manager left
out: DXWidgets' X11 window, SDL's x11 driver and Wine's read the property as
it changes, where the window manager may not take a request. With --size it
asks the window manager to size the window to <w>x<h> in place of the
maximize, as a drag of its frame would (EWMH's _NET_MOVERESIZE_WINDOW, on
the user's behalf), and prints the least size the window gives the window
manager (WM_NORMAL_HINTS) with each line (bugs.md WIN.16): the original's
window holds itself to 640x480, its frame too (WinDrv.dll 0x1110a812),
which Wine hands its X11 window's hints as it works them out.

    tools/dxcap/maximize.py :0 ' - DXEngine$' 20 12     # DXEngine's window, any backend, a Windows build's too
    tools/dxcap/maximize.py :0 '^Deus Ex$' 40 12        # the original's, under Proton
    tools/dxcap/maximize.py :0 ' - DXEngine$' 20 6 --size 320x240   # dragged small

The game's window should go full screen (its state FULLSCREEN, not
MAXIMIZED_VERT and MAXIMIZED_HORZ, its size the screen's), and a widget
window, the Fatal Error window, maximize.
"""
import ctypes
import ctypes.util
import os
import subprocess
import sys
import time

CLIENT_MESSAGE = 33
SUBSTRUCTURE_NOTIFY = 1 << 19
SUBSTRUCTURE_REDIRECT = 1 << 20
XA_ATOM = 4
PROP_MODE_REPLACE = 0
NET_WM_STATE_ADD = 1
SOURCE_USER = 2             # a request on the user's behalf, as a pager's (EWMH)


class XClientMessageEvent(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong), ('send_event', ctypes.c_int),
                ('display', ctypes.c_void_p), ('window', ctypes.c_ulong),
                ('message_type', ctypes.c_ulong), ('format', ctypes.c_int),
                ('l', ctypes.c_long * 5)]


class XEvent(ctypes.Union):
    _fields_ = [('xclient', XClientMessageEvent), ('pad', ctypes.c_long * 24)]


class XSizeHints(ctypes.Structure):
    _fields_ = [('flags', ctypes.c_long), ('x', ctypes.c_int), ('y', ctypes.c_int),
                ('width', ctypes.c_int), ('height', ctypes.c_int),
                ('min_width', ctypes.c_int), ('min_height', ctypes.c_int),
                ('max_width', ctypes.c_int), ('max_height', ctypes.c_int),
                ('width_inc', ctypes.c_int), ('height_inc', ctypes.c_int),
                ('min_aspect_x', ctypes.c_int), ('min_aspect_y', ctypes.c_int),
                ('max_aspect_x', ctypes.c_int), ('max_aspect_y', ctypes.c_int),
                ('base_width', ctypes.c_int), ('base_height', ctypes.c_int),
                ('win_gravity', ctypes.c_int)]


P_MIN_SIZE = 1 << 4
MOVERESIZE_WIDTH = 1 << 10
MOVERESIZE_HEIGHT = 1 << 11
MOVERESIZE_USER = 2 << 12   # the source, the user's (EWMH)

ERROR_HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)


class X:
    def __init__(self, name):
        x = ctypes.CDLL(ctypes.util.find_library('X11'))
        x.XOpenDisplay.restype = ctypes.c_void_p
        x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x.XDefaultRootWindow.restype = ctypes.c_ulong
        x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        x.XInternAtom.restype = ctypes.c_ulong
        x.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        x.XGetAtomName.restype = ctypes.c_void_p
        x.XGetAtomName.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        x.XFree.argtypes = [ctypes.c_void_p]
        x.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long,
                                 ctypes.POINTER(XEvent)]
        x.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long,
                                         ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
                                         ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
                                         ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
                                         ctypes.POINTER(ctypes.c_void_p)]
        x.XChangeProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong,
                                      ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
        x.XGetGeometry.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
                                   ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
                                   ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
        x.XFlush.argtypes = [ctypes.c_void_p]
        x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        x.XCloseDisplay.argtypes = [ctypes.c_void_p]
        x.XSetErrorHandler.argtypes = [ERROR_HANDLER]
        x.XGetWMNormalHints.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XSizeHints),
                                        ctypes.POINTER(ctypes.c_long)]
        self.x = x
        # A window gone (BadWindow) is said by its reads, not the process's end
        self.handler = ERROR_HANDLER(lambda d, e: 0)
        x.XSetErrorHandler(self.handler)
        self.d = x.XOpenDisplay(name.encode())
        if not self.d:
            sys.exit(f'maximize.py: no X display at {name}')
        self.root = x.XDefaultRootWindow(self.d)
        self.state = self.atom('_NET_WM_STATE')
        self.vert = self.atom('_NET_WM_STATE_MAXIMIZED_VERT')
        self.horz = self.atom('_NET_WM_STATE_MAXIMIZED_HORZ')
        self.moveresize = self.atom('_NET_MOVERESIZE_WINDOW')

    def atom(self, name):
        return self.x.XInternAtom(self.d, name.encode(), 0)

    def atom_name(self, atom):
        p = self.x.XGetAtomName(self.d, atom)
        if not p:
            return str(atom)
        name = ctypes.string_at(p).decode(errors='replace')
        self.x.XFree(p)
        return name

    def states(self, window):
        kind, fmt = ctypes.c_ulong(), ctypes.c_int()
        count, after, data = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_void_p()
        if self.x.XGetWindowProperty(self.d, window, self.state, 0, 64, 0, XA_ATOM, ctypes.byref(kind),
                                     ctypes.byref(fmt), ctypes.byref(count), ctypes.byref(after),
                                     ctypes.byref(data)) != 0:
            return None
        atoms = []
        if data.value:
            if fmt.value == 32:
                atoms = list(ctypes.cast(data, ctypes.POINTER(ctypes.c_ulong))[:count.value])
            self.x.XFree(data)
        return atoms

    def size(self, window):
        root, x, y = ctypes.c_ulong(), ctypes.c_int(), ctypes.c_int()
        w, h, border, depth = ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
        if not self.x.XGetGeometry(self.d, window, ctypes.byref(root), ctypes.byref(x), ctypes.byref(y),
                                   ctypes.byref(w), ctypes.byref(h), ctypes.byref(border), ctypes.byref(depth)):
            return None
        return w.value, h.value

    # The least size the window gives the window manager, or None
    def min_size(self, window):
        hints, supplied = XSizeHints(), ctypes.c_long()
        if not self.x.XGetWMNormalHints(self.d, window, ctypes.byref(hints), ctypes.byref(supplied)):
            return None
        if not hints.flags & P_MIN_SIZE:
            return None
        return hints.min_width, hints.min_height

    def resize(self, window, width, height):
        event = XEvent()
        event.xclient.type = CLIENT_MESSAGE
        event.xclient.window = window
        event.xclient.message_type = self.moveresize
        event.xclient.format = 32
        event.xclient.l[0] = MOVERESIZE_WIDTH | MOVERESIZE_HEIGHT | MOVERESIZE_USER
        event.xclient.l[3] = width
        event.xclient.l[4] = height
        self.x.XSendEvent(self.d, self.root, 0, SUBSTRUCTURE_REDIRECT | SUBSTRUCTURE_NOTIFY, ctypes.byref(event))
        self.x.XSync(self.d, 0)

    def maximize(self, window, prop):
        if prop:
            atoms = self.states(window) or []
            atoms += [a for a in (self.vert, self.horz) if a not in atoms]
            array = (ctypes.c_ulong * len(atoms))(*atoms)
            self.x.XChangeProperty(self.d, window, self.state, XA_ATOM, 32, PROP_MODE_REPLACE, array, len(atoms))
        else:
            event = XEvent()
            event.xclient.type = CLIENT_MESSAGE
            event.xclient.window = window
            event.xclient.message_type = self.state
            event.xclient.format = 32
            event.xclient.l[0] = NET_WM_STATE_ADD
            event.xclient.l[1] = self.vert
            event.xclient.l[2] = self.horz
            event.xclient.l[3] = SOURCE_USER
            self.x.XSendEvent(self.d, self.root, 0, SUBSTRUCTURE_REDIRECT | SUBSTRUCTURE_NOTIFY, ctypes.byref(event))
        self.x.XSync(self.d, 0)


def find_window(display, pattern):
    env = dict(os.environ, DISPLAY=display)
    found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', pattern],
                           env=env, capture_output=True, text=True)
    ids = found.stdout.split()
    if len(ids) > 1:
        print(f"maximize.py: {len(ids)} windows named {pattern}: {' '.join(ids)}; taking {ids[-1]}", flush=True)
    return int(ids[-1]) if ids else None


def main():
    args = sys.argv[1:]
    asked = None
    if '--size' in args:
        i = args.index('--size')
        try:
            asked = tuple(int(v) for v in args[i + 1].split('x'))
        except (IndexError, ValueError):
            sys.exit(__doc__)
        del args[i:i + 2]
    words = [a for a in args if a != '--prop']
    prop = '--prop' in args
    if len(words) < 2:
        sys.exit(__doc__)
    display, pattern = words[0], words[1]
    delay = float(words[2]) if len(words) > 2 else 15.0
    watch = float(words[3]) if len(words) > 3 else 12.0

    time.sleep(delay)
    window = find_window(display, pattern)
    if window is None:
        sys.exit(f'maximize.py: no window named {pattern} on {display}')
    x = X(display)

    def say(when):
        states = x.states(window)
        size = x.size(window)
        names = ' '.join(x.atom_name(a).replace('_NET_WM_STATE_', '') for a in states) if states is not None else '?'
        shown = f'{size[0]}x{size[1]}' if size else '?'
        least = x.min_size(window)
        hint = f' least {least[0]}x{least[1]}' if least else ' least none'
        print(f'maximize.py: {when} window {window:#x} state [{names}] size {shown}{hint}', flush=True)

    say('before')
    if asked:
        print(f'maximize.py: {asked[0]}x{asked[1]} asked of the window manager', flush=True)
        x.resize(window, asked[0], asked[1])
    else:
        print(f"maximize.py: maximize asked ({'its _NET_WM_STATE set' if prop else 'of the window manager'})", flush=True)
        x.maximize(window, prop)
    start = time.time()
    while time.time() - start < watch:
        time.sleep(0.5)
        say(f't={time.time() - start:.1f}')
    x.x.XCloseDisplay(x.d)


if __name__ == '__main__':
    main()
