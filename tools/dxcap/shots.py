#!/usr/bin/env python3
"""The brightness of two runs' shots, block by block: DXEngine's against the
original's (docs/DEVELOPMENT.md, shots). Each shot is cut into blocks
(16x16 pixels by default) and each block's mean brightness taken; DXEngine's
shot first goes through the gamma the original's frames carry (1.5 at the runs'
Brightness). Prints, for each pair of shots, the mean ratio over the region
asked for (the whole frame by default), the ratios' spread, and with --blocks
the grid of ratios. With --rmse, the RMSE of the two shots' grey levels over
the region, pixel by pixel, and their correlation; with --shift, also the move
of DXEngine's shot, in quarter pixels up to one each way (bilinear), that fits
the original's best: none when the two register (a move of half a pixel blurs
too, so a flat minimum over moves the same each way is the blur's, not a
misregistration). With --levels, pixel by pixel, the largest difference of a
channel in levels of 255, and how many pixels differ by more than one.

  shots.py <original run> <dxe run> [--gamma 1.5] [--block 16]
           [--region x,y,w,h] [--shot N] [--blocks] [--rmse] [--shift]
           [--levels]

A run is its folder under build/dxcap/runs: the original's shots are the
grabber's Shot0000.png..., DXEngine's Shot0000.bmp...; a pair is the two of one
number (--shot picks one), the original's missing ones passed over. The first
run may be a dxe run too, another device's or build's: its shots are then
DXEngine's, and neither takes the gamma. Needs ImageMagick (magick)."""
import argparse, array, os, re, subprocess, sys

def means(path, gamma, block):
    # One pixel per block: ImageMagick's scale averages each block
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    cmd += ['-colorspace', 'Gray', '-define', 'filter:blur=1', '-scale', f'{100.0 / block}%', '-depth', '16', 'txt:-']
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    grid = {}
    w = h = 0
    for line in out.splitlines():
        m = re.match(r'(\d+),(\d+): \((\d+)', line)
        if not m:
            continue
        x, y, v = int(m[1]), int(m[2]), int(m[3])
        grid[(x, y)] = v / 65535.0
        w, h = max(w, x + 1), max(h, y + 1)
    return grid, w, h

def greys(path, gamma, region, move=None):
    # The region's grey levels, 0 to 65535, the shot first moved by (dx, dy)
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    if move:
        cmd += ['-virtual-pixel', 'edge', '-interpolate', 'bilinear', '-filter', 'triangle', '-distort', 'SRT', '0,0 1 0 %g,%g' % move]
    cmd += ['-colorspace', 'Gray']
    if region:
        cmd += ['-crop', '%dx%d+%d+%d' % (region[2], region[3], region[0], region[1]), '+repage']
    cmd += ['-depth', '16', 'gray:-']
    g = array.array('H')
    g.frombytes(subprocess.run(cmd, check=True, capture_output=True).stdout)
    return g

def channels(path, gamma, region):
    # The region's channels, 0 to 255, red, green and blue a pixel
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    if region:
        cmd += ['-crop', '%dx%d+%d+%d' % (region[2], region[3], region[0], region[1]), '+repage']
    cmd += ['-depth', '8', 'rgb:-']
    return subprocess.run(cmd, check=True, capture_output=True).stdout

def levels(a, b):
    # The largest difference of a channel, and the pixels more than one apart
    worst = over = 0
    for i in range(0, len(a), 3):
        d = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
        worst = max(worst, d)
        over += d > 1
    return worst, over, len(a) // 3

def rmse(a, b):
    # In levels of 255
    return (sum((p - q) ** 2 for p, q in zip(a, b)) / len(a)) ** 0.5 / 257

def correlation(a, b):
    n = len(a)
    ma, mb = sum(a) / n, sum(b) / n
    va = sum((p - ma) ** 2 for p in a)
    vb = sum((q - mb) ** 2 for q in b)
    if not va or not vb:
        return float('nan')
    return sum((p - ma) * (q - mb) for p, q in zip(a, b)) / (va * vb) ** 0.5

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('original')
    ap.add_argument('dxe')
    ap.add_argument('--gamma', type=float, default=1.5, help="the gamma put on DXEngine's shot (1.5)")
    ap.add_argument('--block', type=int, default=16, help='the block size in pixels (16)')
    ap.add_argument('--region', help='x,y,w,h in pixels: the region whose mean ratio is given')
    ap.add_argument('--shot', type=int, help='one shot number, else every pair')
    ap.add_argument('--blocks', action='store_true', help='print the grid of ratios')
    ap.add_argument('--rmse', action='store_true', help="the grey levels' RMSE and correlation over the region")
    ap.add_argument('--shift', action='store_true', help="also the quarter-pixel move of DXEngine's shot that fits best")
    ap.add_argument('--levels', action='store_true', help='the largest difference of a channel, pixel by pixel, and the pixels over one level')
    a = ap.parse_args()
    region = tuple(int(v) for v in a.region.split(',')) if a.region else None
    # A dxe run in the original's place: its shots BMPs, and no gamma on either
    ext = 'png'
    if not any(f.endswith('.png') for f in os.listdir(a.original)) and any(f.endswith('.bmp') for f in os.listdir(a.original)):
        ext = 'bmp'
        a.gamma = 1.0
    numbers = sorted(int(m[1]) for f in os.listdir(a.original) for m in [re.match(r'Shot(\d+)\.' + ext + '$', f)] if m)
    if a.shot is not None:
        numbers = [n for n in numbers if n == a.shot]
    if not numbers:
        sys.exit('no shots of the original to compare')
    for n in numbers:
        dxe = os.path.join(a.dxe, 'Shot%04d.bmp' % n)
        if not os.path.exists(dxe):
            print('shot %d: the dxe run has none' % n)
            continue
        original = os.path.join(a.original, 'Shot%04d.%s' % (n, ext))
        og, w, h = means(original, 1.0, a.block)
        fg, fw, fh = means(dxe, a.gamma, a.block)
        if (w, h) != (fw, fh):
            sys.exit('shot %d: %dx%d blocks against %dx%d' % (n, w, h, fw, fh))
        x0, y0, x1, y1 = 0, 0, w, h
        if a.region:
            rx, ry, rw, rh = (int(v) for v in a.region.split(','))
            x0, y0 = rx // a.block, ry // a.block
            x1, y1 = (rx + rw + a.block - 1) // a.block, (ry + rh + a.block - 1) // a.block
        ratios = []
        osum = fsum = 0.0
        for y in range(y0, y1):
            for x in range(x0, x1):
                o, f = og[(x, y)], fg[(x, y)]
                osum += o
                fsum += f
                if o >= 2.0 / 255:
                    ratios.append(f / o)
        ratios.sort()
        if not ratios:
            print('shot %d: the original is black in the region' % n)
            continue
        mid = ratios[len(ratios) // 2]
        print('shot %d: dxe/original %.3f over the region (%d blocks of %d; the original %.3f, dxe %.3f); blocks %.2f to %.2f, median %.2f'
              % (n, fsum / osum if osum else 0.0, len(ratios), (x1 - x0) * (y1 - y0), osum / ((x1 - x0) * (y1 - y0)), fsum / ((x1 - x0) * (y1 - y0)), ratios[0], ratios[-1], mid))
        if a.rmse or a.shift:
            og16 = greys(original, 1.0, region)
            fg16 = greys(dxe, a.gamma, region)
            line = 'shot %d: the grey levels %.2f apart (RMSE), correlated at %.3f' % (n, rmse(og16, fg16), correlation(og16, fg16))
            if a.shift:
                fits = [(rmse(og16, greys(dxe, a.gamma, region, (dx / 4, dy / 4))), dx / 4, dy / 4) for dy in range(-4, 5) for dx in range(-4, 5)]
                best = min(fits)
                line += '; best moved %+.2f, %+.2f: %.2f' % (best[1], best[2], best[0])
            print(line)
        if a.levels:
            worst, over, total = levels(channels(original, 1.0, region), channels(dxe, a.gamma, region))
            print('shot %d: pixels at most %d levels apart; %d of %d (%.2f%%) more than one' % (n, worst, over, total, 100.0 * over / total))
        if a.blocks:
            for y in range(y0, y1):
                row = []
                for x in range(x0, x1):
                    o, f = og[(x, y)], fg[(x, y)]
                    row.append('  ..' if o < 2.0 / 255 else '%4.2f' % (f / o))
                print(' '.join(row))

if __name__ == '__main__':
    main()
