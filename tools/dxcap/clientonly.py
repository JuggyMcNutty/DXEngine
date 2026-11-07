#!/usr/bin/env python3
"""Writes a copy of a map with one actor made client-only, for ClientOnlyConsole
and ClientOnlyServer (bugs.md ENG.23): no package of the game's has a
client-only export, whose flags hold LoadForClient but not LoadForServer.

    clientonly.py <map> <out> <actor>

The copy is the map's bytes with the named actor's export flags changed in
place, LoadForServer (0x20000) cleared and NotForServer (0x200000) set;
nothing else moves, the package's GUID kept. Its old and new flags are
printed. Put <out> in a folder of its own and name that folder in
DXCAP_MAPS, both engines then finding the map by its file's name.
"""
import struct
import sys

TAG = 0x9E2A83C1
LOAD_FOR_SERVER = 0x00020000
NOT_FOR_SERVER = 0x00200000


def read_index(b, p):
    """A compact index: sign and six bits, then seven bits a byte."""
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


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    src, out, actor = sys.argv[1:]
    b = bytearray(open(src, 'rb').read())
    tag, ver, _lic, _flags, ncount, noff, ecount, eoff, _icount, _ioff = struct.unpack_from('<IHHIiiiiii', b, 0)
    if tag != TAG:
        sys.exit('%s: not a package' % src)
    names = []
    p = noff
    for _ in range(ncount):
        if ver >= 64:
            n, p = read_index(b, p)
            names.append(bytes(b[p:p + n - 1]).decode('latin-1'))
            p += n
        else:
            e = b.index(b'\0', p)
            names.append(bytes(b[p:e]).decode('latin-1'))
            p = e + 1
        p += 4
    found = []
    p = eoff
    for _ in range(ecount):
        cls, p = read_index(b, p)
        _super, p = read_index(b, p)
        p += 4                      # the outer, an int
        name, p = read_index(b, p)
        at = p                      # the flags, four bytes
        p += 4
        size, p = read_index(b, p)
        if size > 0:
            _off, p = read_index(b, p)
        if cls != 0 and names[name].lower() == actor.lower():
            found.append(at)
    if len(found) != 1:
        sys.exit('%s: %d exports named %s, not one' % (src, len(found), actor))
    at = found[0]
    old, = struct.unpack_from('<I', b, at)
    new = (old & ~LOAD_FOR_SERVER) | NOT_FOR_SERVER
    struct.pack_into('<I', b, at, new)
    open(out, 'wb').write(b)
    print('%s: %s flags 0x%x -> 0x%x' % (out, actor, old, new))


if __name__ == '__main__':
    main()
