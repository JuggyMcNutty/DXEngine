#!/usr/bin/env python3
"""Reads a pair of LaserConsole runs: do the beams draw as D3DDrv's?

    laser.py <original run> <dxe run> [--shot N] [--gamma 1.5] [--rows]

LaserConsole's shots 3 and 4 have a still texture in every beam's slots,
unlit and lit, so that a beam draws alike in both engines where the fire's
sparks differ from run to run. A beam is a run of rows whose median red over
the frame's width stands 16 levels or more over the rows round it (the
median of the 25 about it), found in the original's shot: a still beam draws
two lines a segment, and the median passes over the gaps between segments
and over the one segment drawn twice (dx-reverse-info deusex-dll.md,
particles and lasers). For each beam it gives the rows each engine covers
and DXEngine's brightest row's red over the original's; with --rows, each
row's red. DXEngine's shot first goes through the gamma the original's
frames carry (1.5 at the runs' Brightness), as in shots.py. Both runs at
DXCAP_WINDOW=1024x768, with NoDetail=True. Needs ImageMagick (magick)."""
import argparse, os, subprocess, sys

def medians(path, gamma):
    # Each row's median red, 0 to 255, and the shot's size
    size = subprocess.run(['magick', 'identify', '-format', '%w %h', path], check=True, capture_output=True, text=True).stdout.split()
    w, h = int(size[0]), int(size[1])
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    cmd += ['-depth', '8', 'rgb:-']
    rgb = subprocess.run(cmd, check=True, capture_output=True).stdout
    return [sorted(rgb[y * w * 3:(y + 1) * w * 3:3])[w // 2] for y in range(h)], (w, h)

def over(m, y):
    # How far a row's red stands over the rows round it
    near = sorted(m[max(0, y - 12):y + 13])
    return m[y] - near[len(near) // 2]

def spans(rows):
    # 75, 77-82
    out = []
    for y in rows:
        if out and y == out[-1][1] + 1:
            out[-1][1] = y
        else:
            out.append([y, y])
    return ', '.join(str(a) if a == b else '%d-%d' % (a, b) for a, b in out) or 'none'

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('original')
    ap.add_argument('dxe')
    ap.add_argument('--shot', type=int, help='one shot number, else 3 and 4')
    ap.add_argument('--gamma', type=float, default=1.5, help="the gamma put on DXEngine's shot (1.5)")
    ap.add_argument('--rows', action='store_true', help="print each row's red")
    a = ap.parse_args()
    for n in [a.shot] if a.shot is not None else [3, 4]:
        o = os.path.join(a.original, 'Shot%04d.png' % n)
        d = os.path.join(a.dxe, 'Shot%04d.bmp' % n)
        if not os.path.exists(o) or not os.path.exists(d):
            print('shot %d: not in both runs' % n)
            continue
        om, osize = medians(o, 1.0)
        dm, dsize = medians(d, a.gamma)
        if osize != dsize:
            sys.exit('shot %d: %dx%d against %dx%d' % (n, dsize[0], dsize[1], osize[0], osize[1]))
        beams = []
        for y in range(len(om)):
            if over(om, y) >= 16:
                if beams and y - beams[-1][1] <= 3:
                    beams[-1][1] = y
                else:
                    beams.append([y, y])
        if not beams:
            print('shot %d: no beam in the original' % n)
        for y0, y1 in beams:
            near = range(max(0, y0 - 2), min(len(om), y1 + 3))
            ob, db = max(om[y] for y in near), max(dm[y] for y in near)
            print("shot %d: the beam at %d-%d: DXEngine's rows %s, the original's %s; the brightest row's red %d against %d, %.3f"
                  % (n, y0, y1, spans([y for y in near if over(dm, y) >= 16]), spans([y for y in near if over(om, y) >= 16]), db, ob, db / ob))
            if a.rows:
                for y in near:
                    print('  row %d: %d against %d' % (y, dm[y], om[y]))

if __name__ == '__main__':
    main()
