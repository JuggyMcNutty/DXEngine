#!/usr/bin/env python3
"""Writes the harness's own mesh, which DXCapture.u imports into itself
(CapQuad.uc's #exec MESH IMPORT; tools/dxcap.sh compile; docs/DEVELOPMENT.md,
scripted runs), in the editor's two Unreal 3D files: a shape the game ships
none of, made here rather than kept as files.

    testmeshes.py <folder>

  CapQuad_d.3d, CapQuad_a.3d  a flat square of 16 by 16 tiles, its points 120
                  apart from -960 to 960 along X and Y, Z 0: 289 points, 512
                  triangles, two-sided, one frame. Each tile is two triangles
                  that take the whole of texture 0 once, U along X and V
                  along Y, so a texture of 256 by 256 lies on it 16 times
                  across: a floor whose texels shrink from the eye to the
                  horizon (MipConsole).
"""
import os
import struct
import sys

TILES = 16
STEP = 120


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    n = TILES + 1
    half = TILES * STEP // 2
    points = [(-half + i * STEP, -half + j * STEP, 0) for j in range(n) for i in range(n)]

    def at(i, j):
        return j * n + i

    tris = []
    for j in range(TILES):
        for i in range(TILES):
            a, b, c, d = at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1)
            tris.append(((a, b, c), ((0, 0), (255, 0), (255, 255))))
            tris.append(((a, c, d), ((0, 0), (255, 255), (0, 255))))

    # The data file: its header (the triangles' and points' counts, the rest
    # unused, 48 bytes), then each triangle: its three points, its type (1,
    # two-sided), its colour, its three corners' U and V in 256ths of the
    # texture, its texture's number and its flags
    data = struct.pack('<HHHH', len(tris), len(points), 0, 0) + bytes(40)
    for corners, uv in tris:
        data += struct.pack('<HHH', *corners) + bytes([1, 0])
        data += b''.join(bytes([u, v]) for u, v in uv) + bytes([0, 0])

    # The animation file: the frames' count and a frame's size, then each
    # frame's points, each X, Y and Z signed 16-bit and a 16-bit pad: Deus
    # Ex's own points (its editor reads 8 bytes a point, where Unreal's packs
    # one into 32 bits)
    anim = struct.pack('<HH', 1, 8 * len(points))
    for x, y, z in points:
        anim += struct.pack('<hhhh', x, y, z, 0)

    with open(os.path.join(folder, 'CapQuad_d.3d'), 'wb') as f:
        f.write(data)
    with open(os.path.join(folder, 'CapQuad_a.3d'), 'wb') as f:
        f.write(anim)


if __name__ == '__main__':
    main()
