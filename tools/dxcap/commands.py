#!/usr/bin/env python3
"""Reads the harness's CapCommands song (tools/dxcap/testmusic.py) as an
engine played it: a MusicConsole run with it in Songs, or a recording that
starts with it.

    commands.py <run dir>
    commands.py --wav <file.wav> [<seconds where the song starts>]

A run's audio.wav is laid against its log by MusicConsole's three beeps, and
the song is read from its onset after the log's "song DXCapture.CapCommands"
(its first two 5 ms frames 45 dB under full scale or louder), in its own
time from there, at its speed 6 and tempo 125 (a tick of 20 ms, a row of
120 ms):

  vibrato: tone A's frequency every 10 ms (zero crossings over 20 ms) on
           order 0's T1F rows (0.12 to 3.84 s) and H1F rows (3.96 to
           7.68 s): its lowest and highest, in Hz and semitones from 441,
           the time between the first two lows, and which way it goes first
           on the T1F rows. Galaxy's vibrato of T1F and H1F: 1.86 semitones
           each way (396 and 491 Hz), down first, a cycle of 1.536 s (64
           steps, five a row); IT's H1F 0.94 (417 and 466 Hz), up first, a
           cycle of 1.28 s; a T1F dropped 441 Hz throughout.
  retrigger: order 1's bursts (from 7.68 s): each one's start, where the
           burst rises out of 10 ms of quiet, and the time from the one
           before, ms. Galaxy's E9y: 80 and 40 ms in turn; IT's Q's count
           carried over the rows: 80.
  cut:     order 2's rows of 640 ms at speed 32 (from 11.52 s): how long
           tone A sounds in each, ms, a 5 ms frame within 20 dB of the
           song's level counted. Galaxy: rows 0, 2 and 6 (SCF) 320, row 4
           (SC7) 160, the rows after each (a volume of 64, no note) 640;
           SCF dropped 640 throughout, and IT's SC8 cut stopping the
           sample, row 5 silent.

Levels are dB of full scale.
"""
import array
import math
import re
import sys
import wave

HOP = 0.005
TICK = 2.5 / 125
ROW = 6 * TICK


def read(path):
    w = wave.open(path)
    if w.getsampwidth() != 2:
        sys.exit('%s: not 16-bit' % path)
    rate, ch = w.getframerate(), w.getnchannels()
    data = array.array('h', w.readframes(w.getnframes()))
    if sys.byteorder != 'little':
        data.byteswap()
    # Both channels' mean, the song's tones being centred
    mono = [sum(data[i:i + ch]) / (ch * 32768.0) for i in range(0, len(data) - ch + 1, ch)]
    return rate, mono


def envelope(rate, x):
    hop = int(rate * HOP)
    return [10 * math.log10(max(sum(v * v for v in x[i:i + hop]) / hop, 1e-12)) for i in range(0, len(x) - hop + 1, hop)]


def beep_offset(rate, x, beeps):
    """The offset (recording less log) that puts MusicConsole's first three
    beeps on onsets: the first three rises of 15 dB within 50 ms."""
    env = envelope(rate, x)
    rises = []
    i = 10
    while i < len(env) and len(rises) < 3:
        if env[i] - min(env[i - 10:i]) >= 15.0:
            rises.append(i * HOP)
            i += int(0.3 / HOP)
        else:
            i += 1
    if len(rises) < 3:
        sys.exit('the beeps are not in the recording')
    return sum(r - b for r, b in zip(rises, beeps)) / 3.0


def song_start(rate, x, after):
    env = envelope(rate, x)
    for i in range(max(0, int(after / HOP)), len(env) - 1):
        if env[i] > -45.0 and env[i + 1] > -45.0:
            # the first sample past a hundredth of full scale in that frame
            a = int(i * HOP * rate)
            for j in range(a, a + int(HOP * rate * 2)):
                if abs(x[j]) > 0.01:
                    return j / rate
            return i * HOP
    sys.exit('no song after %.2f s' % after)


def frequency(rate, x, t, span=0.02):
    """Hz from the rising zero crossings over span seconds from t."""
    a, b = int(t * rate), int((t + span) * rate)
    ups = [i + x[i - 1] / (x[i - 1] - x[i]) for i in range(max(a, 1), min(b, len(x))) if x[i - 1] < 0 <= x[i] and x[i] != x[i - 1]]
    if len(ups) < 3:
        return None
    return rate * (len(ups) - 1) / (ups[-1] - ups[0])


def semis(hz):
    return 12 * math.log2(hz / 441.0)


def vibrato(rate, x, start):
    print('vibrato: tone A every 10 ms, Hz (semitones from 441)')
    for name, a, b in (('T1F', 1 * ROW, 32 * ROW), ('H1F', 33 * ROW, 64 * ROW)):
        track = []
        t = a
        while t + 0.02 < b:
            f = frequency(rate, x, start + t)
            if f:
                track.append((t, f))
            t += 0.01
        if not track:
            print('  %s: no tone' % name)
            continue
        lo = min(f for _, f in track)
        hi = max(f for _, f in track)
        # Each run of frames half a semitone under 441 Hz, a row's first
        # tick at 441 not ending one, and the lowest frame of each
        groups = []
        for t, f in track:
            if f < 441.0 * 2 ** (-0.5 / 12):
                if groups and t - groups[-1][-1][0] < 0.2:
                    groups[-1].append((t, f))
                else:
                    groups.append([(t, f)])
        lows = [min(g, key=lambda p: p[1])[0] for g in groups]
        first = next(((t, f) for t, f in track if abs(semis(f)) > 0.25), None)
        way = 'none' if first is None else ('down' if first[1] < 441.0 else 'up')
        cycle = '%.3f s' % (lows[1] - lows[0]) if len(lows) > 1 else 'none'
        print('  %s: low %6.1f Hz (%+5.2f), high %6.1f Hz (%+5.2f), first %s, between lows %s' % (
            name, lo, semis(lo), hi, semis(hi), way, cycle))


def retrigger(rate, x, start):
    a = int((start + 64 * ROW) * rate)
    b = int((start + 96 * ROW) * rate)
    quiet = int(0.010 * rate)
    starts = []
    still = quiet
    for i in range(a, min(b, len(x))):
        if abs(x[i]) > 0.05:
            if still >= quiet:
                starts.append(i / rate - start - 64 * ROW)
            still = 0
        elif abs(x[i]) < 0.01:
            still += 1
    gaps = ['%.0f' % (1000 * (t1 - t0)) for t0, t1 in zip(starts, starts[1:])]
    print('retrigger: %d bursts, the first %.3f s into order 1; ms between: %s' % (len(starts), starts[0] if starts else 0, ' '.join(gaps[:16])))


def cut(rate, x, start):
    env = envelope(rate, x)
    base = start + 96 * ROW
    ref = sorted(env[int(start / HOP) + 4:int((start + 32 * ROW) / HOP)])
    level = ref[len(ref) // 2] if ref else -20.0
    rows = []
    for r in range(8):
        f0 = int((base + r * 32 * TICK) / HOP)
        f1 = int((base + (r + 1) * 32 * TICK) / HOP)
        rows.append('%.0f' % (1000 * HOP * sum(1 for e in env[f0:f1] if e > level - 20.0)))
    print('cut: the tone in each of order 2\'s rows, ms (the song at %.1f dB): %s' % (level, ' '.join(rows)))


def main():
    args = sys.argv[1:]
    if args and args[0] == '--wav':
        rate, x = read(args[1])
        start = song_start(rate, x, float(args[2]) if len(args) > 2 else 0.0)
    elif len(args) == 1:
        log = None
        for name in ('engine.log', 'DeusEx.log'):
            try:
                log = open('%s/%s' % (args[0], name), encoding='latin1').read()
                break
            except OSError:
                pass
        if log is None:
            sys.exit('%s: no engine.log or DeusEx.log' % args[0])
        evts = [(float(m.group(2)), m.group(1)) for m in re.finditer(r'DXMUSIC: (.*?) at (-?\d+\.\d+)', log)]
        beeps = [t for t, what in evts if what == 'beep'][:3]
        song = [t for t, what in evts if what == 'song DXCapture.CapCommands']
        if len(beeps) < 3 or not song:
            sys.exit('no beeps, or no "song DXCapture.CapCommands", in the log')
        rate, x = read('%s/audio.wav' % args[0])
        offset = beep_offset(rate, x, beeps)
        start = song_start(rate, x, song[0] + offset - 0.2)
        print('beeps at %+.3f s from the log; the song from %.3f s' % (offset, start))
    else:
        sys.exit(__doc__)
    vibrato(rate, x, start)
    retrigger(rate, x, start)
    cut(rate, x, start)


if __name__ == '__main__':
    main()
