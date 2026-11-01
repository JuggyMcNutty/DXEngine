#!/usr/bin/env python3
"""Samples a window's marked frames one by one, and reads what it sampled.

LightConsole's Burst marks frames in a row, each with its number's low 8 bits
in CaptureConsole's mark -- a magenta block, then eight black or white blocks
of 12 pixels. grab.py, five grabs a second, keeps one frame in a dozen at 60
frames; this reads the mark and a square at the view's middle some hundreds
of times a second (libX11's XGetImage, through ctypes), the mark before and
after the square, and keeps a sample only where both carry one number: the
X server takes each request whole, so the square is that frame's. Each
number is written to the csv when it first shows: the seconds since the
start, the number, and the square's mean red, green and blue.

    burst.py grab <out csv> <display> <window name pattern> <view width> <view height> [<square>]
    burst.py report <csv> [<log>] [--threshold <level>]

grab finds the window as grab.py does (xdotool's search, the newest of the
name), waits for the mark, and samples until killed, or 2 s after the mark
has gone. report unwraps the numbers into frames and says how many were
missed; puts each frame lit or out by its square's brightness, halfway
between the darkest and the brightest frame (or at --threshold): the out
frames' share, the runs -- how many frames a state holds, between two
changes seen with no frame missed --, how often a frame's state differs from
the one before, and how often its brightness does (a flicker's fresh draw);
and with the run's log, LightConsole's "DXLIGHT: frame" lines, how often the
state seen is the blink bit logged for the frame, or for it or the one
before (a state held two frames).
"""
import ctypes
import ctypes.util
import os
import re
import signal
import subprocess
import sys
import time

BLOCK = 12


class XImage(ctypes.Structure):
    _fields_ = [('width', ctypes.c_int), ('height', ctypes.c_int), ('xoffset', ctypes.c_int),
                ('format', ctypes.c_int), ('data', ctypes.c_void_p), ('byte_order', ctypes.c_int),
                ('bitmap_unit', ctypes.c_int), ('bitmap_bit_order', ctypes.c_int),
                ('bitmap_pad', ctypes.c_int), ('depth', ctypes.c_int),
                ('bytes_per_line', ctypes.c_int), ('bits_per_pixel', ctypes.c_int),
                ('red_mask', ctypes.c_ulong), ('green_mask', ctypes.c_ulong),
                ('blue_mask', ctypes.c_ulong), ('obdata', ctypes.c_void_p),
                ('f', ctypes.c_void_p * 6)]


ERROR_HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
DESTROY_IMAGE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(XImage))
ALL_PLANES = ctypes.c_ulong(-1).value
Z_PIXMAP = 2


class Display:
    def __init__(self, name):
        self.x = ctypes.CDLL(ctypes.util.find_library('X11'))
        self.x.XOpenDisplay.restype = ctypes.c_void_p
        self.x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x.XGetImage.restype = ctypes.POINTER(XImage)
        self.x.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                                     ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
        self.x.XSetErrorHandler.restype = ctypes.c_void_p
        self.x.XSetErrorHandler.argtypes = [ERROR_HANDLER]
        # A window gone, or a rectangle off it, is an X error, on which
        # Xlib's own handler exits: here a grab that gives nothing.
        self.handler = ERROR_HANDLER(lambda display, event: 0)
        self.x.XSetErrorHandler(self.handler)
        self.d = self.x.XOpenDisplay(name.encode())
        if not self.d:
            sys.exit(f'burst.py: cannot open display {name}')

    def pixels(self, window, x, y, w, h):
        """The rectangle's pixels, row by row, as (r, g, b); None for none.
        A 32-bit image in the X server's order, which is this machine's."""
        img = self.x.XGetImage(self.d, window, x, y, w, h, ALL_PLANES, Z_PIXMAP)
        if not img:
            return None
        im = img.contents
        try:
            if im.bits_per_pixel != 32 or not im.data:
                return None
            words = memoryview(ctypes.string_at(im.data, im.bytes_per_line * im.height)).cast('I')
            stride = im.bytes_per_line // 4
            masks = (im.red_mask, im.green_mask, im.blue_mask)
            shifts = [(m & -m).bit_length() - 1 for m in masks]
            out = []
            for r in range(im.height):
                for p in words[r * stride:r * stride + im.width]:
                    out.append(((p & masks[0]) >> shifts[0], (p & masks[1]) >> shifts[1], (p & masks[2]) >> shifts[2]))
            return out
        finally:
            DESTROY_IMAGE(im.f[1])(img)


def is_magenta(p):
    return p[0] > 200 and p[1] < 60 and p[2] > 200


def find_window(env, name):
    found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', name],
                           env=env, capture_output=True, text=True)
    ids = found.stdout.split()
    # The newest, X's ids rising, as grab.py takes it.
    return int(ids[-1]) if ids else None


def find_mark(dpy, window):
    """The mark's corner in the window's top-left area, as grab.py finds it."""
    width, height = 200, 120
    pixels = dpy.pixels(window, 0, 0, width, height)
    if not pixels:
        return None
    for y in range(height):
        for x in range(width):
            if is_magenta(pixels[y * width + x]):
                # Its full block, not a stray pixel.
                if y + BLOCK - 1 < height and x + BLOCK - 1 < width and is_magenta(pixels[(y + BLOCK - 1) * width + x + BLOCK - 1]):
                    return x, y
                return None
    return None


def read_number(dpy, window, x, y):
    """The mark's number, or None where there is no mark: a row through the
    blocks' middles."""
    row = dpy.pixels(window, x, y + BLOCK // 2, 9 * BLOCK, 1)
    if not row or not is_magenta(row[BLOCK // 2]):
        return None
    number = 0
    for bit in range(8):
        if sum(row[BLOCK + bit * BLOCK + BLOCK // 2]) > 3 * 200:
            number |= 1 << bit
    return number


def grab(out, display, name, vw, vh, square):
    # Killed, it exits through the with, its file closed.
    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(0))
    env = dict(os.environ, DISPLAY=display)
    env.pop('WAYLAND_DISPLAY', None)
    dpy = Display(display)
    window = mark = None
    while mark is None:
        window = find_window(env, name)
        mark = find_mark(dpy, window) if window else None
        if mark is None:
            time.sleep(0.05)
    x, y = mark
    sx, sy = x + vw // 2 - square // 2, y + vh // 2 - square // 2
    print(f'burst.py: window {window:#x}, mark at {x},{y}, a square of {square} at {sx},{sy}', file=sys.stderr)
    start = last_seen = time.monotonic()
    last = None
    with open(out, 'w') as f:
        f.write('seconds,number,red,green,blue\n')
        while True:
            first = read_number(dpy, window, x, y)
            pixels = dpy.pixels(window, sx, sy, square, square)
            second = read_number(dpy, window, x, y)
            now = time.monotonic()
            if first is None or second is None:
                if last is not None and now - last_seen > 2.0:
                    break
            elif first == second and pixels:
                last_seen = now
                if first != last:
                    n = len(pixels)
                    r = sum(p[0] for p in pixels) / n
                    g = sum(p[1] for p in pixels) / n
                    b = sum(p[2] for p in pixels) / n
                    f.write(f'{now - start:.4f},{first},{r:.2f},{g:.2f},{b:.2f}\n')
                    f.flush()
                    last = first
            time.sleep(0.0005)


def read_csv(path):
    samples = []
    with open(path) as f:
        next(f)
        for line in f:
            t, n, r, g, b = line.strip().split(',')
            samples.append((float(t), int(n), 0.299 * float(r) + 0.587 * float(g) + 0.114 * float(b)))
    return samples


def read_bits(path):
    """LightConsole's blink bit by frame, from a run's log."""
    bits = {}
    line_re = re.compile(r'DXLIGHT: frame (\d+) time (\S+) blink (\d)')
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = line_re.search(line)
            if m:
                bits[int(m.group(1))] = int(m.group(3)) == 1
    return bits


def report(path, log, threshold):
    samples = read_csv(path)
    if not samples:
        sys.exit(f'{path}: no samples')
    # The numbers unwrapped: each sample a frame after the one before, by
    # the 8 bits' difference; the first the burst's frame of its number, the
    # grabber started before the burst.
    frame = previous = samples[0][1]
    frames = {frame: samples[0][2]}
    for _, n, level in samples[1:]:
        frame += (n - previous) % 256
        previous = n
        frames[frame] = level
    first, last = min(frames), max(frames)
    print(f'frames {first} to {last}: {len(frames)} seen, {last - first + 1 - len(frames)} missed')
    levels = sorted(frames.values())
    if threshold is None:
        threshold = (levels[0] + levels[-1]) / 2
    print(f'brightness {levels[0]:.1f} to {levels[-1]:.1f}, out under {threshold:.1f}')
    out = {k: v < threshold for k, v in frames.items()}
    print(f'out: {sum(out.values())} of {len(out)} ({sum(out.values()) / len(out):.3f})')
    pairs = [(k, k + 1) for k in frames if k + 1 in frames]
    changed = sum(out[a] != out[b] for a, b in pairs)
    print(f'a state other than the frame before\'s: {changed} of {len(pairs)} ({changed / max(len(pairs), 1):.3f})')
    span = max(levels[-1] - levels[0], 1.0)
    moved = sum(abs(frames[a] - frames[b]) > 0.02 * span for a, b in pairs)
    print(f'a brightness other than the frame before\'s, by 2% of the range: {moved} of {len(pairs)} ({moved / max(len(pairs), 1):.3f})')
    # Runs between two changes seen, no frame missed between them.
    runs = {}
    start = None
    for k in range(first + 1, last + 1):
        if k not in frames or k - 1 not in frames:
            start = None
            continue
        if out[k] != out[k - 1]:
            if start is not None:
                runs[k - start] = runs.get(k - start, 0) + 1
            start = k
    total = sum(runs.values())
    if total:
        mean = sum(length * count for length, count in runs.items()) / total
        even = sum(count for length, count in runs.items() if length % 2 == 0)
        print(f'runs: {total}, mean {mean:.2f} frames, {even} of an even length ({even / total:.3f}); by length '
              + ', '.join(f'{length}: {runs[length]}' for length in sorted(runs)))
    if log:
        bits = read_bits(log)
        own = held = counted = 0
        for k, is_out in out.items():
            if k not in bits:
                continue
            counted += 1
            own += bits[k] == is_out
            held += bits[k] == is_out or (k - 1 in bits and bits[k - 1] == is_out)
        print(f'blink bits logged for {counted} frames seen: the frame\'s own {own} ({own / max(counted, 1):.3f}), '
              f'its own or the one before\'s {held} ({held / max(counted, 1):.3f})')


def main():
    if len(sys.argv) >= 7 and sys.argv[1] == 'grab':
        square = int(sys.argv[7]) if len(sys.argv) > 7 else 32
        grab(sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6]), square)
    elif len(sys.argv) >= 3 and sys.argv[1] == 'report':
        args = sys.argv[2:]
        threshold = None
        if '--threshold' in args:
            i = args.index('--threshold')
            threshold = float(args[i + 1])
            del args[i:i + 2]
        report(args[0], args[1] if len(args) > 1 else None, threshold)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
