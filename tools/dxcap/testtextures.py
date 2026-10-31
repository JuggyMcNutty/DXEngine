#!/usr/bin/env python3
"""Writes the harness's own texture package, which DXCapture.u loads into itself
(tools/dxcap.sh compile; docs/DEVELOPMENT.md, scripted runs): textures of kinds
the game ships none of, made here rather than kept as files.

    testtextures.py <folder>

  CapTextures.utx  a package of version 68, the game's own, holding:
    CapGrey        a palette of 256 greys, entry n's red, green and blue n;
    CapScripted    a ScriptedTexture of 64 by 64 in CapGrey, no source texture
                   and no notify actor, its one mip saved empty, as an editor
                   saves a scripted texture: the engine makes its pixels.
    CapSourced     the same with CoreTexMisc.plaque_b, 64 by 64, its source
                   texture.
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


NAMES = ['None', 'Core', 'Engine', 'Package', 'Class', 'Palette', 'ScriptedTexture', 'CapTextures',
         'CapGrey', 'CapScripted', 'bParametric', 'bRealtime', 'UBits', 'VBits', 'USize', 'VSize',
         'UClamp', 'VClamp', 'CapSourced', 'SourceTexture', 'Texture', 'CoreTexMisc', 'Metal', 'plaque_b']


def name(n):
    return compact(NAMES.index(n))


def prop_bool(n):
    # A bool's tag carries its value; size byte 0, as editors write it
    return name(n) + bytes([0xd3, 0x00])


def prop_byte(n, v):
    return name(n) + bytes([0x01, v])


def prop_int(n, v):
    return name(n) + bytes([0x22]) + struct.pack('<i', v)


def prop_object(n, ref):
    return name(n) + bytes([0x05]) + compact(ref)


def package():
    # Imports: Engine (a package), and its classes Palette and ScriptedTexture;
    # an import is referred to as -(its index + 1).
    imports = [('Core', 'Package', 0, 'Engine'), ('Core', 'Class', -1, 'Palette'),
               ('Core', 'Class', -1, 'ScriptedTexture'), ('Core', 'Package', 0, 'CoreTexMisc'),
               ('Core', 'Package', -4, 'Metal'), ('Engine', 'Texture', -5, 'plaque_b')]
    grey = name('None') + compact(256) + b''.join(bytes([n, n, n, 0]) for n in range(256))

    def scripted(at, source=None):
        size = 64
        props = (prop_bool('bParametric') + prop_bool('bRealtime') + prop_object('Palette', 1) +
                 (prop_object('SourceTexture', source) if source else b'') +
                 prop_byte('UBits', 6) + prop_byte('VBits', 6) + prop_int('USize', size) +
                 prop_int('VSize', size) + prop_int('UClamp', size) + prop_int('VClamp', size) +
                 name('None'))
        # One mip: its lazy array's skip place (the byte after its data), no
        # data, then its size and bits
        mip_at = at + len(props) + len(compact(1))
        mip = struct.pack('<i', mip_at + 4 + len(compact(0))) + compact(0) + struct.pack('<ii', size, size) + bytes([6, 6])
        return props + compact(1) + mip

    header_size = 4 + 2 + 2 + 4 + 6 * 4 + 16 + 4 + 8
    name_table = b''.join(compact(len(n) + 1) + n.encode('latin-1') + b'\0' + struct.pack('<I', 0x70010) for n in NAMES)
    import_table = b''.join(name(cp) + name(cn) + struct.pack('<i', pk) + name(on) for cp, cn, pk, on in imports)
    name_at = header_size
    import_at = name_at + len(name_table)
    data_at = import_at + len(import_table)
    grey_at = data_at
    scripted_at = grey_at + len(grey)
    scripted_data = scripted(scripted_at)
    sourced_at = scripted_at + len(scripted_data)
    sourced_data = scripted(sourced_at, source=-6)
    export_at = sourced_at + len(sourced_data)
    exports = [(-2, 'CapGrey', 0x70004, grey_at, len(grey)),
               (-3, 'CapScripted', 0xf0004, scripted_at, len(scripted_data)),
               (-3, 'CapSourced', 0xf0004, sourced_at, len(sourced_data))]
    export_table = b''.join(compact(cls) + compact(0) + struct.pack('<i', 0) + name(n) + struct.pack('<I', flags) +
                            compact(size) + compact(at) for cls, n, flags, at, size in exports)
    header = (struct.pack('<IHHI', 0x9e2a83c1, 68, 0, 1) +
              struct.pack('<iiiiii', len(NAMES), name_at, len(exports), export_at, len(imports), import_at) +
              bytes(range(16)) + struct.pack('<i', 1) + struct.pack('<ii', len(exports), len(NAMES)))
    assert len(header) == header_size
    return header + name_table + import_table + grey + scripted_data + sourced_data + export_table


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, 'CapTextures.utx'), 'wb') as f:
        f.write(package())


if __name__ == '__main__':
    main()
