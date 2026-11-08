#!/usr/bin/env python3
"""Writes a copy of a version-68 package that says it is of an older
version, 60 to 67, read the same by the original's serializers that apply
to it, for OldVerConsole (bugs.md PKG.40).

    vergen.py <package> <out> --version <n>

The copy keeps every byte of the source and appends what the older version
reads elsewhere, so that no offset moves:
  - its summary's GUID and generations (bytes 36 on) become a heritage
    count of 1 and the offset of the GUID, appended (Core.dll's summary
    reader 0x1015f710 reads a heritage below 68, its last GUID the
    package's, one generation of its counts);
  - below 64, its name table appended as NUL-ended 8-bit names with their
    flags, the summary's name offset pointing there (FNameEntry's reader
    0x1014e260: 8-bit names below 64, an FString from 64).
What the version changes in the exports is left as the source has it: a
sound's, a song's and each mip's skip offset, which the original reads from
62 on (the lazy array's reader, Engine.dll 0x10371140, from USound 0x1036f900,
UMusic 0x10370bd0 and FMipmap 0x103f0d80), so a 62 copy of a sound or
texture package loads in the original as its source does. Not for a package
with script code (UFunction reads three more fields up to 63, 0x10129010),
nor a mesh or a model (their readers test 61, 65 and 66).

Name the copy for its own package (CapStucco62.utx), in a folder of its own
that a run finds by DXCAP_SERVERPKGS.
"""
import struct
import sys

TAG = 0x9E2A83C1


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


def main():
    args = sys.argv[1:]
    if len(args) != 4 or args[2] != '--version':
        sys.exit(__doc__)
    src, out, version = args[0], args[1], int(args[3])
    if not 60 <= version <= 67:
        sys.exit('a version from 60 to 67')
    b = bytearray(open(src, 'rb').read())
    tag, ver, lic = struct.unpack_from('<IHH', b, 0)
    if tag != TAG or ver != 68:
        sys.exit('%s: not a version-68 package' % src)
    name_count, name_offset = struct.unpack_from('<II', b, 12)
    guid = bytes(b[36:52])

    # The name table, read as version 68 has it (an FString, then flags)
    names = []
    p = name_offset
    for _ in range(name_count):
        n, p = read_index(b, p)
        if n < 0:
            sys.exit('a 16-bit name: not written below 64')
        text = bytes(b[p:p + n - 1])
        p += n
        flags = struct.unpack_from('<I', b, p)[0]
        p += 4
        names.append((text, flags))

    tail = bytearray()
    heritage_offset = len(b)
    tail += guid
    new_name_offset = name_offset
    if version < 64:
        new_name_offset = len(b) + len(tail)
        for text, flags in names:
            if b'\0' in text:
                sys.exit('a name with a NUL')
            tail += text + b'\0' + struct.pack('<I', flags)

    struct.pack_into('<H', b, 4, version)
    struct.pack_into('<II', b, 12, name_count, new_name_offset)
    struct.pack_into('<II', b, 36, 1, heritage_offset)
    b += tail
    with open(out, 'wb') as f:
        f.write(b)
    print('%s: %d bytes, version %d, %d names%s' % (
        out, len(b), version, name_count,
        ', 8-bit at %d' % new_name_offset if version < 64 else ''))


if __name__ == '__main__':
    main()
