#!/usr/bin/env python3
"""Writes the harness's own song package, which DXCapture.u loads into itself
(tools/dxcap.sh compile; docs/DEVELOPMENT.md, scripted runs): a song of steady
tones, of a kind the game ships none of, made here rather than kept as a file.

    testmusic.py <folder>

  CapTone.it    the song alone, for a MUSIC IMPORT should a package not do.
  CapMusic.umx  a package of version 68, the game's own, holding:
    CapTone     an Impulse Tracker module, as the game's songs are, of samples
                alone (no instruments): speed 6, tempo 125, linear slides,
                global volume 128, mix volume 48 (Title's and most stock
                songs'), every channel at volume 64, and four samples, each
                16-bit signed mono at a C5Speed of 44,100 Hz, at volume 64 and
                global volume 64, half of full scale, a tone of a whole
                number of frames a period, looped from frame 64 to its end
                for whole periods (Galaxy's loader fades in a sample's first
                32 frames), uncompressed (Galaxy's loader reads no IT 2.14
                compression). Each tone is a C-5 struck once with no volume
                column on row 0 of its order's pattern, which then jumps to an
                empty one jumping to itself, so the tone holds, unstruck, as
                long as its section plays:
      order 0   tone A, 441 Hz, channel 1, centred (pan 32)
      order 2   tone B, 630 Hz, channel 1, centred
      order 4   the pair: 551.25 Hz on channel 2, hard left (pan 0), and
                882 Hz on channel 3, hard right (pan 64)
                (MusicConsole; tools/dxcap/sound.py --music)

Galaxy's IT loader (Galaxy.dll 0x10628990) and OpenMPT both read it: the
header's orders, sample and pattern offsets in IT's order, IMPS samples, IT's
packed patterns; Galaxy reads neither the mix volume, the channel volumes nor
a sample's global volume, which OpenMPT applies (bugs.md AUD.10).
"""
import math
import os
import struct
import sys

RATE = 44100
LEVEL = 0.5          # of full scale
LOOP_START = 64      # past Galaxy's 32-frame fade-in

# (frequency, frames a period, periods in the loop)
TONES = [(441.0, 100, 44), (630.0, 70, 63), (551.25, 80, 55), (882.0, 50, 88)]


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


def sample_data(period, periods):
    frames = LOOP_START + period * periods
    return frames, struct.pack('<%dh' % frames, *(
        int(round(LEVEL * 32767 * math.sin(2 * math.pi * i / period))) for i in range(frames)))


def imps(name, frames, pointer):
    # IMPS: DOS name, 0, global volume 64, flags (data, 16-bit, loop), volume
    # 64, name, convert (signed), default pan off, length, loop, C5Speed,
    # no sustain loop, the data's place, no vibrato
    return (b'IMPS' + name.encode('latin-1')[:12].ljust(12, b'\0') + bytes([0, 64, 0x01 | 0x02 | 0x10, 64]) +
            name.encode('latin-1')[:26].ljust(26, b'\0') + bytes([0x01, 32]) +
            struct.pack('<IIIIIII', frames, LOOP_START, frames, RATE, 0, 0, pointer) + bytes(4))


def pattern(rows):
    """rows: {row: [(channel, note, instrument, command, parameter), ...]};
    note, instrument or command None when the cell has none."""
    data = bytearray()
    for r in range(64):
        for channel, note, instrument, command, parameter in rows.get(r, []):
            mask = (0x01 if note is not None else 0) | (0x02 if instrument is not None else 0) | \
                   (0x08 if command is not None else 0)
            data += bytes([0x80 | (channel + 1), mask])
            if note is not None:
                data.append(note)
            if instrument is not None:
                data.append(instrument)
            if command is not None:
                data += bytes([command, parameter])
        data.append(0)
    return struct.pack('<HH', len(data), 64) + bytes(4) + bytes(data)


C5 = 60          # IT's C-5
B = 2            # IT's command B, a jump to an order


def module():
    orders = [0, 1, 2, 3, 4, 5, 255]
    patterns = [
        pattern({0: [(0, C5, 1, None, 0)], 63: [(0, None, None, B, 1)]}),
        pattern({63: [(0, None, None, B, 1)]}),
        pattern({0: [(0, C5, 2, None, 0)], 63: [(0, None, None, B, 3)]}),
        pattern({63: [(0, None, None, B, 3)]}),
        pattern({0: [(1, C5, 3, None, 0), (2, C5, 4, None, 0)], 63: [(1, None, None, B, 5)]}),
        pattern({63: [(1, None, None, B, 5)]}),
    ]
    samples = [sample_data(period, periods) for _, period, periods in TONES]

    pan = bytearray([32 | 128] * 64)      # unused channels off
    pan[0], pan[1], pan[2] = 32, 0, 64
    volume = bytes([64] * 64)
    header = (b'IMPM' + b'CapTone'.ljust(26, b'\0') + struct.pack('<H', 0x1004) +
              struct.pack('<HHHHHHHH', len(orders), 0, len(samples), len(patterns), 0x0214, 0x0214,
                          0x0001 | 0x0008, 0) +
              bytes([128, 48, 6, 125, 128, 0]) + struct.pack('<HII', 0, 0, 0) + bytes(pan) + volume)
    assert len(header) == 0xC0

    at = len(header) + len(orders) + 4 * len(samples) + 4 * len(patterns)
    imps_at = []
    for _ in samples:
        imps_at.append(at)
        at += 0x50
    pattern_at = []
    for p in patterns:
        pattern_at.append(at)
        at += len(p)
    data_at = []
    for frames, data in samples:
        data_at.append(at)
        at += len(data)

    out = bytearray(header + bytes(orders))
    out += struct.pack('<%dI' % len(samples), *imps_at) + struct.pack('<%dI' % len(patterns), *pattern_at)
    for i, (frames, _) in enumerate(samples):
        out += imps('CapTone%d' % (i + 1), frames, data_at[i])
    for p in patterns:
        out += p
    for _, data in samples:
        out += data
    assert len(out) == at
    return bytes(out)


NAMES = ['None', 'Core', 'Engine', 'Package', 'Class', 'Music', 'CapTone', 'it']


def name(n):
    return compact(NAMES.index(n))


def package():
    # As the game's music packages: Engine (a package) and its class Music
    # imported; one Music export, its FileType it, its data a lazy array
    # (the place past it, then its count), as Engine's UMusic serializes it.
    imports = [('Core', 'Package', 0, 'Engine'), ('Core', 'Class', -1, 'Music')]
    song = module()
    header_size = 4 + 2 + 2 + 4 + 6 * 4 + 16 + 4 + 8
    name_table = b''.join(compact(len(n) + 1) + n.encode('latin-1') + b'\0' + struct.pack('<I', 0x70010) for n in NAMES)
    import_table = b''.join(name(cp) + name(cn) + struct.pack('<i', pk) + name(on) for cp, cn, pk, on in imports)
    name_at = header_size
    import_at = name_at + len(name_table)
    data_at = import_at + len(import_table)
    head = name('None') + name('it')
    count = compact(len(song))
    export_data = head + struct.pack('<i', data_at + len(head) + 4 + len(count) + len(song)) + count + song
    export_at = data_at + len(export_data)
    exports = [(-2, 'CapTone', 0xf0004, data_at, len(export_data))]
    export_table = b''.join(compact(cls) + compact(0) + struct.pack('<i', 0) + name(n) + struct.pack('<I', flags) +
                            compact(size) + compact(at) for cls, n, flags, at, size in exports)
    header = (struct.pack('<IHHI', 0x9e2a83c1, 68, 0, 1) +
              struct.pack('<iiiiii', len(NAMES), name_at, len(exports), export_at, len(imports), import_at) +
              bytes(range(16)) + struct.pack('<i', 1) + struct.pack('<ii', len(exports), len(NAMES)))
    assert len(header) == header_size
    return header + name_table + import_table + export_data + export_table


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, 'CapTone.it'), 'wb') as f:
        f.write(module())
    with open(os.path.join(folder, 'CapMusic.umx'), 'wb') as f:
        f.write(package())


if __name__ == '__main__':
    main()
