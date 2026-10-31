#!/usr/bin/env python3
"""Writes the harness's own test sounds, which DXCapture.u imports (tools/dxcap.sh
compile; docs/DEVELOPMENT.md, scripted runs): sounds of kinds the game ships none of,
made here rather than kept as files.

    testsounds.py <folder>

  CapPingPong.wav  16-bit mono, 22,050 Hz, 1 s: a 500 Hz tone, its loop from
                   0.2 s to 0.7 s a ping-pong one (the smpl chunk's type 1), its
                   level rising through the loop from 0.1 to 0.9 of full scale,
                   so a forward loop's level climbs and drops and a ping-pong
                   one's climbs and falls.
  CapLoop.wav      the same with a forward loop (type 0).
  CapEarlyLoop.wav the forward one with its smpl chunk before its data chunk.
  Cap8Bit.wav      8-bit mono, 22,050 Hz, 2 s: 0.06 s of a 250 Hz tone, then a
                   1,000 Hz one, each at 0.8 of full scale.
  CapStereo.wav    16-bit stereo, 22,050 Hz, 2 s: eight tones of 0.25 s each,
                   300 to 1,000 Hz in steps of 100, the right side a fifth over
                   the left, at 0.5 of full scale.
"""
import math
import os
import struct
import sys

RATE = 22050


def riff(chunks):
    body = b'WAVE' + b''.join(struct.pack('<4sI', cid, len(data)) + data + (b'\0' if len(data) & 1 else b'')
                             for cid, data in chunks)
    return b'RIFF' + struct.pack('<I', len(body)) + body


def fmt(channels, bits):
    align = channels * bits // 8
    return struct.pack('<HHIIHH', 1, channels, RATE, RATE * align, align, bits)


def smpl(start, end, kind):
    # Manufacturer to SMPTE offset, the loop count, the sampler data; then the
    # loop: its cue ID, type, first and last frame, fraction and play count.
    return struct.pack('<9I', 0, 0, 1000000000 // RATE, 60, 0, 0, 0, 1, 0) + struct.pack('<6I', 0, kind, start, end, 0, 0)


def tone(hz, at, level):
    return level * math.sin(2 * math.pi * hz * at / RATE)


def looped(kind, early=False):
    frames = RATE
    start, end = int(0.2 * RATE), int(0.7 * RATE)
    out = []
    for i in range(frames):
        if i < start:
            level = 0.1
        elif i <= end:
            level = 0.1 + 0.8 * (i - start) / (end - start)
        else:
            level = 0.0
        out.append(int(round(32767 * tone(500, i, level))))
    data = struct.pack('<%dh' % frames, *out)
    chunks = [(b'fmt ', fmt(1, 16)), (b'data', data), (b'smpl', smpl(start, end, kind))]
    if early:
        chunks[1], chunks[2] = chunks[2], chunks[1]
    return riff(chunks)


def eightbit():
    frames = 2 * RATE
    first = int(0.06 * RATE)
    out = bytes(128 + int(round(127 * tone(250 if i < first else 1000, i, 0.8)))
                for i in range(frames))
    return riff([(b'fmt ', fmt(1, 8)), (b'data', out)])


def stereo():
    frames = 2 * RATE
    out = []
    for i in range(frames):
        hz = 300 + 100 * (i // (RATE // 4))
        out += [int(round(32767 * tone(hz, i, 0.5))), int(round(32767 * tone(hz * 1.5, i, 0.5)))]
    return riff([(b'fmt ', fmt(2, 16)), (b'data', struct.pack('<%dh' % len(out), *out))])


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    for name, make in (('CapPingPong.wav', lambda: looped(1)), ('CapLoop.wav', lambda: looped(0)),
                       ('CapEarlyLoop.wav', lambda: looped(0, True)), ('Cap8Bit.wav', eightbit),
                       ('CapStereo.wav', stereo)):
        with open(os.path.join(folder, name), 'wb') as f:
            f.write(make())


if __name__ == '__main__':
    main()
