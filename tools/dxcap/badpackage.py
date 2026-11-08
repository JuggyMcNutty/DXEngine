#!/usr/bin/env python3
"""Writes a package that does not load, for LocalMapConsole's starts on a
broken map or save (bugs.md ENG.28).

    badpackage.py <package> <out> [--cut <bytes>]

The copy is the package's bytes with its tag, the first four, zeroed: the
original's linker logs Core's BinaryFormat for it and fails the load with
Aborted (Core.dll 0x10160000), which the game's map load catches. With
--cut it is instead the first <bytes> bytes alone, the tag kept: a file cut
short within its header, which the original's reader stops the game on
(ReadFile beyond EOF), and DXEngine's too.

A map: <out> in a folder of its own, named in DXCAP_MAPS, under a name of
its own (BadMap.dx), so the game's own map is left as it is. A save: <out>
the level's file in the game's Save\\Save0009, a copy of SaveConsole's save
(its SaveInfo names the level, 01_NYC_UNATCOIsland.dxs for one made on the
island); delete Save0009 after, as SaveConsole's.
"""
import sys

TAG = b'\xc1\x83\x2a\x9e'


def main():
    args = sys.argv[1:]
    cut = None
    if '--cut' in args:
        i = args.index('--cut')
        if i + 1 >= len(args):
            sys.exit(__doc__)
        cut = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    src, out = args
    b = bytearray(open(src, 'rb').read())
    if b[:4] != TAG:
        sys.exit('%s: not a package' % src)
    if cut is not None:
        b = b[:cut]
    else:
        b[:4] = b'\0\0\0\0'
    with open(out, 'wb') as f:
        f.write(b)
    print('%s: %d bytes%s' % (out, len(b), ', cut' if cut is not None else ', its tag zeroed'))


if __name__ == '__main__':
    main()
