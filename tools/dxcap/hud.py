#!/usr/bin/env python3
"""Where the HUD's own pixels lie in runs' shots (docs/DEVELOPMENT.md, shots):
in each run, a shot with the HUD less the shot without it, over a region, the
pixels that changed by more than a level; printed for each run, the columns
and the rows they cover, as runs of numbers in the frame's pixels. The scene
behind is the same in both shots, so only the HUD shows. DXEngine's shots
first go through the gamma the original's frames carry, as shots.py's do, so
that a shadow over the scene changes it alike.
Each run after the first is then set against it: the pixels in one run's HUD
and not the other's, as they lie and at the move of up to 8 pixels each way
that fits best, the move given as the later run's pixels to the first's.
CrosshairConsole's shots: 0 and 2 with the HUD, 1 without.

  hud.py <run> [<run> ...] --region x,y,w,h [--with 0] [--without 1]
         [--level 24] [--gamma 1.5]

A run is its folder under build/dxcap/runs: the original's shots are the
grabber's Shot0000.png..., DXEngine's Shot0000.bmp.... Needs ImageMagick
(magick)."""

import argparse, array, os, subprocess, sys

def greys(path, region, gamma):
    # The region's grey levels, 0 to 255
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    cmd += ['-colorspace', 'Gray', '-crop', '%dx%d+%d+%d' % (region[2], region[3], region[0], region[1]),
            '+repage', '-depth', '8', 'gray:-']
    g = array.array('B')
    g.frombytes(subprocess.run(cmd, check=True, capture_output=True).stdout)
    return g

def shot(run, n):
    for ext in ('png', 'bmp'):
        path = os.path.join(run, 'Shot%04d.%s' % (n, ext))
        if os.path.exists(path):
            return path
    return None

def spans(numbers):
    # 3 4 5 9 as "3-5 9"
    out = []
    for n in sorted(numbers):
        if out and n == out[-1][1] + 1:
            out[-1][1] = n
        else:
            out.append([n, n])
    return ' '.join('%d-%d' % (a, b) if a != b else '%d' % a for a, b in out)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--region', required=True, help='x,y,w,h in pixels')
    ap.add_argument('--with', dest='with_', type=int, default=0, help='the shot with the HUD (0)')
    ap.add_argument('--without', type=int, default=1, help='the shot without it (1)')
    ap.add_argument('--level', type=int, default=24, help='the change in grey levels of 255 that counts (24)')
    ap.add_argument('--gamma', type=float, default=1.5, help="the gamma put on DXEngine's shots (1.5)")
    a = ap.parse_args()
    region = tuple(int(v) for v in a.region.split(','))
    w = region[2]
    masks = []
    for run in a.runs:
        on, off = shot(run, a.with_), shot(run, a.without)
        if not on or not off:
            print('%s: no shot %d or %d' % (run, a.with_, a.without))
            continue
        gamma = a.gamma if on.endswith('.bmp') else 1.0
        g1, g0 = greys(on, region, gamma), greys(off, region, gamma)
        mask = set((region[0] + i % w, region[1] + i // w) for i in range(len(g1)) if abs(g1[i] - g0[i]) > a.level)
        print('%s: shot %d less %d, %d pixels' % (os.path.basename(run.rstrip('/')), a.with_, a.without, len(mask)))
        print('  columns ' + (spans(set(x for x, y in mask)) or 'none'))
        print('  rows    ' + (spans(set(y for x, y in mask)) or 'none'))
        masks.append((os.path.basename(run.rstrip('/')), mask))
    for name, mask in masks[1:]:
        first = masks[0][1]
        best = min(((len(first ^ set((x + dx, y + dy) for x, y in mask)), dx, dy)
                    for dx in range(-8, 9) for dy in range(-8, 9)), key=lambda t: (t[0], abs(t[1]) + abs(t[2])))
        print('%s against %s: %d pixels in one and not the other as they lie; best moved %+d,%+d: %d'
              % (name, masks[0][0], len(first ^ mask), best[1], best[2], best[0]))

if __name__ == '__main__':
    main()
