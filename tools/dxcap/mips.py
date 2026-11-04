#!/usr/bin/env python3
"""Reads MipConsole's shots: which of CapMips' mips each row of the floor was
drawn from, and how many of its pixels took more than one texel
(docs/DEVELOPMENT.md, scripted runs; bugs.md D.40, D.41).

    mips.py <shot> [<shot> ...] [--columns 448,576] [--top 406]
    mips.py --compare <shot> <shot> [--columns ...] [--top ...]

A shot is any image ImageMagick reads (the original's grabbed ShotNNNN.ppm,
DXEngine's ShotNNNN.bmp). Each pixel of the columns asked for (the middle
128 of a 1024 view by default), from --top (the first row under the floor's
far edge, 404.6 at 1024x768 and the player's field of view of 75) to the
bottom, is named by the hue nearest its own among testtextures.py's HUES,
each colour scaled so its brightest channel is 1: its mip that hue's. Its
shade, its brightest channel, is a texel's full or half one when the pixel
took one texel, and any other when it took two or more: a filter's blend.
A shot's full and half shades are the two commonest among its bottom 64
rows (gamma and an unlit mesh's light differ between the engines), a shade
within 3 of either one texel's. Pixels darker than 24 are the sky.

For each shot: each mip's rows, the first and the last row whose pixels are
mostly of that mip; the blended share of the floor's pixels by mip. With
--compare, also each mip's first row in both shots, and the rows where the
two differ.
"""
import argparse
import subprocess
import sys

# testtextures.py's CapMips colours, mip 0 first
HUES = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255),
        (255, 255, 255), (255, 128, 0), (128, 0, 255)]
SKY = 24


def load(path):
    w, h = map(int, subprocess.run(['magick', 'identify', '-format', '%w %h', path + '[0]'],
                                   check=True, capture_output=True, text=True).stdout.split())
    raw = subprocess.run(['magick', path + '[0]', '-depth', '8', 'rgb:-'], check=True, capture_output=True).stdout
    return w, h, raw


def hue_of(r, g, b):
    m = max(r, g, b)
    best, bestd = 0, None
    for i, (hr, hg, hb) in enumerate(HUES):
        hm = max(hr, hg, hb)
        d = (r / m - hr / hm) ** 2 + (g / m - hg / hm) ** 2 + (b / m - hb / hm) ** 2
        if bestd is None or d < bestd:
            best, bestd = i, d
    return best


def two_levels(pixels, h):
    # A shot's full and half shades: the two commonest of its bottom rows'
    counts = {}
    for y, mip, m in pixels:
        if y >= h - 64:
            counts[m] = counts.get(m, 0) + 1
    top = sorted(counts, key=counts.get, reverse=True)
    # Neighbouring values of one shade (a device's rounding) are one
    first = top[0]
    second = next((v for v in top[1:] if abs(v - first) > 3), first)
    return min(first, second), max(first, second)


def read(path, columns, top):
    w, h, raw = load(path)
    x0, x1 = columns
    rows = {}
    pixels = []
    for y in range(top, h):
        for x in range(x0, x1):
            i = 3 * (y * w + x)
            r, g, b = raw[i], raw[i + 1], raw[i + 2]
            m = max(r, g, b)
            if m < SKY:
                continue
            pixels.append((y, hue_of(r, g, b), m))
    if not pixels:
        return None
    half, full = two_levels(pixels, h)
    blended = {}
    counts = {}
    for y, mip, m in pixels:
        rows.setdefault(y, {}).setdefault(mip, 0)
        rows[y][mip] += 1
        counts[mip] = counts.get(mip, 0) + 1
        if abs(m - half) > 3 and abs(m - full) > 3:
            blended[mip] = blended.get(mip, 0) + 1
    majority = {y: max(c, key=c.get) for y, c in rows.items()}
    return {'size': (w, h), 'half': half, 'full': full, 'majority': majority, 'counts': counts,
            'blended': blended, 'pixels': len(pixels)}


def bands(majority):
    out = {}
    for y in sorted(majority):
        m = majority[y]
        a, b = out.get(m, (y, y))
        out[m] = (min(a, y), max(b, y))
    return out


def report(path, r):
    print(f'{path}: {r["size"][0]}x{r["size"][1]}, {r["pixels"]} floor pixels, shades {r["half"]:.0f} and {r["full"]:.0f}')
    b = bands(r['majority'])
    total = sum(r['blended'].values())
    print(f'  blended {total} of {r["pixels"]} ({100.0 * total / r["pixels"]:.1f}%)')
    for m in sorted(b):
        n = r['counts'].get(m, 0)
        k = r['blended'].get(m, 0)
        print(f'  mip {m}: rows {b[m][0]}-{b[m][1]}, {n} pixels, {k} blended ({100.0 * k / n if n else 0:.1f}%)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('shots', nargs='+')
    ap.add_argument('--compare', action='store_true')
    ap.add_argument('--columns', default='448,576')
    ap.add_argument('--top', type=int, default=406)
    a = ap.parse_args()
    columns = tuple(int(v) for v in a.columns.split(','))
    results = []
    for path in a.shots:
        r = read(path, columns, a.top)
        if r is None:
            print(f'{path}: no floor in the columns')
            sys.exit(1)
        report(path, r)
        results.append(r)
    if a.compare:
        if len(results) != 2:
            sys.exit('--compare takes two shots')
        ma, mb = results[0]['majority'], results[1]['majority']
        ba, bb = bands(ma), bands(mb)
        print('first row (from the horizon down) of each mip, first shot / second:')
        for m in sorted(set(ba) | set(bb)):
            fa = ba[m][0] if m in ba else '-'
            fb = bb[m][0] if m in bb else '-'
            print(f'  mip {m}: {fa} / {fb}')
        apart = [y for y in sorted(set(ma) & set(mb)) if ma[y] != mb[y]]
        print(f'rows whose mips differ: {len(apart)} of {len(set(ma) & set(mb))}'
              + (f' ({", ".join(str(y) for y in apart[:40])}{" ..." if len(apart) > 40 else ""})' if apart else ''))


if __name__ == '__main__':
    main()
