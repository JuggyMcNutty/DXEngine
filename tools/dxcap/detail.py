#!/usr/bin/env python3
"""Reads a DetailConsole run: are the detail texture's bands D3DDrv's?

    detail.py <dxe run dir> [--region x,y,w,h]

DetailConsole looks straight down at the pier's planks from a list of
heights, a shot with the detail textures on and one with them off at each;
its DXDETAIL lines name each shot's height and setting, in the order of the
run's Shot*.bmp files. Over the region (by default the frame's upper middle,
the pier's planks alone at every height: the police boat beside it, nearer
the eye, has detail textures of its own), each height's ratio of the two
shots' sums, on over off, is laid beside the bands' (dx-reverse-info
d3ddrv-dll.md, detail textures): for each band k of 0..2, bound
380 / 4.223^k, where the eye is nearer than the bound,
a = min((bound / z - 1) * 100 / 255, 1) and the band multiplies the frame by
1 + a * (2 * d - 1) on average, d the mean of the detail texture it samples.
The first band alone draws between 90 and 380 units: there that is exact,
d fitted to those heights. Nearer, each band sees a part of a tile, whose
own mean the others' need not share: those heights are shown beside the
first band's d, not checked. Checked:

- between 90 and 380, each ratio within 0.002 of the first band's;
- at 380 and beyond, the two shots the same to the bit;
- no seam: across each bound (379 and 381, 89 and 91, 21 and 22) the ratio
  changes by under 0.003.

The exit code is the number of checks that failed. Needs ImageMagick
(magick).
"""
import argparse, os, re, subprocess, sys

SHOT = re.compile(r"DXDETAIL: shot (\d+) height ([0-9.]+) detail (True|False)")
BOUNDS = [380.0 / 4.223 ** k for k in range(3)]
SEAMS = [(379, 381), (89, 91), (21, 22)]


def alphas(z):
    return [min((b / z - 1.0) * 100.0 / 255.0, 1.0) if z < b else 0.0 for b in BOUNDS]


def predicted(z, c):
    r = 1.0
    for a in alphas(z):
        r *= 1.0 + a * c
    return r


def pixels(path, region):
    """The region's bytes, red, green and blue in turn."""
    x, y, w, h = region
    return subprocess.run(["magick", path, "-crop", "%dx%d+%d+%d" % (w, h, x, y), "+repage", "-depth", "8", "rgb:-"],
                          check=True, capture_output=True).stdout


def size(path):
    out = subprocess.run(["magick", "identify", "-format", "%w %h", path], check=True, capture_output=True, text=True).stdout
    w, h = out.split()
    return int(w), int(h)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run")
    ap.add_argument("--region", help="x,y,w,h in pixels (the frame's upper middle: x a quarter to three, y a 24th to a third)")
    a = ap.parse_args()
    log = os.path.join(a.run, "engine.log")
    if not os.path.exists(log):
        sys.exit(a.run + ": no engine.log")
    shots = [(int(n), float(h), on == "True") for n, h, on in SHOT.findall(open(log, errors="replace").read())]
    files = sorted(f for f in os.listdir(a.run) if re.match(r"Shot\d+\.bmp$", f))
    if not shots or len(files) != len(shots):
        sys.exit("%d shots logged, %d Shot*.bmp in the run" % (len(shots), len(files)))
    w, h = size(os.path.join(a.run, files[0]))
    region = tuple(int(v) for v in a.region.split(",")) if a.region else (w // 4, h // 24, w // 2, h // 3 - h // 24)

    heights = {}
    for (n, z, on), f in zip(shots, files):
        heights.setdefault(z, {})[on] = pixels(os.path.join(a.run, f), region)
    rows = []
    for z in sorted(heights):
        pair = heights[z]
        if True not in pair or False not in pair:
            print("height %g: not both shots" % z)
            continue
        on, off = pair[True], pair[False]
        rows.append((z, sum(on) / max(sum(off), 1), on == off))

    # The first band's mean, fitted where it draws alone: c = 2 d - 1.
    alone = [(z, r) for z, r, _ in rows if BOUNDS[1] <= z < BOUNDS[0]]
    if not alone:
        sys.exit("no height between 90 and 380: the first band's mean is not known")
    def error(c):
        return sum((r - predicted(z, c)) ** 2 for z, r in alone)
    lo, hi = -1.0, 1.0
    for _ in range(100):
        m1, m2 = lo + (hi - lo) / 3, hi - (hi - lo) / 3
        if error(m1) < error(m2):
            hi = m2
        else:
            lo = m1
    c = (lo + hi) / 2

    failed = 0
    print("%s, region %d,%d %dx%d; the first band's detail mean %.4f (2d - 1 = %+.4f)" % (os.path.basename(a.run), *region,
                                                                                       (c + 1) / 2, c))
    print("height    a(380)  a(90)  a(21.3)   on/off   bands'   off by (from 90 up: checked)")
    measured = {}
    for z, r, same in rows:
        p = predicted(z, c)
        al = alphas(z)
        measured[z] = r
        bad = BOUNDS[1] <= z and abs(r - p) > 0.002
        if all(x == 0.0 for x in al) and not same:
            bad = True
        failed += bad
        print("%6g   %7.3f %6.3f %7.3f   %7.4f  %7.4f  %+7.4f%s%s" % (
            z, al[0], al[1], al[2], r, p, r - p, "  the same to the bit" if same else "", "  FAIL" if bad else ""))
    for z1, z2 in SEAMS:
        if z1 in measured and z2 in measured:
            step = abs(measured[z1] - measured[z2])
            bad = step >= 0.003
            failed += bad
            print("seam %g/%g: %.4f%s" % (z1, z2, step, "  FAIL" if bad else ""))
    print("%d failed" % failed)
    sys.exit(failed)


if __name__ == "__main__":
    main()
