#!/usr/bin/env python3
"""A keyboard made by uinput (/dev/uinput, which the user must be able to
write), for TypeConsole in the engine's own Wayland window with an input
method (bugs.md WIN.35): it waits for the newest TypeConsole run under <runs>
(the harness's build/dxcap/runs) begun after it, and for that run's
"DXTYPE: an edit field" line, then a second; with --toggle it taps
Ctrl+Space, an input method's usual switch (fcitx5's), and waits half a
second; then it types <text> (letters, digits and spaces; "nihao " by
default) a key a fifth of a second apart, each held 0.05 s; then the device
goes. The game's window must have the keyboard: the compositor gives it the
keys, through the input method while its text input is on. No focus guard:
a Wayland window is not X's to name.

  imtype.py [--toggle] <runs> [<text>]"""
import fcntl, glob, os, struct, sys, time

EV_SYN, EV_KEY = 0, 1
SYN_REPORT = 0
KEY_LEFTCTRL, KEY_SPACE = 29, 57
CODES = dict(zip('qwertyuiop', range(16, 26)))
CODES.update(zip('asdfghjkl', range(30, 39)))
CODES.update(zip('zxcvbnm', range(44, 51)))
CODES.update(zip('1234567890', range(2, 12)))
CODES[' '] = KEY_SPACE
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

def key(fd, code, down):
    os.write(fd, EVENT.pack(0, 0, EV_KEY, code, 1 if down else 0))
    os.write(fd, EVENT.pack(0, 0, EV_SYN, SYN_REPORT, 0))

def tap(fd, code):
    key(fd, code, True)
    time.sleep(0.05)
    key(fd, code, False)

def wait_for_field(runs, started, limit=120):
    # The newest TypeConsole run made since the script began, and its line
    end = time.time() + limit
    while time.time() < end:
        dirs = [d for d in glob.glob(os.path.join(runs, 'dxe-TypeConsole-*')) if os.path.getmtime(d) >= started]
        if dirs:
            log = os.path.join(max(dirs, key=os.path.getmtime), 'engine.log')
            try:
                with open(log, 'rb') as f:
                    if b'DXTYPE: an edit field' in f.read():
                        return log
            except OSError:
                pass
        time.sleep(0.2)
    return None

def main():
    args = sys.argv[1:]
    toggle = '--toggle' in args
    args = [a for a in args if a != '--toggle']
    if not args:
        print(__doc__)
        sys.exit(2)
    runs = args[0]
    text = args[1] if len(args) > 1 else 'nihao '
    bad = [c for c in text if c not in CODES]
    if bad:
        sys.exit('imtype: no key for %r' % ''.join(bad))
    started = time.time()
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    for code in sorted(set(CODES.values()) | {KEY_LEFTCTRL}):
        fcntl.ioctl(fd, UI_SET_KEYBIT, code)
    fcntl.ioctl(fd, UI_DEV_SETUP, SETUP.pack(BUS_USB, 0x1209, 0xd0d5, 1, b'dxcap input method keys', 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    try:
        log = wait_for_field(runs, started)
        if not log:
            print('imtype: no TypeConsole edit field seen: nothing typed', flush=True)
            return
        time.sleep(1.0)
        if toggle:
            print('imtype: Ctrl+Space', flush=True)
            key(fd, KEY_LEFTCTRL, True)
            time.sleep(0.05)
            tap(fd, KEY_SPACE)
            time.sleep(0.05)
            key(fd, KEY_LEFTCTRL, False)
            time.sleep(0.5)
        print('imtype: typing %r (%s)' % (text, log), flush=True)
        for c in text:
            tap(fd, CODES[c])
            time.sleep(0.15)
        print('imtype: typed', flush=True)
        time.sleep(0.5)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)

if __name__ == '__main__':
    main()
