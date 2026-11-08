"""A small, read-only UE1 package reader for the sweep (names, imports,
exports), and a scan for a byte property's tag in each export's data.

    ue1pkg.py tag <package> <PropertyName>     every export whose data holds
                                               <Name> as a byte property tag
"""
import struct
import sys


def read_compact(b, o):
    """UE1's compact index: (value, new offset)."""
    b0 = b[o]
    o += 1
    neg = b0 & 0x80
    v = b0 & 0x3F
    if b0 & 0x40:
        shift = 6
        while True:
            bx = b[o]
            o += 1
            v |= (bx & 0x7F) << shift
            shift += 7
            if not (bx & 0x80):
                break
    return (-v if neg else v), o


def compact_bytes(v):
    neg = v < 0
    v = abs(v)
    out = bytearray()
    b0 = (0x80 if neg else 0) | (v & 0x3F)
    v >>= 6
    if v:
        b0 |= 0x40
    out.append(b0)
    while v:
        bx = v & 0x7F
        v >>= 7
        if v:
            bx |= 0x80
        out.append(bx)
    return bytes(out)


class Package:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.b = f.read()
        b = self.b
        tag, ver, lic, flags = struct.unpack_from("<IHHI", b, 0)
        assert tag == 0x9E2A83C1, hex(tag)
        self.version = ver
        name_count, name_off, exp_count, exp_off, imp_count, imp_off = struct.unpack_from("<6I", b, 12)
        self.names = []
        o = name_off
        for _ in range(name_count):
            if ver >= 64:
                n, o = read_compact(b, o)
                s = b[o:o + n - 1].decode("latin-1")
                o += n
            else:
                e = b.index(b"\0", o)
                s = b[o:e].decode("latin-1")
                o = e + 1
            o += 4  # flags
            self.names.append(s)
        self.imports = []
        o = imp_off
        for _ in range(imp_count):
            cpkg, o = read_compact(b, o)
            cname, o = read_compact(b, o)
            pkg = struct.unpack_from("<i", b, o)[0]
            o += 4
            name, o = read_compact(b, o)
            self.imports.append((cpkg, cname, pkg, name))
        self.exports = []
        o = exp_off
        for _ in range(exp_count):
            cls, o = read_compact(b, o)
            sup, o = read_compact(b, o)
            pkg = struct.unpack_from("<i", b, o)[0]
            o += 4
            name, o = read_compact(b, o)
            oflags = struct.unpack_from("<I", b, o)[0]
            o += 4
            size, o = read_compact(b, o)
            off = 0
            if size > 0:
                off, o = read_compact(b, o)
            self.exports.append(dict(cls=cls, sup=sup, pkg=pkg, name=name, flags=oflags, size=size, off=off))

    def obj_name(self, ref):
        if ref == 0:
            return "None"
        if ref < 0:
            return self.names[self.imports[-ref - 1][3]]
        return self.names[self.exports[ref - 1]["name"]]

    def class_name(self, exp):
        return self.obj_name(exp["cls"]) if exp["cls"] else "Class"

    def export_at(self, offset):
        for e in self.exports:
            if e["size"] and e["off"] <= offset < e["off"] + e["size"]:
                return e
        return None


def tag_scan(path, prop):
    p = Package(path)
    try:
        idx = p.names.index(prop)
    except ValueError:
        print(f"{path}: no name {prop}")
        return
    pat = compact_bytes(idx) + b"\x01"
    b = p.b
    o = 0
    seen = []
    while True:
        o = b.find(pat, o)
        if o < 0:
            break
        e = p.export_at(o)
        if e:
            val = b[o + len(pat)]
            seen.append((p.class_name(e), p.names[e["name"]], o - e["off"], e["size"], val))
        o += 1
    for s in seen:
        print("%-12s %-32s at %6d of %6d: %d" % s)


if __name__ == "__main__":
    if sys.argv[1] == "tag":
        tag_scan(sys.argv[2], sys.argv[3])


SIZES = {0: 1, 1: 2, 2: 4, 3: 12, 4: 16}


def skip_properties(p, o):
    """Past an object's tagged properties: the offset after its None."""
    b = p.b
    while True:
        n, o = read_compact(b, o)
        if p.names[n] == "None":
            return o
        info = b[o]
        o += 1
        t = info & 0x0F
        sc = (info >> 4) & 7
        arr = info & 0x80
        if t == 10:
            _, o = read_compact(b, o)
        if sc in SIZES:
            size = SIZES[sc]
        elif sc == 5:
            size = b[o]; o += 1
        elif sc == 6:
            size = struct.unpack_from("<H", b, o)[0]; o += 2
        else:
            size = struct.unpack_from("<I", b, o)[0]; o += 4
        if arr and t != 3:
            ai = b[o]
            if ai & 0x80 == 0:
                o += 1
            elif ai & 0xC0 == 0x80:
                o += 2
            else:
                o += 4
        if t != 3:
            o += size


class Reader:
    def __init__(self, p, o):
        self.p, self.b, self.o = p, p.b, o

    def u8(self):
        v = self.b[self.o]; self.o += 1; return v

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.o)[0]; self.o += 2; return v

    def i32(self):
        v = struct.unpack_from("<i", self.b, self.o)[0]; self.o += 4; return v

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.o)[0]; self.o += 4; return v

    def f32(self):
        v = struct.unpack_from("<f", self.b, self.o)[0]; self.o += 4; return v

    def idx(self):
        v, self.o = read_compact(self.b, self.o); return v


def read_lodmesh(p, e):
    """The LodMesh's tables the sweep asks of: faces, wedges, materials,
    textures (names), and the special faces."""
    r = Reader(p, e["off"])
    r.o = skip_properties(p, r.o)
    r.o += 25 + 16  # bounding box and sphere
    # Verts (lazy)
    r.u32(); n = r.idx(); r.o += 8 * n
    verts = n
    # Tris (lazy)
    r.u32(); n = r.idx(); r.o += 20 * n
    # AnimSeqs
    n = r.idx()
    for _ in range(n):
        r.idx(); r.idx(); r.i32(); r.i32()
        k = r.idx()
        for _ in range(k):
            r.f32(); r.idx()
        r.f32()
    # Connects (lazy)
    r.u32(); n = r.idx(); r.o += 8 * n
    r.o += 25 + 16
    # VertLinks (lazy)
    r.u32(); n = r.idx(); r.o += 4 * n
    n = r.idx()
    textures = [p.obj_name(r.idx()) for _ in range(n)]
    n = r.idx(); r.o += 25 * n
    n = r.idx(); r.o += 16 * n
    framev = r.i32(); frames = r.i32()
    r.u32(); r.u32()
    r.o += 12 + 12 + 12 + 8
    n = r.idx(); r.o += 4 * n  # TextureLOD (v68)
    # ULodMesh
    n = r.idx(); collapse_points = [r.u16() for _ in range(n)]
    n = r.idx(); face_level = [r.u16() for _ in range(n)]
    n = r.idx(); faces = [(r.u16(), r.u16(), r.u16(), r.u16()) for _ in range(n)]
    n = r.idx(); collapse_wedges = [r.u16() for _ in range(n)]
    n = r.idx(); wedges = [(r.u16(), r.u8(), r.u8()) for _ in range(n)]
    n = r.idx(); materials = [(r.u32(), r.i32()) for _ in range(n)]
    n = r.idx(); special = [(r.u16(), r.u16(), r.u16(), r.u16()) for _ in range(n)]
    model_verts = r.u32(); special_verts = r.u32()
    return dict(textures=textures, faces=faces, wedges=wedges, materials=materials,
                special=special, model_verts=model_verts, special_verts=special_verts,
                frame_verts=framev, frames=frames, face_level=face_level)


def meshes(path):
    p = Package(path)
    for e in p.exports:
        if p.class_name(e) == "LodMesh" and e["size"]:
            try:
                yield p.names[e["name"]], read_lodmesh(p, e)
            except Exception as ex:  # a reader's limit, not the file's
                print(f"{path}: {p.names[e['name']]}: {ex}", file=sys.stderr)


def bool_scan(path, prop):
    """Every export whose data holds a BoolProperty tag of <prop>: its value."""
    p = Package(path)
    try:
        idx = p.names.index(prop)
    except ValueError:
        return []
    out = []
    for info, val in ((0x03, False), (0x83, True)):
        pat = compact_bytes(idx) + bytes([info])
        o = 0
        while True:
            o = p.b.find(pat, o)
            if o < 0:
                break
            e = p.export_at(o)
            if e:
                out.append((p.class_name(e), p.names[e["name"]], val))
            o += 1
    return out


def bool_scan2(path, prop, classes_only=True):
    """As bool_scan, any size code: (export class, export name, value)."""
    p = Package(path)
    try:
        idx = p.names.index(prop)
    except ValueError:
        return []
    out = []
    pat = compact_bytes(idx)
    o = 0
    while True:
        o = p.b.find(pat, o)
        if o < 0:
            break
        info = p.b[o + len(pat)]
        if info & 0x0F == 3:
            e = p.export_at(o)
            if e and (not classes_only or p.class_name(e) == "Class"):
                out.append((p.class_name(e), p.names[e["name"]], bool(info & 0x80)))
        o += 1
    return out


def props(p, e):
    """An export's tagged properties as {name[index]: raw bytes or bool},
    past its state frame when it has one (RF_HasStack)."""
    b = p.b
    o = e["off"]
    if e["flags"] & 0x02000000:
        node, o = read_compact(b, o)
        _, o = read_compact(b, o)
        o += 8 + 4  # probe mask, latent action
        if node:
            _, o = read_compact(b, o)
    out = {}
    while True:
        n, o = read_compact(b, o)
        name = p.names[n]
        if name == "None":
            return out
        info = b[o]
        o += 1
        t = info & 0x0F
        sc = (info >> 4) & 7
        arr = info & 0x80
        if t == 10:
            _, o = read_compact(b, o)
        if sc in SIZES:
            size = SIZES[sc]
        elif sc == 5:
            size = b[o]; o += 1
        elif sc == 6:
            size = struct.unpack_from("<H", b, o)[0]; o += 2
        else:
            size = struct.unpack_from("<I", b, o)[0]; o += 4
        ai = 0
        if arr and t != 3:
            x = b[o]
            if x & 0x80 == 0:
                ai = x; o += 1
            elif x & 0xC0 == 0x80:
                ai = struct.unpack_from(">H", b, o)[0] & 0x3FFF; o += 2
            else:
                ai = struct.unpack_from(">I", b, o)[0] & 0x3FFFFFFF; o += 4
        key = name if ai == 0 else f"{name}[{ai}]"
        if t == 3:
            out[key] = bool(arr)
        else:
            out[key] = (t, b[o:o + size])
            o += size
