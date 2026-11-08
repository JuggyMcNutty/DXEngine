#!/usr/bin/env python3
"""Writes CapDXT1.utx, a texture package of version 68, the game's own, whose
textures carry DXT1 mips beside their P8 ones (bHasComp, CompFormat 3,
TEXF_DXT1, the compressed chain a mod's S3TC texture carries), some of them
short of the bytes their size needs, for bugs.md REN.38's run: DXEngine draws
a texture's compressed mips when they are DXT1 (UTexture::Load), and its
package load pads the P8 mips but not those.

    testdxt1.py <folder>

  CapDXT1Grey        a palette of 256 greys, the P8 mips'.
  CapDXT1Whole       64x64, its seven DXT1 mips whole: green, the control.
  CapDXT1ShortTop    64x64, its top DXT1 mip 1,024 of its 2,048 bytes (blue),
                     the six below whole (blue): the white texel on every
                     device after the fix; before it GL read 1,024 bytes past
                     the mip.
  CapDXT1ShortLower  64x64, its top DXT1 mip whole (red), its 32x32 mip 256
                     of its 512 bytes (yellow), the five below whole (cyan):
                     red drawn large, the 32x32 level zeros (black) after the
                     fix; before it GL read 256 bytes past.
  CapDXT1Wide        32768x4, one DXT1 mip whole (magenta): past 16,384, the
                     GL device's limit before the fix and RADV's and
                     radeonsi's, so the white texel before and after here.
  CapDXT1Big         2048x8, one DXT1 mip whole (magenta): drawn as it is,
                     the white texel once the GL device takes a limit under
                     2048 (the run's TEMPORARY DEBUG hook,
                     DXE_DEBUG_GL_MAXTEX=1024).
  CapDXT1Tiny        2x2, its two DXT1 mips whole, red then a blue 1x1, for
                     bugs.md REN.58 (f): drawn a texel to a pixel, red on every
                     device; the D3D11 device, which pads a block texture under
                     4 texels with a larger level, took the padding in its mip
                     bias too and drew the blue one.

Every P8 mip is whole (index 128, a mid grey). Nothing here is the game's: a
scratch package for the run, never committed. A short mip's lazy-array skip
offset still points past its own (short) data, as a mod's package would.
"""
import os
import struct
import sys


def compact(v):
    """A UE1 compact index."""
    out = bytearray()
    neg = v < 0
    v = abs(v)
    first = (0x80 if neg else 0) | (v & 0x3f)
    v >>= 6
    if v:
        first |= 0x40
    out.append(first)
    while v:
        b = v & 0x7f
        v >>= 7
        out.append(b | (0x80 if v else 0))
    return bytes(out)


NAMES = ['None', 'Core', 'Engine', 'Package', 'Class', 'Palette', 'Texture', 'CapDXT1',
         'CapDXT1Grey', 'UBits', 'VBits', 'USize', 'VSize', 'UClamp', 'VClamp', 'bHasComp',
         'CompFormat', 'CapDXT1Whole', 'CapDXT1ShortTop', 'CapDXT1ShortLower', 'CapDXT1Wide',
         'CapDXT1Big', 'CapDXT1Tiny']


def name(n):
    return compact(NAMES.index(n))


def prop_bool(n):
    # A bool's tag carries its value (the high bit); size byte 0
    return name(n) + bytes([0xd3, 0x00])


def prop_byte(n, v):
    return name(n) + bytes([0x01, v])


def prop_int(n, v):
    return name(n) + bytes([0x22]) + struct.pack('<i', v)


def prop_object(n, ref):
    return name(n) + bytes([0x05]) + compact(ref)


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


GREEN, BLUE, RED, YELLOW, CYAN, MAGENTA = (rgb565(0, 255, 0), rgb565(0, 0, 255), rgb565(255, 0, 0),
                                           rgb565(255, 255, 0), rgb565(0, 255, 255), rgb565(255, 0, 255))


def dxt1_bytes(w, h, colour):
    """A whole DXT1 mip of one colour: c0 = c1, every index 0 (c0 opaque)."""
    blocks = ((w + 3) // 4) * ((h + 3) // 4)
    return struct.pack('<HHI', colour, colour, 0) * blocks


def bits(n):
    b = 0
    while (1 << b) < n:
        b += 1
    return b


def mip(at, data, w, h):
    """A mip as a v68 package holds it: its lazy array's skip offset (the
    file's place after the data), the count, the bytes, its size and bits."""
    body = compact(len(data)) + data
    return struct.pack('<i', at + 4 + len(body)) + body + struct.pack('<ii', w, h) + bytes([bits(w), bits(h)])


def texture(at, w, h, comp_mips):
    """A Texture: its properties, one whole P8 mip, then its DXT1 chain;
    comp_mips is a list of (data, w, h)."""
    props = (prop_object('Palette', 1) + prop_byte('UBits', bits(w)) + prop_byte('VBits', bits(h)) +
             prop_int('USize', w) + prop_int('VSize', h) + prop_int('UClamp', w) + prop_int('VClamp', h) +
             prop_bool('bHasComp') + prop_byte('CompFormat', 3) + name('None'))
    out = props + compact(1)
    out += mip(at + len(out), bytes([128]) * (w * h), w, h)
    out += compact(len(comp_mips))
    for data, mw, mh in comp_mips:
        out += mip(at + len(out), data, mw, mh)
    return out


def chain(w, h, colours, short=None):
    """The DXT1 chain from w x h down to 1x1, a colour a level (the last
    repeated); short = (level, bytes) cuts that level's data."""
    out = []
    level = 0
    while True:
        data = dxt1_bytes(w, h, colours[min(level, len(colours) - 1)])
        if short and short[0] == level:
            data = data[:short[1]]
        out.append((data, w, h))
        if w == 1 and h == 1:
            break
        w, h = max(w // 2, 1), max(h // 2, 1)
        level += 1
    return out


def package():
    imports = [('Core', 'Package', 0, 'Engine'), ('Core', 'Class', -1, 'Palette'),
               ('Core', 'Class', -1, 'Texture')]
    grey = name('None') + compact(256) + b''.join(bytes([n, n, n, 0]) for n in range(256))

    textures = [
        ('CapDXT1Whole', 64, 64, chain(64, 64, [GREEN])),
        ('CapDXT1ShortTop', 64, 64, chain(64, 64, [BLUE], short=(0, 1024))),
        ('CapDXT1ShortLower', 64, 64, chain(64, 64, [RED, YELLOW, CYAN], short=(1, 256))),
        ('CapDXT1Wide', 32768, 4, [(dxt1_bytes(32768, 4, MAGENTA), 32768, 4)]),
        ('CapDXT1Big', 2048, 8, [(dxt1_bytes(2048, 8, MAGENTA), 2048, 8)]),
        ('CapDXT1Tiny', 2, 2, chain(2, 2, [RED, BLUE])),
    ]

    header_size = 4 + 2 + 2 + 4 + 6 * 4 + 16 + 4 + 8
    name_table = b''.join(compact(len(n) + 1) + n.encode('latin-1') + b'\0' + struct.pack('<I', 0x70010) for n in NAMES)
    import_table = b''.join(name(cp) + name(cn) + struct.pack('<i', pk) + name(on) for cp, cn, pk, on in imports)
    name_at = header_size
    import_at = name_at + len(name_table)
    data_at = import_at + len(import_table)

    # Flags: public, loaded for client, server and editor; the textures
    # standalone too
    exports = [(-2, 'CapDXT1Grey', 0x70004, data_at, len(grey))]
    blobs = [grey]
    at = data_at + len(grey)
    for tname, w, h, comp in textures:
        blob = texture(at, w, h, comp)
        exports.append((-3, tname, 0xf0004, at, len(blob)))
        blobs.append(blob)
        at += len(blob)
    export_at = at
    export_table = b''.join(compact(cls) + compact(0) + struct.pack('<i', 0) + name(n) + struct.pack('<I', flags) +
                            compact(size) + compact(where) for cls, n, flags, where, size in exports)
    header = (struct.pack('<IHHI', 0x9e2a83c1, 68, 0, 1) +
              struct.pack('<iiiiii', len(NAMES), name_at, len(exports), export_at, len(imports), import_at) +
              bytes(range(16)) + struct.pack('<i', 1) + struct.pack('<ii', len(exports), len(NAMES)))
    assert len(header) == header_size
    return header + name_table + import_table + b''.join(blobs) + export_table, textures


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    data, textures = package()
    with open(os.path.join(folder, 'CapDXT1.utx'), 'wb') as f:
        f.write(data)
    for tname, w, h, comp in textures:
        sizes = ', '.join('%dx%d %d/%d' % (mw, mh, len(d), ((mw + 3) // 4) * ((mh + 3) // 4) * 8) for d, mw, mh in comp)
        print('%s %dx%d: %s' % (tname, w, h, sizes))


if __name__ == '__main__':
    main()
