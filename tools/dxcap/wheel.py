#!/usr/bin/env python3
"""A mouse with a high-resolution wheel, made by uinput (/dev/uinput, which
the user must be able to write), for WheelConsole (docs/DEVELOPMENT.md): it
waits <delay> s, then turns one notch down as eight REL_WHEEL_HI_RES of 15
(the last with its REL_WHEEL, as such a mouse sends them), 2 s on one notch up
so, 2 s on a classic notch down (REL_WHEEL and its 120 at once); then the
device goes. The engine's window should press each notch once.

  wheel.py [<delay>]      # 10 s by default, beside a dxe run"""
import fcntl, os, struct, sys, time

EV_SYN, EV_KEY, EV_REL = 0, 1, 2
SYN_REPORT = 0
BTN_LEFT, BTN_RIGHT = 0x110, 0x111
REL_X, REL_Y, REL_WHEEL, REL_WHEEL_HI_RES = 0, 1, 8, 11
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

def emit(fd, t, code, value):
    os.write(fd, EVENT.pack(0, 0, t, code, value))

def notch(fd, direction):
    for i in range(8):
        emit(fd, EV_REL, REL_WHEEL_HI_RES, 15 * direction)
        if i == 7:
            emit(fd, EV_REL, REL_WHEEL, direction)
        emit(fd, EV_SYN, SYN_REPORT, 0)
        time.sleep(0.012)

def main():
    delay = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    for key in (BTN_LEFT, BTN_RIGHT):
        fcntl.ioctl(fd, UI_SET_KEYBIT, key)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_REL)
    for rel in (REL_X, REL_Y, REL_WHEEL, REL_WHEEL_HI_RES):
        fcntl.ioctl(fd, UI_SET_RELBIT, rel)
    fcntl.ioctl(fd, UI_DEV_SETUP, SETUP.pack(BUS_USB, 0x1209, 0xd0d2, 1, b'dxcap hi-res wheel', 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    try:
        time.sleep(delay)
        print('wheel: a high-resolution notch down', flush=True)
        notch(fd, -1)
        time.sleep(2)
        print('wheel: a high-resolution notch up', flush=True)
        notch(fd, 1)
        time.sleep(2)
        print('wheel: a classic notch down', flush=True)
        emit(fd, EV_REL, REL_WHEEL_HI_RES, -120)
        emit(fd, EV_REL, REL_WHEEL, -1)
        emit(fd, EV_SYN, SYN_REPORT, 0)
        time.sleep(1)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)

if __name__ == '__main__':
    main()
