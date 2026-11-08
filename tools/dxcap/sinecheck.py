#!/usr/bin/env python3
"""SineTableConsole's shots against the two sine tables (bugs.md REN.68).

The wet texture the console draws (Effects.Electricity.Xplsn_EMPG, eleven
WhirlyThing drops over Xplsn_Emp, no random bytes) is stepped Steps times by
wetsim.py beside it, once with
the fire engine's sine table truncated and once rounded to the nearest
(Fire.dll's fistp, 0x1050106a). Each texel of the shot -- the centre pixel of
its Scale-by-Scale block, the tile's corner at (X, Y) -- is matched to the
nearer of the two predictions' colours, over the texels whose two predicted
colours lie more than 8 levels apart in a channel; the original's frames
carry its gamma ramp, so with --gamma the predictions take it first, as
shots.py puts DXEngine's shots through it.

    sinecheck.py <Effects.utx> <shot> [--gamma 1.5] [--steps 100]
                 [--x 64] [--y 64] [--scale 4] [--tex Xplsn_EMPG]

Prints how many of those texels the shot matches rounded, truncated, and
neither (within 24 levels of neither), and the mean channel difference to
each prediction over the whole tile. The original's shot and a fixed
DXEngine's should match the rounded table; HEAD's before the fix the
truncated one. Needs ImageMagick (magick)."""
import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue1pkg  # noqa: E402
import wetsim  # noqa: E402


def predictions(path, name, steps):
    p = ue1pkg.Package(path)
    wet = wetsim.texture(p, name)
    pr = ue1pkg.props(p, wet)
    import struct
    n = struct.unpack("<i", pr["NumDrops"][1][:4])[0]
    drops = []
    for k, v in pr.items():
        if k.startswith("Drops"):
            i = int(k[6:-1]) if "[" in k else 0
            if i < n:
                drops.append((i, tuple(v[1][:8])))
    drops = [d for _, d in sorted(drops)]
    amp = pr["WaveAmp"][1][0]
    srcname = p.obj_name(ue1pkg.read_compact(bytes(pr["SourceTexture"][1]), 0)[0])
    w, h, src = wetsim.read_mip0(p, wetsim.texture(p, srcname))
    src_e = wetsim.texture(p, srcname)
    pal_ref, _ = ue1pkg.read_compact(bytes(ue1pkg.props(p, src_e)["Palette"][1]), 0)
    pal_e = p.exports[pal_ref - 1]
    o = ue1pkg.skip_properties(p, pal_e["off"])
    cnt, o = ue1pkg.read_compact(p.b, o)
    palette = [tuple(p.b[o + 4 * i:o + 4 * i + 3]) for i in range(cnt)]
    trunc = wetsim.run(steps, wetsim.sine(False), drops, w, h, src, amp)
    rnd = wetsim.run(steps, wetsim.sine(True), drops, w, h, src, amp)
    return w, h, palette, trunc, rnd


def shot_pixels(path, x, y, w, h):
    cmd = ["magick", path, "-crop", "%dx%d+%d+%d" % (w, h, x, y), "+repage", "-depth", "8", "rgb:-"]
    return subprocess.run(cmd, check=True, capture_output=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("utx")
    ap.add_argument("shot")
    ap.add_argument("--gamma", type=float, default=1.0)
    ap.add_argument("--steps", type=int, default=100)
    ap.add_argument("--x", type=int, default=64)
    ap.add_argument("--y", type=int, default=64)
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--tex", default="Xplsn_EMPG")
    a = ap.parse_args()
    w, h, palette, trunc, rnd = predictions(a.utx, a.tex, a.steps)
    gam = [round(255.0 * (v / 255.0) ** (1.0 / a.gamma)) for v in range(256)]
    colour = lambda i: tuple(gam[c] for c in palette[i])
    px = shot_pixels(a.shot, a.x, a.y, w * a.scale, h * a.scale)
    row = w * a.scale * 3
    near_r = near_t = neither = counted = 0
    sum_r = sum_t = 0
    for ty in range(h):
        for tx in range(w):
            sx = tx * a.scale + a.scale // 2
            sy = ty * a.scale + a.scale // 2
            o = sy * row + sx * 3
            got = (px[o], px[o + 1], px[o + 2])
            cr, ct = colour(rnd[ty * w + tx]), colour(trunc[ty * w + tx])
            dr = max(abs(got[c] - cr[c]) for c in range(3))
            dt = max(abs(got[c] - ct[c]) for c in range(3))
            sum_r += sum(abs(got[c] - cr[c]) for c in range(3)) / 3.0
            sum_t += sum(abs(got[c] - ct[c]) for c in range(3)) / 3.0
            if max(abs(cr[c] - ct[c]) for c in range(3)) <= 8:
                continue
            counted += 1
            if min(dr, dt) > 24:
                neither += 1
            elif dr < dt:
                near_r += 1
            elif dt < dr:
                near_t += 1
    n = w * h
    print(f"{a.shot}: of {counted} texels whose predictions differ, rounded {near_r}, truncated {near_t}, neither {neither}")
    print(f"mean channel difference over {n} texels: to rounded {sum_r / n:.2f}, to truncated {sum_t / n:.2f}")


if __name__ == "__main__":
    main()
