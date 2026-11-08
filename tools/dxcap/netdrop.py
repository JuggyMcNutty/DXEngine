#!/usr/bin/env python3
"""netdrop.py: tools/dxcap/netrelay.py that loses one bunch, the lost-open
test's relay (OpenLossServer, CapOpenLoss, OpenLossJoinConsole).

    netdrop.py [--listen 7790] [--server 127.0.0.1:7792] --drop-marker TEXT
               [--drop-secs 3] [--log FILE] [--secs 300]

As netrelay.py it forwards each client's datagrams to --server and the
server's answers back, decoding every packet as the original's are (its
number, its acks, each bunch's header; dx-reverse-info network.md, packets).
From the first server bunch whose data carries TEXT's bytes, at any bit
offset, for --drop-secs, every such bunch is taken out of its datagram and
the rest goes on: an actor's opening bunch carrying the marker string, and
its resends, are lost alone, so a reliable bunch of that channel sent after
the open, and the channel's unreliable updates, reach the client before it,
as they do when the open's packet alone is lost. Each datagram a bunch was
taken from is lost to the server as well: the client's acks of it are taken
out of the client's datagrams, so the server counts it lost and sends the
open again (UNetConnection::ReceivedNak), which once the window is over goes
through. Each change is printed; --log writes a line for every datagram.
Exits after --secs, or on Ctrl-C.
"""
import argparse
import select
import socket
import time

MAX_PACKET = 512
MAX_PACKET_ID = 16384
MAX_CHANNELS = 1023
MAX_CH_SEQUENCE = 1024
MAX_CH_TYPE = 8


class Bits:
    """The packet's bits, first bit lowest; ReadInt as FBitReader's."""

    def __init__(self, data, num):
        self.data, self.num, self.pos = data, num, 0

    def bit(self):
        if self.pos >= self.num:
            raise ValueError("past the end")
        b = (self.data[self.pos >> 3] >> (self.pos & 7)) & 1
        self.pos += 1
        return b

    def int(self, value_max):
        value, mask = 0, 1
        while value + mask < value_max and mask:
            if self.bit():
                value |= mask
            mask <<= 1
        return value


def decode(data):
    """(packet id, its bits before the first element, [element], bit count)
    of a datagram: an element is an ack or a bunch, with the bits it spans
    (start, end); a bunch's data from data_start, data_bits long."""
    if not data or data[-1] == 0:
        raise ValueError("no trailing bit")
    last, num = data[-1], len(data) * 8 - 1
    while not last & 0x80:
        last = (last << 1) & 0xFF
        num -= 1
    r = Bits(data, num)
    packet = r.int(MAX_PACKET_ID)
    head = r.pos
    out = []
    while r.pos < r.num:
        start = r.pos
        if r.bit():
            out.append({"ack": r.int(MAX_PACKET_ID), "start": start, "end": r.pos})
            continue
        b = {"ack": None, "start": start, "control": r.bit()}
        b["open"] = r.bit() if b["control"] else 0
        b["close"] = r.bit() if b["control"] else 0
        b["reliable"] = r.bit()
        b["ch"] = r.int(MAX_CHANNELS)
        b["seq"] = r.int(MAX_CH_SEQUENCE) if b["reliable"] else None
        b["type"] = r.int(MAX_CH_TYPE) if (b["reliable"] or b["open"]) else None
        b["data_bits"] = r.int(MAX_PACKET * 8)
        if r.pos + b["data_bits"] > r.num:
            raise ValueError("bunch past the end")
        b["data_start"] = r.pos
        r.pos += b["data_bits"]
        b["end"] = r.pos
        out.append(b)
    return packet, head, out, num


def encode(data, head, elements):
    """The datagram again from its packet number's bits and the elements
    kept, each copied bit for bit, then the trailing 1 and the byte's 0s."""
    bits = []

    def copy(start, end):
        for i in range(start, end):
            bits.append((data[i >> 3] >> (i & 7)) & 1)

    copy(0, head)
    for e in elements:
        copy(e["start"], e["end"])
    bits.append(1)
    while len(bits) & 7:
        bits.append(0)
    out = bytearray(len(bits) >> 3)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def holds(data, b, marker):
    """Whether the bunch's data carries the marker's bytes at a bit offset
    of 0 to 7 (a string's characters go 8 bits each, from wherever the
    field before them left the bunch)."""
    value = int.from_bytes(data, "little")
    for shift in range(8):
        n = (b["data_bits"] - shift) // 8
        if n < len(marker):
            break
        chunk = ((value >> (b["data_start"] + shift)) & ((1 << (8 * n)) - 1)).to_bytes(n, "little")
        if marker in chunk:
            return True
    return False


def describe(elements):
    parts = []
    for b in elements:
        if b["ack"] is not None:
            parts.append("ack%d" % b["ack"])
            continue
        flags = ("O" if b["open"] else "") + ("C" if b["close"] else "") + ("R" if b["reliable"] else "U")
        parts.append("ch%d:%s:%d%s" % (b["ch"], flags, b["data_bits"], "" if b["seq"] is None else "#%d" % b["seq"]))
    return " ".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--listen", type=int, default=7790)
    ap.add_argument("--server", default="127.0.0.1:7792")
    ap.add_argument("--drop-marker", required=True)
    ap.add_argument("--drop-secs", type=float, default=3.0)
    ap.add_argument("--log")
    ap.add_argument("--secs", type=float, default=300.0)
    args = ap.parse_args()
    host, port = args.server.rsplit(":", 1)
    server = (host, int(port))
    marker = args.drop_marker.encode("latin-1")

    front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    front.bind(("0.0.0.0", args.listen))
    log = open(args.log, "w") if args.log else None
    upstream, clients = {}, {}
    start = time.monotonic()
    first_drop = None
    stripped_bunches = 0
    hidden_acks = 0
    lost = {}   # client address -> the server's packet numbers taken from
    print("netdrop: %d -> %s:%d, taking out bunches holding '%s' for %.1f s" % (args.listen, server[0], server[1], args.drop_marker, args.drop_secs), flush=True)
    while time.monotonic() - start < args.secs:
        ready, _, _ = select.select([front] + list(clients), [], [], 0.05)
        for s in ready:
            data, addr = s.recvfrom(65536)
            now = time.monotonic() - start
            if s is front:
                up = upstream.get(addr)
                if up is None:
                    up = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    up.bind(("127.0.0.1", 0))
                    upstream[addr] = up
                    clients[up] = addr
                    lost[addr] = set()
                    print("netdrop: client %s:%d" % addr, flush=True)
                # The client's acks of a datagram a bunch was taken from go
                # nowhere: the server counts that datagram lost.
                gone = lost[addr]
                if gone:
                    try:
                        packet, head, elements, _ = decode(data)
                        kept = [e for e in elements if e["ack"] is None or e["ack"] not in gone]
                        if len(kept) != len(elements):
                            hidden = [e["ack"] for e in elements if e["ack"] is not None and e["ack"] in gone]
                            hidden_acks += len(hidden)
                            print("netdrop: %.4f c2s pk %d: acks of %s hidden" % (now, packet, ",".join(str(a) for a in hidden)), flush=True)
                            data = encode(data, head, kept)
                    except ValueError as e:
                        if log:
                            log.write("%.4f c2s %d bytes: undecoded (%s)\n" % (now, len(data), e))
                if log:
                    log.write("%.4f c2s %d bytes\n" % (now, len(data)))
                up.sendto(data, server)
                continue
            client = clients[s]
            try:
                packet, head, elements, _ = decode(data)
            except ValueError as e:
                if log:
                    log.write("%.4f s2c %d bytes: undecoded (%s)\n" % (now, len(data), e))
                front.sendto(data, client)
                continue
            marked = [e for e in elements if e["ack"] is None and holds(data, e, marker)]
            if marked and first_drop is None:
                first_drop = now
            if marked and now - first_drop <= args.drop_secs:
                kept = [e for e in elements if e not in marked]
                stripped_bunches += len(marked)
                lost[client].add(packet)
                print("netdrop: %.4f s2c pk %d: took out %s, passed %s" % (now, packet, describe(marked), describe(kept) or "nothing"), flush=True)
                if log:
                    log.write("%.4f s2c pk %d TOOK OUT %s PASSED %s\n" % (now, packet, describe(marked), describe(kept)))
                front.sendto(encode(data, head, kept), client)
                continue
            if marked:
                print("netdrop: %.4f passed marked s2c pk %d (window over): %s" % (now, packet, describe(elements)), flush=True)
            if log:
                log.write("%.4f s2c pk %d %s\n" % (now, packet, describe(elements)))
            front.sendto(data, client)
    print("netdrop: %d bunches taken out, %d acks hidden" % (stripped_bunches, hidden_acks), flush=True)
    if log:
        log.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
