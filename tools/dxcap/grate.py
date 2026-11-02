#!/usr/bin/env python3
"""GrateConsole's sweeps, the original's log against DXEngine's, place by
place (docs/DEVELOPMENT.md, the console classes).

    grate.py <original DeusEx.log> <dxe engine.log> [-v] [--aside REGEX]

A place is a sweep's grate, grid point and direction ("DXGRATE: sweep"); each
log gives there the actor hit, where and its normal, or nothing. Places where
either log meets an actor whose name matches --aside (by default Terrorist:
the terrorist walking through, whose place differs run to run) are set
aside. The rest are counted by kind: alike, another actor met, the same
actor met at another place (by the largest coordinate apart; under a
thousandth, a float step), or only the normal apart. -v lists each place
that differs, the original's hit first."""
import re, sys
pat = re.compile(r'DXGRATE: sweep (\S+) (-?\d+) (-?\d+) (\+[XY]) (\S+) at (\S+) normal (\S+)')
args = [a for a in sys.argv[1:]]
verbose = '-v' in args
aside_re = re.compile('Terrorist')
if '--aside' in args:
    aside_re = re.compile(args[args.index('--aside') + 1])
files = [a for a in args if not a.startswith('-') and a != (args[args.index('--aside') + 1] if '--aside' in args else None)]
def load(path):
    out = {}
    for line in open(path, errors='replace'):
        m = pat.search(line)
        if m:
            g, x, y, d, a, loc, n = m.groups()
            out[(g, int(x), int(y), d)] = (a, tuple(float(v) for v in loc.split(',')), tuple(float(v) for v in n.split(',')))
    return out
o = load(files[0]); e = load(files[1])
keys = sorted(set(o) | set(e))
kind = lambda a: re.sub(r'\d+$', '', a or 'none')
cats = {}
aside = 0
for k in keys:
    ho, he = o.get(k), e.get(k)
    ao = ho[0] if ho else None; ae = he[0] if he else None
    if (ao and aside_re.search(ao)) or (ae and aside_re.search(ae)):
        aside += 1
        continue
    if ho == he:
        cat = 'alike'
    elif ao != ae:
        cat = 'actor: original %s, DXEngine %s' % (kind(ao), kind(ae))
    else:
        dl = max(abs(a - b) for a, b in zip(ho[1], he[1]))
        if dl > 1e-3:
            cat = 'place: %s, %.2f apart' % (kind(ao), dl)
        elif dl > 0.0:
            cat = 'place: %s, a float step apart%s' % (kind(ao), '' if ho[2] == he[2] else ', and the normal')
        else:
            cat = 'normal: %s' % kind(ao)
    cats.setdefault(cat, []).append(k)
print('places', len(keys), '| set aside', aside, '| compared', len(keys) - aside)
for c, ks in sorted(cats.items(), key=lambda t: -len(t[1])):
    print('%5d  %s' % (len(ks), c))
    if verbose and c != 'alike':
        for k in ks:
            print('         %s %d %d %s: %s | %s' % (k + (o.get(k), e.get(k))))
