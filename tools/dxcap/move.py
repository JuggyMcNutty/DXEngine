#!/usr/bin/env python3
"""Reads MoveConsole runs: does each pawn move as the original's does?

    move.py <reference run dir> <run dir> [<run dir> ...]
    move.py --pawn <name> [--off <units>] <reference run dir> <run dir> [...]

Each run's DXMOVE lines -- MoveConsole's, every two seconds: a pawn's state,
orders, move target, place, and the distance it moved since the line before
-- are read from its log (engine.log or DeusEx.log). The first run is the
reference, normally the original's, and each other is laid beside it, pawn by
pawn:

- distance: the whole run's distance moved, and whether it is within
  tolerance of the reference's (15% of it, or 200 units, whichever is more);
- stalls: the longest run of lines over which a pawn moved under 5 units
  while its orders were to go somewhere (anything but Standing, Sitting,
  Dancing or Idle), flagged where it is 3 lines or more longer than the
  reference's longest;
- targets: the move targets it reached, as each line names its mt, and those
  the reference reached that it never did.

The summary counts the pawns within tolerance and the stalls the reference
does not have; a pawn missing from either run is listed apart.

With --pawn it reads the runs' "DXMOVE: watch" lines instead --
MoveConsole's with Watch=<name>, the pawn each tick -- and prints for each
run where its path parts from the reference's, by place, not by time, as
two runs' clocks start apart (the load's first frame): the first tick more
than --off units (1 by default) from the reference's path -- before its start
or past its end, across the line its first or last stretch goes along, as two
runs' watches start and end at other places on it -- or, along it, with
another state or physics than the reference's nearest tick; a way walked back
is matched to its nearest stretch. That tick and the reference's nearest are printed whole,
with how much later or earlier the reference passed there. Then how far along
the reference's path the run came, where it stopped short of its end by more
than --off.
"""
import os
import re
import sys

LINE = re.compile(r"DXMOVE: (\d+) (\S+) (\S+) state (\S+) orders (\S+) mt (\S+) dest \S+ vel \S+ at \S+ moved (\d+)")
STILL_ORDERS = {"Standing", "Sitting", "Dancing", "Idle", "None"}


def read_run(run):
    """Each pawn's lines, in order: (time, state, orders, move target, moved)."""
    for name in ("engine.log", "DeusEx.log"):
        path = os.path.join(run, name)
        if os.path.exists(path):
            break
    else:
        sys.exit(f"{run}: no engine.log or DeusEx.log")
    pawns = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            t, pawn, cls, state, orders, mt, moved = m.groups()
            pawns.setdefault(pawn, {"class": cls, "lines": []})["lines"].append((int(t), state, orders, mt, int(moved)))
    return pawns


def longest_stall(lines):
    """The longest run of lines moving under 5 units while ordered to go."""
    best = run = 0
    for _, _, orders, _, moved in lines:
        if moved < 5 and orders not in STILL_ORDERS:
            run += 1
            best = max(best, run)
        else:
            run = 0
    return best


def targets(lines):
    return {mt for _, _, _, mt, _ in lines if mt != "None"}


def within(dist, ref):
    return abs(dist - ref) <= max(0.15 * ref, 200)


WATCH = re.compile(r"DXMOVE: watch ([-0-9.]+) dt \S+ \S+ (\S+) at ([-0-9.]+),([-0-9.]+),([-0-9.]+) "
                   r"vel \S+ physics (\d+) base \S+ state (\S+)")


def read_watch(run, pawn):
    """The pawn's ticks, in order: (time, (x, y, z), physics, state, line)."""
    for name in ("engine.log", "DeusEx.log"):
        path = os.path.join(run, name)
        if os.path.exists(path):
            break
    else:
        sys.exit(f"{run}: no engine.log or DeusEx.log")
    ticks = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = WATCH.search(line)
            if m and m.group(2).lower() == pawn.lower():
                t, x, y, z = (float(v) for v in m.group(1, 3, 4, 5))
                ticks.append((t, (x, y, z), int(m.group(6)), m.group(7), line[m.start():].rstrip()))
    return ticks


def nearest(ref, k, at):
    """The reference's path nearest to at, looked for from its segment k on:
    (segment, its share along it, the distance across), on the ground."""
    best = None
    for i in range(max(k - 1, 0), min(len(ref) - 1, k + 400)):
        (ax, ay, _), (bx, by, _) = ref[i][1], ref[i + 1][1]
        dx, dy = bx - ax, by - ay
        span = dx * dx + dy * dy
        f = 0.0 if span == 0.0 else max(0.0, min(1.0, ((at[0] - ax) * dx + (at[1] - ay) * dy) / span))
        d = ((ax + dx * f - at[0]) ** 2 + (ay + dy * f - at[1]) ** 2) ** 0.5
        if best is None or d < best[2]:
            best = (i, f, d)
    return best


def across(a, b, at):
    """at's distance from the line through a and b, on the ground; from a
    where they are one place."""
    (ax, ay, _), (bx, by, _) = a, b
    dx, dy = bx - ax, by - ay
    span = (dx * dx + dy * dy) ** 0.5
    if span == 0.0:
        return ((at[0] - ax) ** 2 + (at[1] - ay) ** 2) ** 0.5
    return abs((at[0] - ax) * dy - (at[1] - ay) * dx) / span


def parting(ref, other, off):
    """The first of other's ticks off ref's path by more than off -- across
    the line of its first or last stretch for a tick before its start or past
    its end -- or, along it, in another state or physics: (the tick, ref's
    nearest tick, the distance, ref's time there, where: "along", "before its
    start" or "past its end"); None if it never parts."""
    if len(ref) < 2:
        return None
    places = [r[1] for r in ref]
    first = next((p for p in places if p[:2] != places[0][:2]), places[-1])
    last = next((p for p in reversed(places) if p[:2] != places[-1][:2]), places[0])
    k = 0
    for tick in other:
        t, at, physics, state, _ = tick
        i, f, d = nearest(ref, k, at)
        k = i
        where = ("before its start" if i == 0 and f == 0.0 else
                 "past its end" if i == len(ref) - 2 and f == 1.0 else "along")
        if where == "before its start":
            d = across(places[0], first, at)
        elif where == "past its end":
            d = across(last, places[-1], at)
        a = ref[i] if f < 0.5 else ref[i + 1]
        if d > off or (where == "along" and (physics != a[2] or state != a[3])):
            when = ref[i][0] + (ref[i + 1][0] - ref[i][0]) * f
            return tick, a, d, when, where
    return None


def reach(ref, other, off):
    """How far along ref's path other came, its ticks within off of it, and
    the path's whole length, on the ground."""
    ends = [0.0]
    for i in range(len(ref) - 1):
        (ax, ay, _), (bx, by, _) = ref[i][1], ref[i + 1][1]
        ends.append(ends[-1] + ((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5)
    best = 0.0
    k = 0
    for tick in other:
        i, f, d = nearest(ref, k, tick[1])
        k = i
        if d <= off:
            best = max(best, ends[i] + (ends[i + 1] - ends[i]) * f)
    return best, ends[-1]


def watch_main(pawn, runs, off):
    ref = read_watch(runs[0], pawn)
    if not ref:
        sys.exit(f"{runs[0]}: no watch lines of {pawn}")
    for run in runs[1:]:
        other = read_watch(run, pawn)
        print(f"{os.path.basename(run)} against {os.path.basename(runs[0])}, {pawn}: {len(other)} ticks against {len(ref)}")
        found = parting(ref, other, off)
        if not found:
            print(f"  never parts by more than {off:g}")
        else:
            tick, near, d, when, where = found
            print(f"  parts {where} at {tick[0]:.3f} s, {d:.2f} units off; the reference passed there at {when:.3f} s "
                  f"({when - tick[0]:+.3f} s)")
            print(f"    {tick[4]}")
            print(f"    the reference's nearest tick: {near[4]}")
        came, length = reach(ref, other, off) if len(ref) >= 2 else (0.0, 0.0)
        if came < length - off:
            print(f"  stops short: {came:.1f} of the reference's {length:.1f} units along its path")


def main():
    if len(sys.argv) >= 5 and sys.argv[1] == "--pawn":
        args = sys.argv[3:]
        off = 1.0
        if args[0] == "--off" and len(args) >= 4:
            off = float(args[1])
            args = args[2:]
        return watch_main(sys.argv[2], args, off)
    if len(sys.argv) < 3 or sys.argv[1] == "--pawn":
        sys.exit(__doc__)
    ref_run = sys.argv[1]
    ref = read_run(ref_run)
    for run in sys.argv[2:]:
        other = read_run(run)
        print(f"{os.path.basename(run)} against {os.path.basename(ref_run)}:")
        names = sorted(set(ref) & set(other))
        ok = 0
        new_stalls = []
        for pawn in names:
            r_lines, o_lines = ref[pawn]["lines"], other[pawn]["lines"]
            r_dist = sum(l[4] for l in r_lines)
            o_dist = sum(l[4] for l in o_lines)
            r_stall, o_stall = longest_stall(r_lines), longest_stall(o_lines)
            missed = sorted(targets(r_lines) - targets(o_lines))
            good = within(o_dist, r_dist)
            ok += good
            stalled = o_stall >= r_stall + 3
            if stalled:
                new_stalls.append(pawn)
            flags = []
            if not good:
                flags.append("DISTANCE")
            if stalled:
                flags.append(f"STALL {o_stall} lines (reference {r_stall})")
            if missed:
                flags.append("never reached " + " ".join(missed))
            mark = "  " if not flags else "! "
            print(f"  {mark}{pawn:22s} {ref[pawn]['class']:18s} moved {o_dist:6d} vs {r_dist:6d}"
                  + (("  " + "; ".join(flags)) if flags else ""))
        only_ref = sorted(set(ref) - set(other))
        only_other = sorted(set(other) - set(ref))
        print(f"  {ok} of {len(names)} pawns' distance within tolerance; "
              f"{len(new_stalls)} stalling where the reference does not"
              + (f" ({', '.join(new_stalls)})" if new_stalls else ""))
        if only_ref:
            print(f"  only in the reference: {', '.join(only_ref)}")
        if only_other:
            print(f"  only in this run: {', '.join(only_other)}")
        print()


if __name__ == "__main__":
    main()
