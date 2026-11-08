#!/usr/bin/env python3
"""Writes a copy of a UCC-compiled package with two of a
class's properties retyped, for FixedMapConsole: no package of the game's,
Revision's or the mods' declares a FixedArrayProperty or a MapProperty
(pkgscan.py), and whether UCC can declare one is not read (Editor.dll is
never read), so a package made by hand reaches them (bugs.md VM.31, VM.32).

Checked on a copy of DeusEx.u (Skill cost CurrentLevel): the two exports
retyped, three appended, every other export's bytes and the tables' old
entries alike, a reader of the package's tables reading it back.

    fixedmap.py <package> <out> [<class> [<fixed> <mapped>]]

<class> (FixedMapConsole) declares `var config int Fixed[3];` and
`var config int Mapped;`. In the copy:
- Fixed is a FixedArrayProperty (Core's class, imported) of ArrayDim 1, its
  flags and category kept, its Inner a new IntProperty export named
  FixedInner (outer Fixed) and its Count the old ArrayDim, 3. Its size in an
  object is the old one, 12 bytes (the original's Link: Count x the inner's
  ElementSize, Core.dll 0x10167c60).
- Mapped is a MapProperty of ArrayDim 1, its Key and Value new IntProperty
  exports named MappedKey and MappedValue (outer Mapped). Its size is the
  original's TMap, 20 bytes (0x10168ca0).
Each new inner is serialized as UCC writes an ArrayProperty's Inner: no
tagged properties, no super, no next, ArrayDim 1, no flags, category None.
The serials as the original's UFixedArrayProperty::Serialize (0x10167df0:
UProperty's, then Inner, then Count as four bytes) and UMapProperty::Serialize
(0x10168db0: UProperty's, then Key, then Value).

Names, imports and exports are appended, so every index the class's code
and the other exports hold stays valid; the file is laid out again in UCC's
order -- header, names, the exports' data, imports, exports -- with the last
generation's counts set to the new ones. The class's code must not name the
two properties (FixedMapConsole reads them by their text alone).
"""
import struct
import sys

TAG = 0x9E2A83C1
CPF_NET = 0x20


def read_index(b, p):
    v = b[p]
    p += 1
    neg = v & 0x80
    r = v & 0x3F
    if v & 0x40:
        shift = 6
        while True:
            v = b[p]
            p += 1
            r |= (v & 0x7F) << shift
            shift += 7
            if not v & 0x80:
                break
    return (-r if neg else r), p


def write_index(v):
    out = bytearray()
    a = abs(v)
    first = (0x80 if v < 0 else 0) | (a & 0x3F)
    a >>= 6
    if a:
        first |= 0x40
    out.append(first)
    while a:
        byte = a & 0x7F
        a >>= 7
        if a:
            byte |= 0x80
        out.append(byte)
    return bytes(out)


def main():
    if len(sys.argv) not in (3, 4, 6):
        sys.exit(__doc__)
    src, dst = sys.argv[1:3]
    clsname = sys.argv[3] if len(sys.argv) >= 4 else 'FixedMapConsole'
    fixed_name, mapped_name = sys.argv[4:6] if len(sys.argv) == 6 else ('Fixed', 'Mapped')
    b = open(src, 'rb').read()
    tag, ver, lic, pflags, nc, no, ec, eo, ic, io = struct.unpack_from('<IHHIiiiiii', b, 0)
    if tag != TAG or ver < 68:
        sys.exit('%s: not a version 68 package' % src)
    guid = b[36:52]
    gencount, = struct.unpack_from('<i', b, 52)
    gens = [list(struct.unpack_from('<ii', b, 56 + 8 * i)) for i in range(gencount)]

    names = []
    p = no
    for _ in range(nc):
        n, p = read_index(b, p)
        names.append([b[p:p + n - 1], None])
        p += n
        names[-1][1], = struct.unpack_from('<I', b, p)
        p += 4
    imports = []
    p = io
    for _ in range(ic):
        cp, p = read_index(b, p)
        cn, p = read_index(b, p)
        pk, = struct.unpack_from('<i', b, p)
        p += 4
        on, p = read_index(b, p)
        imports.append([cp, cn, pk, on])
    exports = []
    p = eo
    for _ in range(ec):
        cl, p = read_index(b, p)
        su, p = read_index(b, p)
        pk, = struct.unpack_from('<i', b, p)
        p += 4
        on, p = read_index(b, p)
        of, = struct.unpack_from('<I', b, p)
        p += 4
        size, p = read_index(b, p)
        off = 0
        if size > 0:
            off, p = read_index(b, p)
        exports.append({'class': cl, 'super': su, 'outer': pk, 'name': on, 'flags': of,
                        'data': bytes(b[off:off + size]) if size > 0 else b''})

    def name_of(i):
        return names[i][0].decode('latin-1')

    def name_index(s, like='IntProperty'):
        for i, (n, _) in enumerate(names):
            if n.decode('latin-1') == s:
                return i
        flags = next(f for n, f in names if n.decode('latin-1') == like)
        names.append([s.encode('latin-1'), flags])
        return len(names) - 1

    def import_of(cls, obj):
        for i, im in enumerate(imports):
            if name_of(im[1]) == cls and name_of(im[3]) == obj:
                return -(i + 1)
        return 0

    core = import_of('Package', 'Core')
    intprop = import_of('Class', 'IntProperty')
    if not core or not intprop:
        sys.exit('%s: no Core or Core.IntProperty import' % src)

    def class_import(obj):
        found = import_of('Class', obj)
        if found:
            return found
        imports.append([name_index('Core'), name_index('Class'), core, name_index(obj)])
        return -len(imports)

    owner = [i + 1 for i, e in enumerate(exports) if e['class'] == 0 and name_of(e['name']) == clsname]
    if len(owner) != 1:
        sys.exit('%s: %d classes named %s, not one' % (src, len(owner), clsname))
    owner = owner[0]

    def prop_of(prop):
        found = [i for i, e in enumerate(exports)
                 if e['outer'] == owner and name_of(e['name']) == prop and e['class'] == intprop]
        if len(found) != 1:
            sys.exit('%s: %s.%s is no single IntProperty' % (src, clsname, prop))
        return found[0]

    def split_property(data):
        # None (no tagged properties), SuperField, Next, ArrayDim, flags,
        # category, RepOffset when replicated: UProperty::Serialize
        # (0x10165020) after UField's
        q = 0
        none, q = read_index(data, q)
        sf, q = read_index(data, q)
        nx, q = read_index(data, q)
        dim, flags = struct.unpack_from('<iI', data, q)
        q += 8
        cat, q = read_index(data, q)
        rep = b''
        if flags & CPF_NET:
            rep = data[q:q + 2]
            q += 2
        if q != len(data):
            sys.exit('an IntProperty with %d bytes past its fields' % (len(data) - q))
        return none, sf, nx, dim, flags, cat, rep

    def property_bytes(none, sf, nx, dim, flags, cat, rep):
        return (write_index(none) + write_index(sf) + write_index(nx) +
                struct.pack('<iI', dim, flags) + write_index(cat) + rep)

    none_name = name_index('None')

    def new_inner(outer_export, name):
        exports.append({'class': intprop, 'super': 0, 'outer': outer_export + 1,
                        'name': name_index(name), 'flags': exports[outer_export]['flags'],
                        'data': property_bytes(none_name, 0, 0, 1, 0, none_name, b'')})
        return len(exports)        # its export number, a positive reference

    fixed = prop_of(fixed_name)
    none, sf, nx, dim, flags, cat, rep = split_property(exports[fixed]['data'])
    inner = new_inner(fixed, fixed_name + 'Inner')
    exports[fixed]['class'] = class_import('FixedArrayProperty')
    exports[fixed]['data'] = (property_bytes(none, sf, nx, 1, flags, cat, rep) +
                              write_index(inner) + struct.pack('<i', dim))
    print('%s.%s: FixedArrayProperty, Inner export %d, Count %d' % (clsname, fixed_name, inner, dim))

    mapped = prop_of(mapped_name)
    none, sf, nx, dim, flags, cat, rep = split_property(exports[mapped]['data'])
    key = new_inner(mapped, mapped_name + 'Key')
    value = new_inner(mapped, mapped_name + 'Value')
    exports[mapped]['class'] = class_import('MapProperty')
    exports[mapped]['data'] = (property_bytes(none, sf, nx, 1, flags, cat, rep) +
                               write_index(key) + write_index(value))
    print('%s.%s: MapProperty, Key export %d, Value export %d' % (clsname, mapped_name, key, value))

    # Laid out again: header, names, data, imports, exports
    header_size = 36 + 16 + 4 + 8 * gencount
    out = bytearray(header_size)
    name_off = len(out)
    for n, f in names:
        out += write_index(len(n) + 1) + n + b'\0' + struct.pack('<I', f)
    offsets = []
    for e in exports:
        offsets.append(len(out))
        out += e['data']
    import_off = len(out)
    for cp, cn, pk, on in imports:
        out += write_index(cp) + write_index(cn) + struct.pack('<i', pk) + write_index(on)
    export_off = len(out)
    for e, off in zip(exports, offsets):
        out += (write_index(e['class']) + write_index(e['super']) + struct.pack('<i', e['outer']) +
                write_index(e['name']) + struct.pack('<I', e['flags']) + write_index(len(e['data'])))
        if e['data']:
            out += write_index(off)
    if gens:
        gens[-1] = [len(exports), len(names)]
    struct.pack_into('<IHHIiiiiii', out, 0, TAG, ver, lic, pflags, len(names), name_off,
                     len(exports), export_off, len(imports), import_off)
    out[36:52] = guid
    struct.pack_into('<i', out, 52, gencount)
    for i, (gec, gnc) in enumerate(gens):
        struct.pack_into('<ii', out, 56 + 8 * i, gec, gnc)
    open(dst, 'wb').write(out)
    print('%s: %d bytes, %d names, %d imports, %d exports' % (dst, len(out), len(names), len(imports), len(exports)))


if __name__ == '__main__':
    main()
