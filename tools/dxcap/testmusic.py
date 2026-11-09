#!/usr/bin/env python3
"""Writes the harness's own song package, which DXCapture.u loads into itself
(tools/dxcap.sh compile; docs/DEVELOPMENT.md, scripted runs): a song of steady
tones, of a kind the game ships none of, made here rather than kept as a file.

    testmusic.py <folder>

  CapTone.it, CapCommands.it  the songs alone, for a MUSIC IMPORT should a
                package not do.
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
      order 6   loud: tone A on channels 4 to 11 at once, centred, eight
                voices in phase, past full scale at the Music slider's 255
      order 8   slow: tone A, its row 0 setting speed 32 (A20) and tempo 33
                (T21 on channel 13), 2.42 s a row, no jump: a section asked
                as it plays starts as that row ends (Galaxy.dll 0x10616f29)
      order 9   surround: the pair's 551.25 Hz on channel 13, which the
                header pans as IT's surround (100), and its 882 Hz on channel
                14 with XB4: Galaxy plays a header's surround a quarter left
                and its 8xx's B4 a quarter right, each a surround, its right
                side inverted (0x10628990, 0x10616eb0)
      order 10  low: tone A at C-2, 55.125 Hz, each frame of the sample held
                for eight output frames unless read between them
      order 11  the pair centred by X40 (Galaxy's 8xx centre), no jump: the
                song's end after its 64 rows, then its loop to order 0, which
                Galaxy enters with each channel's own pan again
                (MusicConsole; tools/dxcap/sound.py --music)
    CapCommands an Impulse Tracker module of the same kind, of tone A's
                sample and a burst's (nine of tone A's periods, then
                silence, its loop over the silence, so a voice never ends),
                for the pattern commands Galaxy's player plays its own way,
                one order each, from order 0 (MusicConsole's Songs;
                tools/dxcap/commands.py):
      order 0   tone A struck on row 0, T1F on rows 0 to 31 and H1F on
                rows 32 to 63, at speed 6: Galaxy's loader keeps a T under
                0x20 and its player takes it for a vibrato as H (0x1062a167,
                0x10616eb0), its vibrato y/8 of a semitone deep, down
                first, none on a row's first tick
      order 1   the burst struck on row 0 with Q04, Q04 on rows 1 to 31 with
                no note, at speed 6: Galaxy's E9y moves the voice back to
                its start on each tick of a row that is a multiple of 4,
                tick 0 among them, so the bursts come 4 and 2 ticks apart
                in turn, where IT's count carried over the rows gives 4
      order 2   speed 32 (A20 on channel 2): tone A struck with SCF on rows
                0, 2 and 6 and with SC7 on row 4, a volume of 64 with no
                note on rows 1, 3, 5 and 7, and C00 on row 7, the song's
                end: Galaxy's ECx takes the note's volume to 0 past tick x,
                SCF from tick 16, and the next row's volume brings the note
                back
    CapWave     a Music of FileType wav, 2 s of tone A, 16-bit mono at
                22,050 Hz: none of Galaxy's music loaders takes a WAV, its MOD
                loader, the last, failing with 15 (Galaxy.dll 0x10618be0,
                0x106252f0), so the original logs glxLoadMusic(&Mem,
                GLX_LOADFROMMEMORY ) failed: 15 and plays none

Galaxy's IT loader (Galaxy.dll 0x10628990) and OpenMPT both read it: the
header's orders, sample and pattern offsets in IT's order, IMPS samples, IT's
packed patterns; Galaxy reads neither the mix volume, the channel volumes nor
a sample's global volume, which OpenMPT applies and DXEngine's rewrite of a
song takes out (DXEngine/Audio/GalaxyMusic.cpp).
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
        int(round(LEVEL * 32767 * math.sin(2 * math.pi * i / period))) for i in range(frames))), LOOP_START


BURST = 900          # the burst's frames, nine of tone A's periods
BURST_LOOP = 1000    # its silent loop's start, to its end at 2,000


def burst_data():
    frames = 2 * BURST_LOOP
    return frames, struct.pack('<%dh' % frames, *(
        int(round(LEVEL * 32767 * math.sin(2 * math.pi * i / 100))) if i < BURST else 0
        for i in range(frames))), BURST_LOOP


def imps(name, frames, loop_start, pointer):
    # IMPS: DOS name, 0, global volume 64, flags (data, 16-bit, loop), volume
    # 64, name, convert (signed), default pan off, length, loop, C5Speed,
    # no sustain loop, the data's place, no vibrato
    return (b'IMPS' + name.encode('latin-1')[:12].ljust(12, b'\0') + bytes([0, 64, 0x01 | 0x02 | 0x10, 64]) +
            name.encode('latin-1')[:26].ljust(26, b'\0') + bytes([0x01, 32]) +
            struct.pack('<IIIIIII', frames, loop_start, frames, RATE, 0, 0, pointer) + bytes(4))


def pattern(rows, count=64):
    """rows: {row: [(channel, note, instrument, command, parameter[, volume]),
    ...]}; note, instrument, command or volume None when the cell has none."""
    data = bytearray()
    for r in range(count):
        for cell in rows.get(r, []):
            channel, note, instrument, command, parameter = cell[:5]
            volume = cell[5] if len(cell) > 5 else None
            mask = (0x01 if note is not None else 0) | (0x02 if instrument is not None else 0) | \
                   (0x04 if volume is not None else 0) | (0x08 if command is not None else 0)
            data += bytes([0x80 | (channel + 1), mask])
            if note is not None:
                data.append(note)
            if instrument is not None:
                data.append(instrument)
            if volume is not None:
                data.append(volume)
            if command is not None:
                data += bytes([command, parameter])
        data.append(0)
    return struct.pack('<HH', len(data), count) + bytes(4) + bytes(data)


C5 = 60          # IT's C-5
C2 = 24          # IT's C-2, three octaves under
A = 1            # IT's command A, the speed
B = 2            # IT's command B, a jump to an order
C = 3            # IT's command C, a break to a row of the next order
T = 20           # IT's command T, the tempo
X = 24           # IT's command X, a pan, which Galaxy reads as the MOD's 8xx
H = 8            # IT's command H, a vibrato
Q = 17           # IT's command Q, a retrigger
S = 19           # IT's command S, SCx a note cut
LOUD = range(3, 11)  # the loud order's channels, 4 to 11
SURROUND, SIDE = 12, 13  # the surround order's channels, 13 and 14


def it(title, orders, patterns, samples, pan):
    """An IT module of samples alone: speed 6, tempo 125, linear slides,
    global volume 128, mix volume 48, every channel at volume 64; samples
    (frames, data, loop start) as sample_data gives them."""
    volume = bytes([64] * 64)
    header = (b'IMPM' + title.encode('latin-1').ljust(26, b'\0') + struct.pack('<H', 0x1004) +
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
    for frames, data, _ in samples:
        data_at.append(at)
        at += len(data)

    out = bytearray(header + bytes(orders))
    out += struct.pack('<%dI' % len(samples), *imps_at) + struct.pack('<%dI' % len(patterns), *pattern_at)
    for i, (frames, _, loop_start) in enumerate(samples):
        out += imps('%s%d' % (title, i + 1), frames, loop_start, data_at[i])
    for p in patterns:
        out += p
    for _, data, _ in samples:
        out += data
    assert len(out) == at
    return bytes(out)


def module():
    orders = [0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 8, 255]
    patterns = [
        pattern({0: [(0, C5, 1, None, 0)], 63: [(0, None, None, B, 1)]}),
        pattern({63: [(0, None, None, B, 1)]}),
        pattern({0: [(0, C5, 2, None, 0)], 63: [(0, None, None, B, 3)]}),
        pattern({63: [(0, None, None, B, 3)]}),
        pattern({0: [(1, C5, 3, None, 0), (2, C5, 4, None, 0)], 63: [(1, None, None, B, 5)]}),
        pattern({63: [(1, None, None, B, 5)]}),
        pattern({0: [(c, C5, 1, None, 0) for c in LOUD], 63: [(3, None, None, B, 7)]}),
        pattern({63: [(3, None, None, B, 7)]}),
        pattern({0: [(1, C5, 3, X, 0x40), (2, C5, 4, X, 0x40)]}),
        pattern({0: [(0, C5, 1, A, 0x20), (SURROUND, None, None, T, 0x21)]}),
        pattern({0: [(SURROUND, C5, 3, None, 0), (SIDE, C5, 4, X, 0xB4)], 63: [(SURROUND, None, None, B, 9)]}),
        pattern({0: [(0, C2, 1, None, 0)], 63: [(0, None, None, B, 10)]}),
    ]
    samples = [sample_data(period, periods) for _, period, periods in TONES]

    pan = bytearray([32 | 128] * 64)      # unused channels off
    pan[0], pan[1], pan[2] = 32, 0, 64
    for c in LOUD:
        pan[c] = 32
    pan[SURROUND], pan[SIDE] = 100, 32
    return it('CapTone', orders, patterns, samples, pan)


def commands():
    """CapCommands: the orders of the commands Galaxy plays its own way."""
    orders = [0, 1, 2, 255]
    tone, burst = 1, 2
    vibrato = {0: [(0, C5, tone, T, 0x1F)]}
    for r in range(1, 32):
        vibrato[r] = [(0, None, None, T, 0x1F)]
    for r in range(32, 64):
        vibrato[r] = [(0, None, None, H, 0x1F)]
    retrigger = {0: [(0, C5, burst, Q, 0x04)]}
    for r in range(1, 32):
        retrigger[r] = [(0, None, None, Q, 0x04)]
    cut = {0: [(0, C5, tone, S, 0xCF), (1, None, None, A, 0x20)], 2: [(0, C5, tone, S, 0xCF)],
           4: [(0, C5, tone, S, 0xC7)], 6: [(0, C5, tone, S, 0xCF)]}
    for r in (1, 3, 5, 7):
        cut[r] = [(0, None, None, None, 0, 64)]
    cut[7].append((1, None, None, C, 0x00))
    patterns = [pattern(vibrato), pattern(retrigger, 32), pattern(cut, 32)]
    samples = [sample_data(*TONES[0][1:]), burst_data()]
    pan = bytearray([32 | 128] * 64)
    pan[0], pan[1] = 32, 32
    return it('CapCommands', orders, patterns, samples, pan)


def wave():
    """CapWave's data: 2 s of tone A, a RIFF WAV of 16-bit mono samples."""
    rate, frames = 22050, 44100
    data = struct.pack('<%dh' % frames, *(int(round(LEVEL * 32767 * math.sin(2 * math.pi * 441.0 * i / rate)))
                                          for i in range(frames)))
    fmt = struct.pack('<HHIIHH', 1, 1, rate, 2 * rate, 2, 16)
    return (b'RIFF' + struct.pack('<I', 4 + 8 + len(fmt) + 8 + len(data)) + b'WAVE' +
            b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(data)) + data)


NAMES = ['None', 'Core', 'Engine', 'Package', 'Class', 'Music', 'CapTone', 'it', 'CapCommands', 'CapWave', 'wav']


def name(n):
    return compact(NAMES.index(n))


def package():
    # As the game's music packages: Engine (a package) and its class Music
    # imported; a Music export for each song, its FileType it (CapWave's
    # wav), its data a lazy array (the place past it, then its count), as
    # Engine's UMusic serializes it.
    imports = [('Core', 'Package', 0, 'Engine'), ('Core', 'Class', -1, 'Music')]
    songs = [('CapTone', 'it', module()), ('CapCommands', 'it', commands()), ('CapWave', 'wav', wave())]
    header_size = 4 + 2 + 2 + 4 + 6 * 4 + 16 + 4 + 8
    name_table = b''.join(compact(len(n) + 1) + n.encode('latin-1') + b'\0' + struct.pack('<I', 0x70010) for n in NAMES)
    import_table = b''.join(name(cp) + name(cn) + struct.pack('<i', pk) + name(on) for cp, cn, pk, on in imports)
    name_at = header_size
    import_at = name_at + len(name_table)
    at = import_at + len(import_table)
    export_data = b''
    exports = []
    for title, kind, song in songs:
        head = name('None') + name(kind)
        count = compact(len(song))
        data = head + struct.pack('<i', at + len(head) + 4 + len(count) + len(song)) + count + song
        exports.append((-2, title, 0xf0004, at, len(data)))
        export_data += data
        at += len(data)
    export_at = at
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
    with open(os.path.join(folder, 'CapCommands.it'), 'wb') as f:
        f.write(commands())
    with open(os.path.join(folder, 'CapMusic.umx'), 'wb') as f:
        f.write(package())


if __name__ == '__main__':
    main()
