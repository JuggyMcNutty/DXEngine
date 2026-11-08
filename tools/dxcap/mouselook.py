#!/usr/bin/env python3
"""A relative mouse made by uinput (/dev/uinput, which the user must be able
to write), for MouseLookConsole (bugs.md ENG.36): it waits <delay> s, then,
while the game's window has the focus, moves right in <steps> steps of
<count> counts 10 ms apart, waits 2 s, moves left as far, waits 2 s, then
moves down and up half as far; then the device goes. A slow, even move, so
that a desktop's adaptive acceleration changes the original's pointer pixels
little; the console compares the turn a unit the game received, which the
acceleration does not touch. The pointer must be over the game's window: a
run with DXCAP_FULLSCREEN=1 has it there wherever it is. It stops, saying
so, when the game's window loses the focus.

  mouselook.py [<delay> [<steps> [<count>]]]   # 10 s, 100 steps, 3 counts"""
import fcntl, os, struct, subprocess, sys, time

EV_SYN, EV_KEY, EV_REL = 0, 1, 2
SYN_REPORT = 0
BTN_LEFT, BTN_RIGHT = 0x110, 0x111
REL_X, REL_Y = 0, 1
BUS_USB = 3

def _IOC(d, t, nr, size):
    return (d << 30) | (size << 16) | (ord(t) << 8) | nr

UI_SET_EVBIT = _IOC(1, 'U', 100, 4)
UI_SET_KEYBIT = _IOC(1, 'U', 101, 4)
UI_SET_RELBIT = _IOC(1, 'U', 102, 4)
SETUP = struct.Struct('HHHH80sI')          # struct uinput_setup: input_id, name, ff_effects_max
UI_DEV_SETUP = _IOC(1, 'U', 3, SETUP.size)
UI_DEV_CREATE = _IOC(0, 'U', 1, 0)
UI_DEV_DESTROY = _IOC(0, 'U', 2, 0)
EVENT = struct.Struct('llHHi')             # struct input_event: time, type, code, value

def focused():
    # The game's window alone: either engine's title starts with "Deus Ex";
    # a Wayland window (a terminal) has the focus when X names none.
    try:
        name = subprocess.run(['xdotool', 'getactivewindow', 'getwindowname'], capture_output=True,
                              text=True, timeout=2, env=dict(os.environ, DISPLAY=os.environ.get('DISPLAY', ':0'))).stdout.strip()
    except Exception:
        return False
    return name.startswith('Deus Ex')

def emit(fd, t, code, value):
    os.write(fd, EVENT.pack(0, 0, t, code, value))

def move(fd, axis, steps, count, label):
    if not focused():
        print('mouselook: the game has not the focus: stopped', flush=True)
        return False
    print('mouselook: %s, %d steps of %d' % (label, steps, count), flush=True)
    for _ in range(steps):
        emit(fd, EV_REL, axis, count)
        emit(fd, EV_SYN, SYN_REPORT, 0)
        time.sleep(0.010)
    return True

def main():
    delay = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
    steps = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    for key in (BTN_LEFT, BTN_RIGHT):
        fcntl.ioctl(fd, UI_SET_KEYBIT, key)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_REL)
    for rel in (REL_X, REL_Y):
        fcntl.ioctl(fd, UI_SET_RELBIT, rel)
    fcntl.ioctl(fd, UI_DEV_SETUP, SETUP.pack(BUS_USB, 0x1209, 0xd0d3, 1, b'dxcap mouselook mouse', 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    try:
        time.sleep(delay)
        for axis, n, c, label in ((REL_X, steps, count, 'right'), (REL_X, steps, -count, 'left'),
                                  (REL_Y, steps // 2, count, 'down'), (REL_Y, steps // 2, -count, 'up')):
            if not move(fd, axis, n, c, label):
                return
            time.sleep(2)
        print('mouselook: done', flush=True)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)

if __name__ == '__main__':
    main()
