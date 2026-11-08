#!/usr/bin/env python3
"""bunchfirst.py: a first packet no stock client sends, for a server of
either engine (bugs.md NET.37).

    bunchfirst.py [--server 127.0.0.1:7790] [--secs 2]

From a socket of its own it sends the server one packet: number 0, an
actor channel's opening bunch on channel 5 (reliable, one byte of data),
then the control channel's opening bunch with "HELLO REVISION=0 MINVER=1100
VER=1100", as a client's first is. The original's server opens a connection
for the new address, acknowledges the packet, and reads no further than
the first bunch: a bunch for a channel not open, with no control channel
yet, ends the packet (UNetConnection::ReceivedPacket, Engine.dll
0x104035d9, "Received bunch before connected"), so the HELLO is never read
and nothing but acks comes back. A server that makes the channel instead
refuses it, sending its close, and reads the HELLO, answering CHALLENGE.
Every datagram that comes back for --secs is decoded and printed, its acks,
its bunches, and the control channel's lines.
"""
import argparse
import socket
import time

MAX_PACKET = 512
MAX_PACKET_ID = 16384
MAX_CHANNELS = 1023
MAX_CH_SEQUENCE = 1024
MAX_CH_TYPE = 8


class Writer:
    def __init__(self):
        self.bits = []

    def bit(self, b):
        self.bits.append(1 if b else 0)

    def int(self, value, value_max):
        new, mask = 0, 1
        while new + mask < value_max and mask:
            b = 1 if value & mask else 0
            self.bit(b)
            new += mask if b else 0
            mask <<= 1

    def byte(self, value):
        for i in range(8):
            self.bit((value >> i) & 1)

    def done(self):
        bits = self.bits + [1]
        while len(bits) & 7:
            bits.append(0)
        out = bytearray(len(bits) >> 3)
        for i, b in enumerate(bits):
            if b:
                out[i >> 3] |= 1 << (i & 7)
        return bytes(out)


def text_bytes(text):
    """A string as the control channel's lines go: its length with the
    terminator as a compact index, its characters, a 0."""
    data = text.encode("latin-1")
    n = len(data) + 1
    out = bytearray()
    b0 = n & 0x3f
    n >>= 6
    if n:
        b0 |= 0x40
    out.append(b0)
    while n:
        b = n & 0x7f
        n >>= 7
        if n:
            b |= 0x80
        out.append(b)
    return bytes(out) + data + b"\0"


def bunch(w, ch, typ, payload):
    w.bit(0)
    w.bit(1)            # control: open and close bits follow
    w.bit(1)            # open
    w.bit(0)            # close
    w.bit(1)            # reliable
    w.int(ch, MAX_CHANNELS)
    w.int(1, MAX_CH_SEQUENCE)
    w.int(typ, MAX_CH_TYPE)
    w.int(len(payload) * 8, MAX_PACKET * 8)
    for b in payload:
        w.byte(b)


class Bits:
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

    def byte(self):
        return sum(self.bit() << i for i in range(8))


def lines_of(r, end):
    """The control channel's lines in a bunch's data."""
    out = []
    while r.pos + 8 <= end:
        b0 = r.byte()
        n, shift = b0 & 0x3f, 6
        more = b0 & 0x40
        while more:
            b = r.byte()
            n |= (b & 0x7f) << shift
            shift += 7
            more = b & 0x80
        if b0 & 0x80:
            chars = [r.byte() | (r.byte() << 8) for _ in range(n)]
        else:
            chars = [r.byte() for _ in range(n)]
        out.append("".join(chr(c) for c in chars).rstrip("\0"))
    return out


def describe(data):
    last, num = data[-1], len(data) * 8 - 1
    while not last & 0x80:
        last = (last << 1) & 0xFF
        num -= 1
    r = Bits(data, num)
    parts = ["pk %d" % r.int(MAX_PACKET_ID)]
    while r.pos < r.num:
        if r.bit():
            parts.append("ack %d" % r.int(MAX_PACKET_ID))
            continue
        control = r.bit()
        opened = r.bit() if control else 0
        closed = r.bit() if control else 0
        reliable = r.bit()
        ch = r.int(MAX_CHANNELS)
        seq = r.int(MAX_CH_SEQUENCE) if reliable else None
        typ = r.int(MAX_CH_TYPE) if (reliable or opened) else None
        bits = r.int(MAX_PACKET * 8)
        start = r.pos
        flags = ("O" if opened else "") + ("C" if closed else "") + ("R" if reliable else "U")
        part = "ch%d:%s:%d" % (ch, flags, bits)
        if ch == 0 and bits:
            part += " " + repr(lines_of(r, start + bits))
        r.pos = start + bits
        parts.append(part)
    return " ".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--server", default="127.0.0.1:7790")
    ap.add_argument("--secs", type=float, default=2.0)
    args = ap.parse_args()
    host, port = args.server.rsplit(":", 1)
    server = (host, int(port))

    w = Writer()
    w.int(0, MAX_PACKET_ID)
    bunch(w, 5, 2, b"\0")
    bunch(w, 0, 1, text_bytes("HELLO REVISION=0 MINVER=1100 VER=1100"))
    packet = w.done()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    s.settimeout(0.1)
    s.sendto(packet, server)
    print("bunchfirst: sent %d bytes to %s:%d: %s" % (len(packet), server[0], server[1], describe(packet)), flush=True)
    start = time.monotonic()
    got = 0
    while time.monotonic() - start < args.secs:
        try:
            data, addr = s.recvfrom(65536)
        except socket.timeout:
            continue
        got += 1
        try:
            text = describe(data)
        except ValueError as e:
            text = "undecoded (%s)" % e
        print("bunchfirst: %.3f s %d bytes: %s" % (time.monotonic() - start, len(data), text), flush=True)
    print("bunchfirst: %d datagrams back" % got, flush=True)


if __name__ == "__main__":
    main()
