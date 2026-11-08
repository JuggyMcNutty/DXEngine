#!/usr/bin/env python3
"""The bright end of each of a run's shots (bugs.md REN.57): with Bloom on,
the world's glow belongs under the screen flash, which the loading and
saving screens' frame dims to a tenth (Engine.dll PaintProgress), so the
dimmed view's brightest pixels stay as dark as with Bloom off.

    bloomlevels.py <run folder> [<run folder> ...] [--over 40] [--region x,y,w,h]

For each shot (the original's ShotNNNN.png, DXEngine's ShotNNNN.bmp), over
the region (the whole frame by default): the 99.9th percentile of the
pixels' grey (0.299 R + 0.587 G + 0.114 B, 0 to 255), and how many pixels'
grey is over --over. Needs ImageMagick (magick)."""
import argparse
import os
import re
import subprocess


def greys(path, region):
    cmd = ['magick', path]
    if region:
        x, y, w, h = region
        cmd += ['-crop', f'{w}x{h}+{x}+{y}', '+repage']
    cmd += ['-depth', '8', 'rgb:-']
    data = subprocess.run(cmd, check=True, capture_output=True).stdout
    return [0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2] for i in range(0, len(data) - 2, 3)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--over', type=float, default=40.0)
    ap.add_argument('--region')
    args = ap.parse_args()
    region = tuple(int(v) for v in args.region.split(',')) if args.region else None
    for run in args.runs:
        shots = sorted(f for f in os.listdir(run) if re.match(r'Shot\d{4}\.(png|bmp)$', f))
        for shot in shots:
            g = sorted(greys(os.path.join(run, shot), region))
            if not g:
                continue
            p999 = g[min(len(g) - 1, int(len(g) * 0.999))]
            over = sum(1 for v in g if v > args.over)
            print(f'{os.path.basename(run.rstrip("/"))} {shot}: 99.9th percentile {p999:.0f}, {over} pixels over {args.over:g}')


if __name__ == '__main__':
    main()
