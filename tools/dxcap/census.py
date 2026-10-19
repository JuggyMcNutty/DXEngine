#!/usr/bin/env python3
"""Reads CensusConsole runs: what a loaded save spawns and keeps, by class.

    census.py <original run dir> <dxe run dir>

Each run's DXCENSUS lines -- CensusConsole's, each second of the counted
level: a line of totals, then each class whose numbers changed, as
<class>=<live>/<peak>/<spawned>/<name> -- are read from its log (DeusEx.log
or engine.log). A class a second does not list kept its live actors, peak and
name number, and spawned none. Printed:

- each second, both runs' totals: actors live, spawned in that second, and
  classes seen;
- each class either run spawned, or whose live actors at the end differ, by
  DXEngine's spawns, most first: each run's spawns over the count, live
  actors at the end, peak, and the gap -- how far its highest name number
  rose beyond its spawns after the second it was first seen, the deaths in
  spawn the notification never hears;
- the first second the two part: a class's spawns so far, or its live
  actors, differing by more than 20 and by more than half the larger.
"""
import os
import re
import sys

TOTAL = re.compile(r"DXCENSUS: (\d+) total live (\d+) spawned (\d+) classes (\d+)")
CLASSES = re.compile(r"DXCENSUS: (\d+)((?: \S+=\d+/\d+/\d+/-?\d+)+)\s*$")
ENTRY = re.compile(r"(\S+)=(\d+)/(\d+)/(\d+)/(-?\d+)")


def read_run(run):
    """(totals by second, {class: {second: (live, peak, spawned, name)}})."""
    for name in ("DeusEx.log", "engine.log"):
        path = os.path.join(run, name)
        if os.path.exists(path):
            break
    else:
        sys.exit(f"{run}: no DeusEx.log or engine.log")
    totals, classes = {}, {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = TOTAL.search(line)
            if m:
                t, live, spawned, kinds = map(int, m.groups())
                totals[t] = (live, spawned, kinds)
                continue
            m = CLASSES.search(line)
            if not m:
                continue
            t = int(m.group(1))
            for cls, live, peak, spawned, num in ENTRY.findall(m.group(2)):
                classes.setdefault(cls, {})[t] = (int(live), int(peak), int(spawned), int(num))
    if not totals:
        sys.exit(f"{path}: no DXCENSUS lines")
    return totals, classes


def series(entries, seconds):
    """A class's (live, peak, spawned, name) for every second, carried over
    the seconds its run did not list it; None before it is first seen."""
    out, last = [], None
    for t in range(seconds):
        if t in entries:
            last = entries[t]
            out.append(last)
        elif last is None:
            out.append(None)
        else:
            out.append((last[0], last[1], 0, last[3]))
    return out


def summary(rows):
    """Spawns over the count, live at the end, peak, and the name gap."""
    seen = [r for r in rows if r is not None]
    if not seen:
        return 0, 0, 0, None
    spawned = sum(r[2] for r in seen)
    first = next(i for i, r in enumerate(rows) if r is not None)
    after = sum(r[2] for r in rows[first + 1:] if r is not None)
    gap = None
    if rows[first][3] >= 0 and seen[-1][3] >= 0:
        gap = seen[-1][3] - rows[first][3] - after
    return spawned, seen[-1][0], max(r[1] for r in seen), gap


def parts(a, b):
    return abs(a - b) > 20 and abs(a - b) > 0.5 * max(a, b)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    orig_run, dxe_run = sys.argv[1:]
    orig_totals, orig = read_run(orig_run)
    dxe_totals, dxe = read_run(dxe_run)
    seconds = min(max(orig_totals), max(dxe_totals)) + 1
    print(f"{os.path.basename(orig_run)} (original) against {os.path.basename(dxe_run)} (DXEngine), {seconds} s:")
    print("  s   original: live spawned classes   DXEngine: live spawned classes")
    for t in range(seconds):
        o = orig_totals.get(t, (0, 0, 0))
        d = dxe_totals.get(t, (0, 0, 0))
        print(f"{t:3}   {o[0]:15} {o[1]:7} {o[2]:7}   {d[0]:15} {d[1]:7} {d[2]:7}")

    names = sorted(set(orig) | set(dxe))
    rows = {}
    for cls in names:
        o = series(orig.get(cls, {}), seconds)
        d = series(dxe.get(cls, {}), seconds)
        rows[cls] = (o, d, summary(o), summary(d))
    shown = [cls for cls in names
             if rows[cls][2][0] or rows[cls][3][0] or rows[cls][2][1] != rows[cls][3][1]]
    shown.sort(key=lambda cls: (-rows[cls][3][0], -rows[cls][2][0], cls))
    print()
    print("class                          spawned: orig    dxe   live at end: orig    dxe   peak: orig    dxe   gap: orig    dxe")
    for cls in shown:
        _, _, (os_, ol, op, og), (ds, dl, dp, dg) = rows[cls]
        gap = lambda g: "-" if g is None else str(g)
        print(f"{cls:30} {os_:13} {ds:6} {ol:17} {dl:6} {op:10} {dp:6} {gap(og):9} {gap(dg):6}")

    print()
    for t in range(seconds):
        parted = []
        for cls in names:
            o, d = rows[cls][0], rows[cls][1]
            o_spawned = sum(r[2] for r in o[:t + 1] if r is not None)
            d_spawned = sum(r[2] for r in d[:t + 1] if r is not None)
            o_live = o[t][0] if o[t] is not None else 0
            d_live = d[t][0] if d[t] is not None else 0
            if parts(o_spawned, d_spawned):
                parted.append(f"{cls} spawned {o_spawned} against {d_spawned}")
            elif parts(o_live, d_live):
                parted.append(f"{cls} live {o_live} against {d_live}")
        if parted:
            print(f"The runs part at {t} s: " + "; ".join(parted))
            break
    else:
        print(f"The runs never part over {seconds} s")


if __name__ == "__main__":
    main()
