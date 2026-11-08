#!/usr/bin/env python3
"""The pixels each of MaskEdgeConsole's masked tiles covers in each run's
shots: those of the tile's box that are not the magenta behind it
(bugs.md REN.52). D3DDrv keeps a texel whose filtered alpha is above
127/255, as DXEngine's devices do: each tile covers the pixels it does in the
original's shots; a test at 0.5 left the smooth tiles (A, B) short, the
point-sampled one (C) alike.

    maskcount.py <run folder> [<run folder> ...]

A run folder is build/dxcap/runs/<...>: the original's shots ShotNNNN.png,
DXEngine's ShotNNNN.bmp, at DXCAP_WINDOW=1280x720. Needs ImageMagick
(magick)."""
import os
import re
import subprocess
import sys

BOXES = {'A (16x16 texels, smooth, x20)': (40, 60, 320, 320),
         'B (64x64 texels, smooth, x5)': (380, 60, 320, 320),
         'C (16x16 texels, point, x20)': (720, 60, 320, 320)}


def covered(path, box):
    x, y, w, h = box
    out = subprocess.run(['magick', path, '-crop', f'{w}x{h}+{x}+{y}', '+repage', '-depth', '8', 'txt:-'],
                         check=True, capture_output=True, text=True).stdout
    n = 0
    for line in out.splitlines():
        m = re.match(r'\d+,\d+: \((\d+),(\d+),(\d+)', line)
        if not m:
            continue
        r, g, b = int(m[1]), int(m[2]), int(m[3])
        # The magenta box, through either engine's gamma, stays (255, 0, 255)
        if not (r >= 250 and g <= 5 and b >= 250):
            n += 1
    return n


for run in sys.argv[1:]:
    shots = sorted(f for f in os.listdir(run) if re.match(r'Shot\d{4}\.(png|bmp)$', f))
    for shot in shots:
        counts = ', '.join(f'{name}: {covered(os.path.join(run, shot), box)}' for name, box in BOXES.items())
        print(f'{os.path.basename(run.rstrip("/"))} {shot}: {counts}')
