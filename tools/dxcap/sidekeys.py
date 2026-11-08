#!/usr/bin/env python3
"""A keyboard made by uinput (/dev/uinput, which the user must be able to
write), for SideKeyConsole (bugs.md WIN.37): it waits <delay> s
(6 by default), taps Left Shift, Right Shift, Left Ctrl, Right Ctrl, Left Alt
and Right Alt a second apart (each held 0.3 s), then at <delay> + 6 s presses
Left Shift, W, X and Space and holds them 8 s, over the console's reload of the
map, then lets them go; then the device goes. The game's window must have
the focus: the desktop gives the keys to it.

  sidekeys.py [<delay>]"""
import fcntl, os, struct, subprocess, sys, time

EV_SYN, EV_KEY = 0, 1
SYN_REPORT = 0
KEY_W, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_X, KEY_RIGHTSHIFT = 17, 29, 42, 45, 54
KEY_LEFTALT, KEY_SPACE, KEY_RIGHTCTRL, KEY_RIGHTALT = 56, 57, 97, 100
KEYS = (KEY_W, KEY_X, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTALT, KEY_SPACE, KEY_RIGHTCTRL, KEY_RIGHTALT)
NAMES = {KEY_LEFTSHIFT: 'Left Shift', KEY_RIGHTSHIFT: 'Right Shift', KEY_LEFTCTRL: 'Left Ctrl',
         KEY_RIGHTCTRL: 'Right Ctrl', KEY_LEFTALT: 'Left Alt', KEY_RIGHTALT: 'Right Alt',
         KEY_W: 'W', KEY_X: 'X', KEY_SPACE: 'Space'}
BUS_USB = 3

def _IOC(d, t, nr, size):
    return (d << 30) | (size << 16) | (ord(t) << 8) | nr

UI_SET_EVBIT = _IOC(1, 'U', 100, 4)
UI_SET_KEYBIT = _IOC(1, 'U', 101, 4)
SETUP = struct.Struct('HHHH80sI')
UI_DEV_SETUP = _IOC(1, 'U', 3, SETUP.size)
UI_DEV_CREATE = _IOC(0, 'U', 1, 0)
UI_DEV_DESTROY = _IOC(0, 'U', 2, 0)
EVENT = struct.Struct('llHHi')

def focused():
    # The game's window alone: either engine's title starts with "Deus Ex";
    # a Wayland window (a terminal) has the focus when X names none.
    try:
        name = subprocess.run(['xdotool', 'getactivewindow', 'getwindowname'], capture_output=True,
                              text=True, timeout=2, env=dict(os.environ, DISPLAY=os.environ.get('DISPLAY', ':0'))).stdout.strip()
    except Exception:
        return False
    return name.startswith('Deus Ex')

def guard(fd, held):
    if focused():
        return True
    for k in held:
        key(fd, k, False)
    print('sidekeys: the game has not the focus: stopped', flush=True)
    return False

def key(fd, code, down):
    os.write(fd, EVENT.pack(0, 0, EV_KEY, code, 1 if down else 0))
    os.write(fd, EVENT.pack(0, 0, EV_SYN, SYN_REPORT, 0))

def main():
    delay = float(sys.argv[1]) if len(sys.argv) > 1 else 6.0
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    for k in KEYS:
        fcntl.ioctl(fd, UI_SET_KEYBIT, k)
    fcntl.ioctl(fd, UI_DEV_SETUP, SETUP.pack(BUS_USB, 0x1209, 0xd0d4, 1, b'dxcap side keys', 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    try:
        time.sleep(delay)
        for k in (KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTALT, KEY_RIGHTALT):
            if not guard(fd, ()):
                return
            print('sidekeys: tap %s' % NAMES[k], flush=True)
            key(fd, k, True)
            time.sleep(0.3)
            key(fd, k, False)
            time.sleep(0.7)
        if not guard(fd, ()):
            return
        print('sidekeys: hold Left Shift, W, X and Space', flush=True)
        held = (KEY_LEFTSHIFT, KEY_W, KEY_X, KEY_SPACE)
        for k in held:
            key(fd, k, True)
        for i in range(16):
            time.sleep(0.5)
            if not guard(fd, held):
                return
        for k in (KEY_SPACE, KEY_X, KEY_W, KEY_LEFTSHIFT):
            key(fd, k, False)
        print('sidekeys: released', flush=True)
        time.sleep(0.5)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)

if __name__ == '__main__':
    main()
