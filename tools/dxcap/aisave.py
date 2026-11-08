#!/usr/bin/env python3
"""Lists a saved level's AI event manager as either engine writes it, for
EventRingConsole's save (SaveAt): the manager's counters and the receiver its
ring resumes at, each event type, each sender's actor and current levels, and
each receiver's actor, callbacks, flags, best score and sender, next slot and
ring neighbours, in the layout of the original's UEventManager::Serialize
(Engine.dll 0x103825f0) and the SDK's UnEventManager.h
(dx-reverse-info engine-dll.md, the AI event system: saved).

    aisave.py <level .dxs> [<actor name prefix>]

With a prefix (CapListener) only the senders and receivers of the actors so
named are listed. Nothing is written.
"""
import struct
import sys

TAG = 0x9E2A83C1
RF_HAS_STACK = 0x02000000


def read_index(b, p):
    """A compact index: sign and six bits, then seven bits a byte."""
    v = b[p]
    p += 1
    neg = v & 0x80
    r = v & 0x3F
    if v & 0x40:
        shift = 6
        while True:
            v = b[p]
            p += 1
            r |= (v & 0x7F) << shift
            shift += 7
            if not v & 0x80:
                break
    return (-r if neg else r), p


class Package:
    def __init__(self, path):
        b = open(path, 'rb').read()
        tag, ver, _lic, _flags, ncount, noff, ecount, eoff, icount, ioff = struct.unpack_from('<IHHIiiiiii', b, 0)
        if tag != TAG:
            sys.exit('%s: not a package' % path)
        self.b = b
        self.names = []
        p = noff
        for _ in range(ncount):
            if ver >= 64:
                n, p = read_index(b, p)
                self.names.append(b[p:p + n - 1].decode('latin-1'))
                p += n
            else:
                e = b.index(b'\0', p)
                self.names.append(b[p:e].decode('latin-1'))
                p = e + 1
            p += 4
        self.imports = []
        p = ioff
        for _ in range(icount):
            _cpkg, p = read_index(b, p)
            cname, p = read_index(b, p)
            p += 4
            oname, p = read_index(b, p)
            self.imports.append((self.names[cname], self.names[oname]))
        self.exports = []
        p = eoff
        for _ in range(ecount):
            cls, p = read_index(b, p)
            _sup, p = read_index(b, p)
            p += 4
            name, p = read_index(b, p)
            flags, = struct.unpack_from('<I', b, p)
            p += 4
            size, p = read_index(b, p)
            off = 0
            if size > 0:
                off, p = read_index(b, p)
            self.exports.append({'cls': cls, 'name': self.names[name], 'flags': flags, 'size': size, 'off': off})

    def ref(self, i):
        if i == 0:
            return 'None'
        if i > 0:
            return self.exports[i - 1]['name']
        return self.imports[-i - 1][1]

    def class_name(self, e):
        c = e['cls']
        if c == 0:
            return 'Class'
        if c > 0:
            return self.exports[c - 1]['name']
        return self.imports[-c - 1][1]

    # The reader over one export's data
    def start(self, e):
        self.p = e['off']
        self.end = e['off'] + e['size']
        if e['flags'] & RF_HAS_STACK:
            node = self.obj()
            self.obj()
            self.p += 8 + 4
            if node != 'None':
                self.index()
        self.skip_properties()

    def index(self):
        v, self.p = read_index(self.b, self.p)
        return v

    def obj(self):
        return self.ref(self.index())

    def name(self):
        return self.names[self.index()]

    def int32(self):
        v, = struct.unpack_from('<i', self.b, self.p)
        self.p += 4
        return v

    def byte(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def float(self):
        v, = struct.unpack_from('<f', self.b, self.p)
        self.p += 4
        return v

    def skip_properties(self):
        while True:
            if self.name() == 'None':
                return
            info = self.byte()
            kind = info & 0x0F
            code = (info >> 4) & 7
            if kind == 10:
                self.index()
            size = {0: 1, 1: 2, 2: 4, 3: 12, 4: 16}.get(code)
            if code == 5:
                size = self.byte()
            elif code == 6:
                size, = struct.unpack_from('<H', self.b, self.p)
                self.p += 2
            elif code == 7:
                size = self.int32()
            if (info & 0x80) and kind != 3:
                a = self.byte()
                if a & 0x80:
                    self.p += 1 if (a & 0xC0) == 0x80 else 3
            self.p += size


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    pkg = Package(sys.argv[1])
    prefix = sys.argv[2].lower() if len(sys.argv) == 3 else ''
    for e in pkg.exports:
        kind = pkg.class_name(e)
        if kind == 'EventManager':
            pkg.start(e)
            level = pkg.obj()
            ref, dele, slot = pkg.int32(), pkg.int32(), pkg.int32()
            first = pkg.obj()
            used = [pkg.obj() for _ in range(256)]
            print('manager %s: level %s refProcessing %d deleteCount %d currentSlot %d firstProcess %s, %d buckets used'
                  % (e['name'], level, ref, dele, slot, first, sum(1 for u in used if u != 'None')))
        elif kind == 'AIEventType':
            pkg.start(e)
            print('type %s: %s hash %08x senders %s receivers %s next %s'
                  % (e['name'], pkg.name(), pkg.int32() & 0xFFFFFFFF, pkg.obj(), pkg.obj(), pkg.obj()))
    for e in pkg.exports:
        kind = pkg.class_name(e)
        if kind not in ('AISenderEvent', 'AIReceiverEvent'):
            continue
        pkg.start(e)
        etype, actor, dying, nxt = pkg.obj(), pkg.obj(), pkg.int32(), pkg.obj()
        if prefix and not actor.lower().startswith(prefix):
            continue
        if kind == 'AISenderEvent':
            slots = [[pkg.float() for _ in range(4)] for _ in range(16)]
            cur = [pkg.float() for _ in range(4)]
            lit = sum(1 for s in slots if s[0] > 0 or s[1] > 0 or s[3] > 0)
            print('sender %s: type %s actor %s dying %d next %s current %s, %d slots with a level'
                  % (e['name'], etype, actor, dying, nxt, ' '.join('%g' % c for c in cur), lit))
        else:
            cb, scb = pkg.name(), pkg.name()
            invoke, state = pkg.int32(), pkg.byte()
            vis, dirc, cyl, los, on = (pkg.int32() for _ in range(5))
            score, best, slot = pkg.float(), pkg.obj(), pkg.int32()
            nproc, pproc = pkg.obj(), pkg.obj()
            print('receiver %s: type %s actor %s dying %d next %s callback %s score %s invoke %d state %d checks %d%d%d%d on %d bestScore %g bestSender %s nextSlot %d ring %s/%s'
                  % (e['name'], etype, actor, dying, nxt, cb, scb, invoke, state, vis, dirc, cyl, los, on, score, best, slot, pproc, nproc))


if __name__ == '__main__':
    main()
