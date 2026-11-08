#!/usr/bin/env python3
"""The fog alone in FogTintConsole's shots, both engines (bugs.md REN.66,
REN.67): run with Baseline=True, shot 0 has every volumetric light's
VolumeBrightness 0, its fog of no density and its surfaces' fog maps clear,
and each later shot less it is what the fog adds, the scene dimmed by the
fog's alpha and its colour added (D3DDrv's PF_Highlighted blend).

    fogdelta.py <original run> <dxe run> [--region x,y,w,h] [--gamma 1.5]
                [--base 0] [--shots 1,2,3]

The original's frames carry its gamma ramp (1.5 at the runs' Brightness), so
they are taken back to linear (^1.5) first; DXEngine's shots are linear
(GammaCorrectScreenshots off). For each shot, over the region (the frame but
its top 24 rows, the grabber's mark, by default): the mean of the shot less
the baseline in each channel, linear 0..1, in each engine, and DXEngine's
over the original's. A run is its folder under build/dxcap/runs: the
original's shots are the grabber's Shot0000.png..., DXEngine's
Shot0000.bmp...; the original's missing ones are passed over. Needs
ImageMagick (magick)."""
import argparse
import array
import os
import subprocess


def shot_path(run, n):
    for ext in ("png", "bmp"):
        p = os.path.join(run, "Shot%04d.%s" % (n, ext))
        if os.path.exists(p):
            return p
    return None


def size(path):
    out = subprocess.run(["magick", "identify", "-format", "%w %h", path], check=True, capture_output=True, text=True).stdout
    w, h = out.split()
    return int(w), int(h)


def linear(path, gamma, region):
    cmd = ["magick", path]
    if region:
        cmd += ["-crop", "%dx%d+%d+%d" % (region[2], region[3], region[0], region[1]), "+repage"]
    cmd += ["-depth", "16", "rgb:-"]
    a = array.array("H")
    a.frombytes(subprocess.run(cmd, check=True, capture_output=True).stdout)
    return [(v / 65535.0) ** gamma for v in a]


def delta(run, base, n, gamma, region):
    pb, pn = shot_path(run, base), shot_path(run, n)
    if not pb or not pn:
        return None
    b = linear(pb, gamma, region)
    s = linear(pn, gamma, region)
    sums = [0.0, 0.0, 0.0]
    for i in range(len(b)):
        sums[i % 3] += s[i] - b[i]
    count = len(b) // 3
    return [v / count for v in sums]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("original")
    ap.add_argument("dxe")
    ap.add_argument("--region")
    ap.add_argument("--gamma", type=float, default=1.5)
    ap.add_argument("--base", type=int, default=0)
    ap.add_argument("--shots", default="1,2,3")
    a = ap.parse_args()
    region = [int(v) for v in a.region.split(",")] if a.region else None
    if not region:
        p = shot_path(a.dxe, a.base) or shot_path(a.original, a.base)
        w, h = size(p)
        region = [0, 24, w, h - 24]
    for n in [int(v) for v in a.shots.split(",")]:
        do = delta(a.original, a.base, n, a.gamma, region)
        dd = delta(a.dxe, a.base, n, 1.0, region)
        if do is None or dd is None:
            print("shot %d: missing in %s" % (n, "the original" if do is None else "DXEngine"))
            continue
        ratio = ["%.3f" % (dd[c] / do[c]) if abs(do[c]) > 1e-6 else "-" for c in range(3)]
        print("shot %d over the baseline, red green blue: original %.4f %.4f %.4f, DXEngine %.4f %.4f %.4f, DXEngine's over the original's %s"
              % (n, do[0], do[1], do[2], dd[0], dd[1], dd[2], " ".join(ratio)))


if __name__ == "__main__":
    main()
