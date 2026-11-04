#!/usr/bin/env python3
"""Reads a SoundConsole run: the recording against the moves in its log.

    sound.py <run dir>
    sound.py --onsets <run dir>
    sound.py --levels <run dir>
    sound.py --music <run dir>

The run's audio.wav is laid against its log (engine.log or DeusEx.log) by
each scenario's three beeps, half a second apart, which SoundConsole plays
before the scenario starts: the console's clock and the recording's differ by
the time a map takes to load, so each scenario is laid by its own beeps.

  wall:   the level and the brightness of the ambient sound heard from the
          open spot and from the spot behind a wall;
  hear:   each probe of the hearing test, the wall's source playing a beep:
          heard or not, and its peak against the first probe's (ref);
  reverb: for each shot in the reverb zone and outside it, its peak, how long
          it rings (until 60 dB under its peak), and its tail: the energy
          from 0.5 to 1.5 s after the peak against the first 0.45 s, the
          shot's own length -- what the zone adds;
  pan:    each channel's peak for beeps from a path node to the right, to
          the left and ahead, and for the root window's beeps from the
          screen's left edge, its centre, its right edge and at its own
          volume.

--onsets reads any recording alone: each sound that starts out of quiet,
its time and peak, and the longest run of them a second apart -- a net
test's client hearing the server's beeps (ServeConsole).

--levels reads any recording alone too: its level every 0.1 s, the median
over the tenth -- the music through a map's load, against the beeps'
onsets (LoadingConsole, with DXCAP_MUSIC=1).

--music reads a MusicConsole run (DXCAP_RECORD=1 DXCAP_MUSIC=1), laid by
its three beeps, each step by the beep a second before it:

  segment: the tone's level at each Music slider -- the median of its 5 ms
           frames, and the tone's own by a windowed DFT at its frequency --
           against the first at 255, beside Galaxy's square of the slider
           and a linear law;
  fade:    each fade's 5 ms frames against the level before it: when it
           passes 3, 6, 12 and 24 dB down, the steps between those (Galaxy's
           square and a linear fade over the time and twice Latency, 40 ms,
           beside them), when it falls silent (60 dB down), how long it is
           silent, the old tone's frames at 20 dB down or louder after the
           24 dB point (a burst) and when the new tone starts, each frame
           told by its brightness, a pure tone's own frequency;
  stereo:  the pair's tones in each channel by the DFT, and each one's
           level in the other side's channel (crossfeed): first as the song
           loads straight into the pair's order, then after order 0 has
           played.

Levels are dB of full scale. Brightness is the frequency of a sine with the
same share of first-difference energy -- a low-pass filter lowers it.
"""
import array
import math
import os
import re
import sys
import wave

HOP = 0.005  # seconds per envelope frame


class Recording:
    def __init__(self, path):
        w = wave.open(path)
        if w.getsampwidth() != 2:
            sys.exit('%s: not 16-bit' % path)
        self.rate = w.getframerate()
        self.channels = w.getnchannels()
        data = array.array('h', w.readframes(w.getnframes()))
        if sys.byteorder != 'little':
            data.byteswap()
        self.data = data
        self.frames = len(data) // self.channels
        self.hop = int(self.rate * HOP)
        # A frame's own length: 220 frames at 44,100 Hz are 4.989 ms, so a
        # time taken as 5 ms frames is 113 ms late at 50 s
        self.step = self.hop / self.rate
        # The energy envelope, both channels together, and each channel's.
        env = []
        per = [[] for _ in range(self.channels)]
        step = self.hop * self.channels
        for i in range(0, len(data) - step + 1, step):
            chunk = data[i:i + step]
            env.append(sum(x * x for x in chunk) / len(chunk))
            for c in range(self.channels):
                one = chunk[c::self.channels]
                per[c].append(sum(x * x for x in one) / len(one))
        self.env = env
        self.per = per

    def channel_peaks(self, start, end):
        """Each channel's loudest frame (dB) between two recording times."""
        a, b = max(0, self.frame(start)), self.frame(end)
        return [self.db(max(env[a:b], default=0.0)) for env in self.per]

    def db(self, energy):
        return 10 * math.log10(max(energy, 1e-3) / (32768.0 * 32768.0))

    def frame(self, t):
        return int(round(t / self.step))

    def level(self, start, end):
        """Median level (dB) and brightness (Hz) between two recording times."""
        frames = sorted(self.env[max(0, self.frame(start)):self.frame(end)])
        a = max(0, int(start * self.rate)) * self.channels
        b = min(self.frames, int(end * self.rate)) * self.channels
        c = self.channels
        chunk = self.data[a:b]
        if not frames or len(chunk) <= 2 * c:
            return None, None
        e = sum(x * x for x in chunk)
        d = sum((chunk[i] - chunk[i - c]) ** 2 for i in range(c, len(chunk)))
        ratio = min(4.0, d / max(e, 1.0))
        bright = self.rate / math.pi * math.asin(math.sqrt(ratio) / 2)
        return self.db(frames[len(frames) // 2]), bright


def read_log(run):
    for name in ('engine.log', 'DeusEx.log'):
        path = os.path.join(run, name)
        if os.path.exists(path):
            return open(path, encoding='latin1').read()
    sys.exit('%s: no engine.log or DeusEx.log' % run)


def events(log, tag='DXCAP'):
    """(time, what) for each timed line of the tag's, in order."""
    out = []
    for m in re.finditer(re.escape(tag) + r': (.*?) at (-?\d+\.\d+)', log):
        out.append((float(m.group(2)), m.group(1)))
    return out


def onsets(rec):
    """Onset strength per frame: its rise over the lowest of the 50 ms before."""
    dbs = [rec.db(e) for e in rec.env]
    out = [0.0] * len(dbs)
    for i in range(10, len(dbs)):
        out[i] = max(0.0, dbs[i] - min(dbs[i - 10:i]))
    return out


def align(rec, strength, beeps, lo, hi):
    """The recording time of console time 0 that puts all the beeps on onsets.

    An offset scores its weakest beep's onset, so a single loud onset -- the
    sound starting out of silence -- cannot stand in for three beeps.
    """
    best, best_offset = -1.0, None
    steps = int((hi - lo) / HOP)
    for s in range(steps):
        offset = lo + s * HOP
        score = None
        for t in beeps:
            f = rec.frame(t + offset)
            if f < 3 or f + 3 >= len(strength):
                score = -1.0
                break
            onset = max(strength[f - 3:f + 4])
            score = onset if score is None else min(score, onset)
        if score is not None and score > best:
            best, best_offset = score, offset
    return best_offset, best


def scenario(evts, first, last):
    """The events from the one whose text starts with first to last's."""
    out, on = [], False
    for t, what in evts:
        if what.startswith(first):
            on = True
        if on:
            out.append((t, what))
            if what.startswith(last):
                break
    return out


def beeps_after(evts, t0):
    b = [t for t, what in evts if what == 'beep' and t >= t0]
    return b[:3]


def report_wall(rec, strength, evts):
    """Returns the scenario's offset, where the next one's search starts."""
    wall = scenario(evts, 'open', 'wall scenario ends')
    if not wall:
        print('wall: not in the log')
        return -5.0
    beeps = beeps_after(wall, wall[0][0])
    # The recording starts a few seconds before the level (6 s at most so
    # far): a wider search can land on a louder triple of onsets further on.
    offset, score = align(rec, strength, beeps, -5.0, 15.0)
    print('wall: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    moves = [(t, what) for t, what in wall if what in ('open', 'wall', 'wall scenario ends')]
    results = {'open': [], 'wall': []}
    for (t, what), (t_next, _) in zip(moves, moves[1:]):
        start = t + 0.5
        if beeps and start < beeps[-1] + 0.5 and t_next > beeps[-1]:
            start = beeps[-1] + 0.5
        lvl, bright = rec.level(start + offset, t_next - 0.2 + offset)
        if lvl is None:
            continue
        results[what].append((lvl, bright))
        print('  %-4s %6.2f-%6.2f s  %6.1f dB  %5.0f Hz' % (what, start, t_next - 0.2, lvl, bright))
    if results['open'] and results['wall']:
        mean = lambda xs: sum(xs) / len(xs)
        o = mean([l for l, _ in results['open']])
        w = mean([l for l, _ in results['wall']])
        ob = mean([b for _, b in results['open']])
        wb = mean([b for _, b in results['wall']])
        print('  behind the wall: %+.1f dB, brightness %.0f -> %.0f Hz' % (w - o, ob, wb))
    return offset


def report_hear(rec, strength, evts, after):
    """after: the wall scenario's offset; the hearing test follows it on the
    same map, so its own beeps are looked for close to it."""
    hear = scenario(evts, 'hear scenario', 'hear scenario ends')
    if not hear:
        print('hear: not in the log')
        return
    beeps = beeps_after(hear, hear[0][0])
    offset, score = align(rec, strength, beeps, after - 1.0, after + 5.0)
    print('hear: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    ref = None
    for t, what in hear:
        if not what.startswith('hear ') or what.startswith('hear scenario') or what == 'hear wall':
            continue
        label = what[5:]
        # The probe's peak against the quiet before it, the median of the
        # half second before: heard when it stands 10 dB over that.
        start = t + offset
        before = sorted(rec.env[max(0, rec.frame(start - 0.6)):max(0, rec.frame(start - 0.1))])
        floor = rec.db(before[len(before) // 2]) if before else None
        peak = rec.db(max(rec.env[max(0, rec.frame(start - 0.05)):rec.frame(start + 0.5)], default=0.0))
        heard = floor is not None and peak > floor + 10.0
        if label == 'ref' and heard:
            ref = peak
        rel = '%+6.1f dB' % (peak - ref) if heard and ref is not None else ''
        print('  %-24s %6.2f s  peak %6.1f dB  floor %6.1f dB  %-6s %s' % (
            label, t, peak, floor if floor is not None else float('nan'), 'heard' if heard else 'silent', rel))


def report_onsets(rec, strength):
    """Each sound starting out of quiet (a rise of 15 dB within 50 ms), its
    time and peak, and the longest run of them 0.9 to 1.1 s apart."""
    found = []
    f = 0
    while f < len(strength):
        if strength[f] >= 15.0:
            top = max(range(f, min(len(strength), f + rec.frame(0.3))), key=lambda i: rec.env[i])
            found.append((f * rec.step, rec.db(rec.env[top])))
            f += rec.frame(0.3)
        else:
            f += 1
    print('onsets: %d' % len(found))
    for t, peak in found:
        print('  %7.2f s  peak %6.1f dB' % (t, peak))
    best = []
    for i in range(len(found)):
        run = [found[i][0]]
        for t, _ in found[i + 1:]:
            if 0.9 <= t - run[-1] <= 1.1:
                run.append(t)
        if len(run) > len(best):
            best = run
    if best:
        print('longest run a second apart: %d, from %.2f s' % (len(best), best[0]))


def report_levels(rec):
    """The level every 0.1 s: the median over each tenth (Recording.level)."""
    print('levels: the median of each 0.1 s, dB')
    tenths = int(rec.frames / rec.rate * 10)
    for i in range(tenths):
        lvl, _ = rec.level(i / 10.0, (i + 1) / 10.0)
        if lvl is not None:
            print('  %7.1f s  %6.1f dB' % (i / 10.0, lvl))


def shot(rec, t):
    """Peak (dB), ring (s) and tail (dB) of a shot fired at recording time t."""
    env = rec.env
    f0 = rec.frame(t - 0.05)
    f1 = rec.frame(t + 0.3)
    if f0 < 0 or f1 + rec.frame(2.8) >= len(env):
        return None, None, None
    peak_f = max(range(f0, f1), key=lambda f: env[f])
    peak = rec.db(env[peak_f])
    # Up to 2.8 s: the next shot comes 3 s after.
    ring = max((f for f in range(peak_f, peak_f + rec.frame(2.8)) if rec.db(env[f]) > peak - 60), default=peak_f)
    body = sum(env[peak_f:peak_f + rec.frame(0.45)])
    tail = sum(env[peak_f + rec.frame(0.5):peak_f + rec.frame(1.5)])
    return peak, (ring - peak_f) * rec.step, 10 * math.log10(max(tail, 1e-3) / max(body, 1e-3))


def report_reverb(rec, strength, evts, after):
    """after: the earlier scenario's offset; a map's load only adds to it.

    Returns the scenario's offset, which the pan that follows it shares."""
    rev = scenario(evts, 'in the zone', 'shot outside')
    rest = [(t, w) for t, w in evts if rev and t >= rev[0][0]]
    if not rev:
        print('reverb: not in the log')
        return None, []
    beeps = beeps_after(rest, rev[0][0])
    offset, score = align(rec, strength, beeps, after - 0.5, after + 20.0)
    print('reverb: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    results = {'in': [], 'out': []}
    for t, what in rest:
        if not what.startswith('shot'):
            continue
        where = 'in' if 'in the zone' in what else 'out'
        peak, ring, tail = shot(rec, t + offset)
        if peak is None:
            print('  %-3s %6.2f s  not in the recording' % (where, t))
            continue
        results[where].append((peak, ring, tail))
        print('  %-3s %6.2f s  peak %6.1f dB  rings %.2f s  tail %+6.1f dB' % (where, t, peak, ring, tail))
    if results['in'] and results['out']:
        # Medians: a stray sound can fall in one shot's window.
        median = lambda xs: sorted(xs)[len(xs) // 2]
        i = [median([r[k] for r in results['in']]) for k in range(3)]
        o = [median([r[k] for r in results['out']]) for k in range(3)]
        print('  in the zone: peak %.1f dB, rings %.2f s, tail %.1f dB' % tuple(i))
        print('  outside:     peak %.1f dB, rings %.2f s, tail %.1f dB' % tuple(o))
    return offset, rest


def report_pan(rec, offset, rest):
    pans = [(t, what) for t, what in rest if what.startswith('pan ') or what.startswith('window ')]
    if offset is None or not pans:
        print('pan: not in the log')
        return
    print('pan: left and right channel peaks')
    for t, what in pans:
        peaks = rec.channel_peaks(t + offset - 0.05, t + offset + 0.4)
        if len(peaks) != 2:
            print('  %-19s not stereo' % what)
            continue
        print('  %-19s %6.2f s  L %6.1f dB  R %6.1f dB  R-L %+5.1f dB' % (
            what, t, peaks[0], peaks[1], peaks[1] - peaks[0]))


# MusicConsole's song, tools/dxcap/testmusic.py's CapTone: its tones, and the
# pair's, hard left and hard right.
MUSIC_TONES = {'A': 441.0, 'B': 630.0}
MUSIC_PAIR = (551.25, 882.0)
MUSIC_LATENCY = 0.040  # the game's Latency, whose twice the fade's hold is


def galaxy_music_gain(slider, fade=1.0):
    """Galaxy's music gain (SetVolumes and its mixer, a music voice's own
    volume the 100 a song's load leaves it at; bugs.md AUD.9)."""
    v = int(max(fade, 0.0) * min(max(slider * 0.00392156862745098, 0.0), 1.0) * 127.0)
    g = (2 * v * v * 20000) >> 15
    for _ in range(4):
        g = (0x7fff * g) >> 15
    return g / 32768.0


def frame_brightness(rec, f):
    """A 5 ms frame's brightness (Hz), both channels: a pure tone's own
    frequency."""
    c = rec.channels
    a = f * rec.hop * c
    chunk = rec.data[a:a + rec.hop * c]
    e = sum(x * x for x in chunk)
    d = sum((chunk[i] - chunk[i - c]) ** 2 for i in range(c, len(chunk)))
    if e <= 0:
        return 0.0
    return rec.rate / math.pi * math.asin(math.sqrt(min(4.0, d / e)) / 2)


def tone_db(rec, start, end, hz, channel=None):
    """A tone's level (dB, as an RMS) between two recording times by a
    Hann-windowed DFT at its frequency, other sounds -- the beeps, at 4 kHz,
    the other tones -- left out; channel None, both channels' mean."""
    a = max(0, int(start * rec.rate))
    b = min(rec.frames, int(end * rec.rate))
    n = b - a
    if n < 256:
        return None
    coeff = 2 * math.cos(2 * math.pi * hz / rec.rate)
    window = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    energy = []
    for c in (range(rec.channels) if channel is None else [channel]):
        s1 = s2 = 0.0
        data = rec.data[a * rec.channels + c:b * rec.channels:rec.channels]
        for x, w in zip(data, window):
            s1, s2 = x * w + coeff * s1 - s2, s1
        power = max(s1 * s1 + s2 * s2 - coeff * s1 * s2, 0.0)
        peak = 2 * math.sqrt(power) / ((n - 1) / 2.0)
        energy.append(peak * peak / 2)
    return rec.db(sum(energy) / len(energy))


def hf_strength(rec):
    """Onset strength of the first differences' energy, which holds the
    beeps (4 kHz) some 19 dB further over the songs' tones than the energy."""
    c = rec.channels
    step = rec.hop * c
    dbs = []
    for i in range(0, len(rec.data) - step + 1, step):
        chunk = rec.data[i:i + step]
        d = sum((chunk[k] - chunk[k - c]) ** 2 for k in range(c, len(chunk)))
        dbs.append(rec.db(d / len(chunk)))
    out = [0.0] * len(dbs)
    for i in range(10, len(dbs)):
        out[i] = max(0.0, dbs[i] - min(dbs[i - 10:i]))
    return out


def beep_offset(rec, hf, t, around):
    """The offset laying a beep logged at t on its onset, within 0.25 s of
    around, the run's offset."""
    best, best_offset = -1.0, around
    for s in range(-50, 51):
        offset = around + s * HOP
        f = rec.frame(t + offset)
        if 3 <= f < len(hf) - 3:
            onset = max(hf[f - 2:f + 3])
            if onset > best:
                best, best_offset = onset, offset
    return best_offset


def report_music(rec, log):
    evts = events(log, 'DXMUSIC')
    beeps = [t for t, what in evts if what == 'beep']
    if len(beeps) < 3:
        print('music: not in the log')
        return
    offset, score = align(rec, onsets(rec), beeps[:3], -5.0, 60.0)
    print('music: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    hf = hf_strength(rec)

    def local(t):
        """The offset by the beep a second before t, or the run's."""
        before = [b for b in beeps if t - 1.3 <= b < t]
        return beep_offset(rec, hf, before[-1], offset) if before else offset

    steps = [(t, what.split()) for t, what in evts if what.split()[0] in ('segment', 'fade', 'stereo', 'stop', 'song', 'end')]
    nexts = [t for t, _ in steps[1:]] + [steps[-1][0] + 5.0]

    print('segments: the median of 5 ms frames, and the tone by DFT, each against the first at 255')
    first = None
    for (t, words), t_next in zip(steps, nexts):
        if words[0] != 'segment':
            continue
        slider, tone = int(words[1]), words[2]
        at = local(t)
        end = min([b for b in beeps if b > t] + [t_next]) - 0.1
        lvl, bright = rec.level(t + 0.5 + at, end + at)
        tlvl = tone_db(rec, t + 0.5 + at, end + at, MUSIC_TONES[tone])
        if lvl is None or tlvl is None:
            print('  %s %3d  %6.2f s  not in the recording' % (tone, slider, t))
            continue
        if first is None and slider == 255:
            first = (lvl, tlvl)
        rel = '' if first is None else '  %+6.2f / %+6.2f' % (lvl - first[0], tlvl - first[1])
        print('  %s %3d  %6.2f-%6.2f s  %6.1f dB  %4.0f Hz  tone %6.1f dB%s   (Galaxy %+6.2f, linear %+6.2f)' % (
            tone, slider, t + 0.5, end, lvl, bright, tlvl, rel,
            20 * math.log10(max(galaxy_music_gain(slider), 1e-9) / galaxy_music_gain(255)),
            20 * math.log10(max(slider, 1e-9) / 255.0)))

    print('fades: 5 ms frames against the 0.4 s before; times from the logged one')
    for (t, words), t_next in zip(steps, nexts):
        if words[0] != 'fade':
            continue
        how, length, old, new, slider = words[1], float(words[2]), words[3], words[4], int(words[5])
        at = local(t)
        t0 = t + at
        before = sorted(rec.env[rec.frame(t0 - 0.45):rec.frame(t0 - 0.05)])
        if not before:
            print('  %s at %.2f s: not in the recording' % (how, t))
            continue
        ref = rec.db(before[len(before) // 2])
        f0, f1 = rec.frame(t0 - 0.1), min(len(rec.env), rec.frame(t_next + at - 1.1))
        dbs = [rec.db(e) - ref for e in rec.env[f0:f1]]

        def when(cond, start=0):
            for i in range(max(start, 0), len(dbs) - 1):
                if cond(i) and cond(i + 1):
                    return i
            return None

        cross = {x: when(lambda i, x=x: dbs[i] <= -x) for x in (3, 6, 12, 24)}
        newhz = MUSIC_TONES[new]
        start = cross[12] if cross[12] is not None else 0
        onset = when(lambda i: dbs[i] >= -12 and abs(frame_brightness(rec, f0 + i) - newhz) < 0.1 * newhz, start)
        quiet_from = cross[24] if cross[24] is not None else start
        end = onset if onset is not None else len(dbs)
        silent = [i for i in range(quiet_from, end) if dbs[i] <= -60]
        oldhz = MUSIC_TONES[old]
        # the old tone 20 dB down or louder after its fade, the frame the
        # new one starts in left out
        burst = [i for i in range(quiet_from, end - 1) if dbs[i] >= -20 and
                 abs(frame_brightness(rec, f0 + i) - oldhz) < abs(frame_brightness(rec, f0 + i) - newhz)]
        sec = lambda i: (f0 + i) * rec.step - t0 if i is not None else float('nan')
        quiet = ('silent from %+.3f s, %.3f s of it' % (sec(silent[0]), len(silent) * rec.step)) if silent else 'never silent'
        print('  %s %.3g s %s->%s at %d, %.2f s: ref %.1f dB; -3 dB %+.3f s, -6 %+.3f, -12 %+.3f, -24 %+.3f; '
              '%s; %s from %+.3f s' % (
                  how, length, old, new, slider, t, ref, sec(cross[3]), sec(cross[6]), sec(cross[12]), sec(cross[24]),
                  quiet, new, sec(onset)))
        sq = lambda x: length * (1 - 10 ** (-x / 40.0))
        lin = lambda x: (length + 2 * MUSIC_LATENCY) * (1 - 10 ** (-x / 20.0))
        if None not in (cross[3], cross[12], cross[24]):
            print('      -3..-12 dB %.3f s, -12..-24 dB %.3f s (Galaxy\'s square %.3f, %.3f; linear %.3f, %.3f)' % (
                (cross[12] - cross[3]) * rec.step, (cross[24] - cross[12]) * rec.step,
                sq(12) - sq(3), sq(24) - sq(12), lin(12) - lin(3), lin(24) - lin(12)))
        if burst:
            hz = sum(frame_brightness(rec, f0 + i) for i in burst) / len(burst)
            print('      burst: %d frames, %.3f s, from %+.3f s, up to %+.1f dB, at %.0f Hz (%s %.0f Hz, %s %.0f Hz)' % (
                len(burst), len(burst) * rec.step, sec(burst[0]), max(dbs[i] for i in burst), hz,
                old, oldhz, new, newhz))
        else:
            print('      burst: none (no frame of %s 20 dB down or louder between its 24 dB point and %s)' % (old, new))

    for (t, words), t_next in zip(steps, nexts):
        if words[0] != 'stereo':
            continue
        at = local(t)
        a, b = t + 0.5 + at, min([bp for bp in beeps if bp > t] + [t_next, t + 4.6]) - 0.1 + at
        left, right = MUSIC_PAIR
        ll, lr = tone_db(rec, a, b, left, 0), tone_db(rec, a, b, left, 1)
        rl, rr = tone_db(rec, a, b, right, 0), tone_db(rec, a, b, right, 1)
        if None in (ll, lr, rl, rr) or rec.channels != 2:
            print('stereo: not in the recording')
            continue
        print('stereo%s %.2f-%.2f s: left channel %.2f Hz %6.1f dB, %.0f Hz %6.1f dB; right channel %.2f Hz %6.1f dB, %.0f Hz %6.1f dB' % (
            ' (' + ' '.join(words[2:]) + ')' if len(words) > 2 else '', t + 0.5, b - at, left, ll, right, rl, left, lr, right, rr))
        print('  crossfeed: the left tone in the right channel %+.1f dB, the right tone in the left %+.1f dB' % (
            lr - ll, rl - rr))

    if any(words[0] == 'song' for _, words in steps):
        print('songs: each from its onset after the change, 0.5 to 7 s of it: the median of 5 ms frames and the mean power')
    for (t, words), t_next in zip(steps, nexts):
        if words[0] != 'song':
            continue
        name = words[1]
        at = local(t)
        f0 = rec.frame(t + at - 0.1)
        f1 = min(len(rec.env) - 1, rec.frame(t_next + at))
        # The song's onset, the first two frames 45 dB under full scale or louder
        onset = next((f for f in range(f0, f1) if rec.db(rec.env[f]) > -45 and rec.db(rec.env[f + 1]) > -45), None)
        if onset is None:
            print('  %-42s at %6.2f s: silent' % (name, t))
            continue
        a = onset * rec.step + 0.5
        b = min(onset * rec.step + 7.0, t_next + at)
        lvl, bright = rec.level(a, b)
        frames = rec.env[rec.frame(a):rec.frame(b)]
        power = rec.db(sum(frames) / len(frames)) if frames else float('nan')
        print('  %-42s at %6.2f s: onset %+.3f s, median %6.1f dB, power %6.1f dB, %4.0f Hz' % (
            name, t, onset * rec.step - (t + at), lvl, power, bright))


def main():
    args = sys.argv[1:]
    mode = args[0] if args and args[0] in ('--onsets', '--levels', '--music') else None
    if mode:
        args = args[1:]
    if len(args) != 1:
        sys.exit(__doc__)
    run = args[0]
    rec = Recording(os.path.join(run, 'audio.wav'))
    print('%s: %.1f s of audio' % (run, rec.frames / rec.rate))
    if mode == '--levels':
        report_levels(rec)
        return
    if mode == '--music':
        report_music(rec, read_log(run))
        return
    strength = onsets(rec)
    if mode == '--onsets':
        report_onsets(rec, strength)
        return
    evts = events(read_log(run))
    offset = report_wall(rec, strength, evts)
    report_hear(rec, strength, evts, offset)
    offset, rest = report_reverb(rec, strength, evts, offset)
    report_pan(rec, offset, rest)


if __name__ == '__main__':
    main()
