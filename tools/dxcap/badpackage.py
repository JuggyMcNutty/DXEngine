#!/usr/bin/env python3
"""Writes a package that does not load as it is, for LocalMapConsole's starts
on a broken map or save (bugs.md ENG.28, PKG.32).

    badpackage.py <package> <out> [--cut <bytes>]
    badpackage.py <package> <out> --version <n>
    badpackage.py <package> <out> --rename <old> <new>
    badpackage.py <package> --imports

The copy is the package's bytes with its tag, the first four, zeroed: the
original's linker logs Core's BinaryFormat for it and fails the load with
Aborted (Core.dll 0x10160000), which the game's map load catches. With
--cut it is instead the first <bytes> bytes alone, the tag kept: a file cut
short within its header, which the original's reader stops the game on
(ReadFile beyond EOF), and DXEngine's too.

--version sets the file's version word (bytes 4 and 5), the rest of the file
as it is: 100 the original reads, no reader of its testing a version above
68, where DXEngine refuses it; 59 the original's reader asserts on
(InPos<=Size), DXEngine refuses it. --rename gives one name of the name
table another of the same length, so that no offset moves: MyLevel to
MyLevex makes a map with no MyLevel, an imported package's name (one of
--imports' "package" lines) a map importing a package that is not there,
each refused by the original's map check (StaticLoadObject of MyLevel,
Engine.dll 0x1038c6ab) and stopping DXEngine; a sound's name (Sink to Sinx)
a map both engines load. Only a package of version 64 or later is renamed.

A map: <out> in a folder of its own, named in DXCAP_MAPS, under a name of
its own (BadMap.dx, BadVer.dx, NoLevel.dx), so the game's own map is left
as it is. A save: <out> the level's file in the game's Save\\Save0009, a
copy of SaveConsole's save (its SaveInfo names the level,
01_NYC_UNATCOIsland.dxs for one made on the island); delete Save0009 after,
as SaveConsole's.
"""
import struct
import sys

TAG = b'\xc1\x83\x2a\x9e'


def read_index(b, p):
    b0 = b[p]
    p += 1
    v = b0 & 0x3F
    if b0 & 0x40:
        shift = 6
        while True:
            bx = b[p]
            p += 1
            v |= (bx & 0x7F) << shift
            shift += 7
            if not (bx & 0x80):
                break
    return (-v if b0 & 0x80 else v), p


def names(b):
    if struct.unpack_from('<H', b, 4)[0] < 64:
        sys.exit('only a package of version 64 or later is read here')
    count, offset = struct.unpack_from('<II', b, 12)
    out = []
    p = offset
    for _ in range(count):
        n, q = read_index(b, p)
        out.append((b[q:q + n - 1].decode('latin-1'), q, n - 1))
        p = q + n + 4
    return out


def imports(b, table):
    count, offset = struct.unpack_from('<II', b, 28)
    p = offset
    rows = []
    for _ in range(count):
        _, p = read_index(b, p)
        cn, p = read_index(b, p)
        outer = struct.unpack_from('<i', b, p)[0]
        p += 4
        on, p = read_index(b, p)
        rows.append((table[cn][0], outer, table[on][0]))
    return rows


def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[1] == '--imports':
        b = open(args[0], 'rb').read()
        for cls, outer, name in imports(b, names(b)):
            print(('package ' if cls == 'Package' and outer == 0 else cls + ' ') + name)
        return
    cut = version = rename = None
    if '--cut' in args:
        i = args.index('--cut')
        if i + 1 >= len(args):
            sys.exit(__doc__)
        cut = int(args[i + 1])
        del args[i:i + 2]
    elif '--version' in args:
        i = args.index('--version')
        if i + 1 >= len(args):
            sys.exit(__doc__)
        version = int(args[i + 1])
        del args[i:i + 2]
    elif '--rename' in args:
        i = args.index('--rename')
        if i + 2 >= len(args):
            sys.exit(__doc__)
        rename = (args[i + 1], args[i + 2])
        del args[i:i + 3]
    if len(args) != 2:
        sys.exit(__doc__)
    src, out = args
    b = bytearray(open(src, 'rb').read())
    if b[:4] != TAG:
        sys.exit('%s: not a package' % src)
    if cut is not None:
        b = b[:cut]
        what = 'cut'
    elif version is not None:
        struct.pack_into('<H', b, 4, version)
        what = 'version %d' % version
    elif rename is not None:
        old, new = rename
        if len(old) != len(new):
            sys.exit('the new name must be as long as the old')
        hits = [(q, n) for (s, q, n) in names(bytes(b)) if s == old]
        if len(hits) != 1:
            sys.exit('%s: %d names %s' % (src, len(hits), old))
        q, n = hits[0]
        b[q:q + n] = new.encode('latin-1')
        what = '%s renamed %s' % (old, new)
    else:
        b[:4] = b'\0\0\0\0'
        what = 'its tag zeroed'
    with open(out, 'wb') as f:
        f.write(b)
    print('%s: %d bytes, %s' % (out, len(b), what))


if __name__ == '__main__':
    main()
