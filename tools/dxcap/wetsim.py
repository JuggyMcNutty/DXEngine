"""DrunkFX (Effects.UserInterface.DrunkFX, a WetTexture over DrunkFXRef whose
27 drops are all ShallowSpot) stepped N times by DXEngine's FireEngine
(ported line for line from FireEngine.cpp: RedrawDrops' ShallowSpot,
WaterPass, BuildWaterTable, BuildRefractionTable, ApplyWet), once with the
sine table truncated (DXEngine's InitTables) and once rounded to nearest
(Fire.dll's fistp at 0x1050106a): how many of its texels differ.

    wetsim.py <Effects.utx> [steps ...]
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue1pkg  # noqa: E402


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def sine(rounded):
    c1, c2 = f32(0.00390625), f32(6.2831855)
    out = []
    for i in range(256):
        v = f32(math.sin(i * c1 * c2) * 127.5 + 127.44999694824219)
        out.append((round(v) if rounded else int(v)) & 0xFF)
    return out


def water_table():
    return [max(min(i // 2 - 256 + (1 if i < 256 else 0), 255), 0) for i in range(1536)]


def refraction_table(amp):
    t = []
    for i in range(1024):
        s = int((i - 511) * (amp * 0.001953125))
        t.append(max(min(s, 127), -128) & 0xFF)
    return t


def texture(p, name):
    for e in p.exports:
        if p.names[e["name"]] == name:
            return e
    raise KeyError(name)


def read_mip0(p, e):
    o = ue1pkg.skip_properties(p, e["off"])
    r = ue1pkg.Reader(p, o)
    n = r.idx()
    if p.version >= 63:
        r.i32()
    size = r.idx()
    data = bytes(p.b[r.o:r.o + size])
    r.o += size
    w = r.u32(); h = r.u32()
    return w, h, data


def run(steps, sin, drops, usize, vsize, source, amp):
    half, rows = usize // 2, vsize // 2
    fields = bytearray([0x80]) * (usize * rows)
    bits = bytearray(usize * vsize)
    wt = water_table()
    rt = refraction_table(amp)
    hmask = (usize - 1) >> 1
    vmask = (vsize - 1) >> 1
    drops = [list(d) for d in drops]
    parity = 0
    size = usize * vsize // 2
    for _ in range(steps):
        # RedrawDrops: the deterministic kinds (FireEngine.cpp's cases)
        def put(at, v1, v2=None):
            if v2 is None:
                v2 = v1
            if 0 <= at < size:
                fields[at] = v1 & 0xFF
            if 0 <= at and at + half < size:
                fields[half + at] = v2 & 0xFF
        cell = lambda x, y: x + y * usize
        for d in drops:
            t = d[0]
            at = cell(d[2], d[3])
            if t == 0:
                put(at, d[7])
            elif t == 1:
                d[1] = (d[1] + d[7]) & 0xFF
                put(at, sin[d[1]])
            elif t == 2:
                d[1] = (d[1] + d[7]) & 0xFF
                put(at, (sin[d[1]] >> 1) + 64)
            elif t == 3:
                d[1] = (d[1] + d[7]) & 0xFF
                v = sin[d[1]]
                put(at, 0x80 if v < 0x80 else v)
            elif t in (6, 7, 64, 65):
                angle = d[4] | (d[5] << 8)
                speed = d[6] | (d[7] << 8)
                angle = (angle + speed) & 0xFFFF if t < 64 else (angle - speed) & 0xFFFF
                d[4] = angle & 0xFF
                d[5] = angle >> 8
                shift = 4 if t in (6, 64) else 3
                x = hmask & (d[2] + (sin[d[5]] >> shift))
                y = vmask & (d[3] + (sin[(d[5] + 64) & 0xFF] >> shift))
                put(cell(x, y), d[1])
            elif t in (8, 9, 10, 11, 12, 13, 14, 15):
                value = d[1]
                if t >= 12:
                    d[1] = (d[1] + d[6]) & 0xFF
                    value = sin[d[1]]
                for k in range((d[7] >> 1) + 1):
                    if t in (8, 12):
                        put(cell(hmask & (d[2] + k), d[3]), value)
                    elif t in (9, 13):
                        put(cell(d[2], vmask & (d[3] + k)), value)
                    elif t in (10, 14):
                        put(cell(hmask & (d[2] - k), vmask & (d[3] + k)), value)
                    else:
                        put(cell(hmask & (d[2] + k), vmask & (d[3] + k)), value)
            elif t == 17:
                n = d[7] >> 1
                for row in range(n):
                    for col in range(n):
                        put(cell(hmask & (d[2] + col), vmask & (d[3] + row)), d[1])
            elif t == 18:
                d[4] = (d[4] + d[7]) & 0xFF
                if d[4] <= d[7]:
                    put(at, d[1], (~d[1]) & 0xFF)
            else:
                raise SystemExit(f"drop kind {t} draws random bytes: not deterministic")
        # WaterPass
        parity = (parity + 1) & 0xFF
        first_moves = (parity & 1) == 0
        old = bytes(fields)

        def first(x, y):
            return old[(y % rows) * usize + (x % half)]

        def second(x, y):
            return old[(y % rows) * usize + half + (x % half)]

        def setpix(x, y, v):
            bits[(y % vsize) * usize + (x % usize)] = v

        for y in range(rows):
            for x in range(half):
                if first_moves:
                    s = second(x - 1, y) + second(x - 1, y - 1) + second(x, y) + second(x, y - 1) - 2 * first(x, y)
                    fields[y * usize + x] = wt[512 + s]
                    sl = lambda sx, sy: second(sx, sy) - second(sx - 2, sy)
                    here, left = sl(x, y), sl(x - 1, y)
                    above = (sl(x, y) + sl(x, y - 1)) >> 1
                    aboveleft = (sl(x - 1, y) + sl(x - 1, y - 1)) >> 1
                    setpix(2 * x - 1, 2 * y, rt[512 + here + left])
                    setpix(2 * x, 2 * y, rt[512 + 2 * here])
                    setpix(2 * x - 1, 2 * y - 1, rt[512 + above + aboveleft])
                    setpix(2 * x, 2 * y - 1, rt[512 + 2 * above])
                else:
                    s = first(x, y) + first(x + 1, y) + first(x, y + 1) + first(x + 1, y + 1) - 2 * second(x, y)
                    fields[y * usize + half + x] = wt[512 + s]
                    sl = lambda sx, sy: first(sx, sy) - first(sx - 2, sy)
                    below, belowleft = sl(x + 1, y + 1), sl(x, y + 1)
                    mid = (sl(x + 1, y + 1) + sl(x + 1, y)) >> 1
                    midleft = (sl(x, y + 1) + sl(x, y)) >> 1
                    setpix(2 * x + 1, 2 * y + 1, rt[512 + 2 * below])
                    setpix(2 * x, 2 * y + 1, rt[512 + below + belowleft])
                    setpix(2 * x + 1, 2 * y, rt[512 + 2 * mid])
                    setpix(2 * x, 2 * y, rt[512 + mid + midleft])
        # ApplyWet
        for y in range(vsize):
            row = y * usize
            for x in range(usize):
                bits[row + x] = source[row + ((x + bits[row + x]) & (usize - 1))]
    return bits


def main():
    path = sys.argv[1]
    steps_list = [int(s) for s in sys.argv[2:]] or [1, 10, 30, 60]
    p = ue1pkg.Package(path)
    name = os.environ.get("WETSIM_TEX", "DrunkFX")
    wet = texture(p, name)
    pr = ue1pkg.props(p, wet)
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
    w, h, src = read_mip0(p, texture(p, srcname))
    print(f"{name} over {srcname} {w}x{h}, {len(drops)} drops, kinds {sorted(set(d[0] for d in drops))}, WaveAmp {amp}")
    # the source's palette, which the wet texture takes (UWetTexture::PostLoad)
    src_e = texture(p, srcname)
    pal_ref, _ = ue1pkg.read_compact(bytes(ue1pkg.props(p, src_e)["Palette"][1]), 0)
    pal_e = p.exports[pal_ref - 1]
    o = ue1pkg.skip_properties(p, pal_e["off"])
    cnt, o = ue1pkg.read_compact(p.b, o)
    palette = [tuple(p.b[o + 4 * i:o + 4 * i + 3]) for i in range(cnt)]
    print("palette", p.names[pal_e["name"]], cnt)
    for steps in steps_list:
        a = run(steps, sine(False), drops, w, h, src, amp)
        b = run(steps, sine(True), drops, w, h, src, amp)
        diff = sum(1 for i in range(len(a)) if a[i] != b[i])
        cdiff = sum(1 for i in range(len(a)) if max(abs(palette[a[i]][c] - palette[b[i]][c]) for c in range(3)) > 8)
        print(f"after {steps} steps: {diff} of {len(a)} texels differ, {cdiff} by more than 8 levels in a channel")
        if "--ppm" in os.environ.get("WETSIM", ""):
            for tag, img in (("truncated", a), ("rounded", b)):
                outname = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"{name.lower()}-{steps}-{tag}.ppm")
                with open(outname, "wb") as f:
                    f.write(b"P6 %d %d 255\n" % (w, h))
                    f.write(bytes(c for i in img for c in palette[i]))
                print("   wrote", outname)


if __name__ == "__main__":
    main()
