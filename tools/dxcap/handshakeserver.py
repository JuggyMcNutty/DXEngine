#!/usr/bin/env python3
"""A server that speaks the join's first lines in ways no stock server does,
for HandshakeConsole (docs/DEVELOPMENT.md, net tests): a server of another
version, and values with blanks the original's Core reads past.

    handshakeserver.py [--port 7790] [--mode upgrade|values] [--minver 400]
                       [--ver 436] [--secs 120]

It listens on UDP --port and answers each client's control channel as the
original's server would (dx-reverse-info network.md, joining and packets):

- upgrade: the HELLO gets "UPGRADE MINVER=<minver> VER=<ver>", as the
  original's server sends it to a client of another version, then nothing
  more of that connection (Engine.dll ULevel::NotifyReceivedText,
  0x1039f780);
- values: the HELLO gets "CHALLENGE VER= 1100 CHALLENGE= 7 STATS= 1", each
  value after a blank, and the LOGIN a FAILURE with a tab before it and two
  after: "<tab>FAILURE<tab><tab>spaced text".

Every packet a client sends is acknowledged in a packet of the server's, as
the new connection the original's server opens for a client's later
packets acknowledges them (IpDrv.dll UTcpNetDriver::TickDispatch,
0x1070a440), so a client left waiting is never timed out. Each line a
client sends on its control channel is logged ("client: <text>"), its
close too, and each line sent; in values mode the LOGIN's RESPONSE= is
told against the answers to 7 and to 0. It exits after --secs.
"""
import argparse
import socket
import time

MAX_PACKET = 512
MAX_PACKET_ID = 16384
MAX_CHANNELS = 1023
MAX_CH_SEQUENCE = 1024
MAX_CH_TYPE = 8
CONTROL = 1


class Reader:
    """A packet's bits, first bit lowest; ReadInt as FBitReader's."""

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

    def bytes(self, bits):
        out = bytearray()
        for _ in range(bits // 8):
            out.append(sum(self.bit() << i for i in range(8)))
        self.pos += bits % 8
        return bytes(out)


class Writer:
    """FBitWriter's: bits first lowest, WriteInt in as many bits as the
    maximum needs."""

    def __init__(self):
        self.bits = []

    def bit(self, b):
        self.bits.append(1 if b else 0)

    def int(self, value, value_max):
        new, mask = 0, 1
        while new + mask < value_max and mask:
            b = value & mask
            self.bit(b)
            if b:
                new += mask
            mask <<= 1

    def bytes(self, data):
        for byte in data:
            for i in range(8):
                self.bit((byte >> i) & 1)

    def packet(self):
        bits = self.bits + [1]
        bits += [0] * (-len(bits) % 8)
        out = bytearray(len(bits) // 8)
        for i, b in enumerate(bits):
            if b:
                out[i >> 3] |= 1 << (i & 7)
        return bytes(out)


def compact_index(v):
    neg, v = v < 0, abs(v)
    out = [(0x80 if neg else 0) | (v & 0x3F) | (0x40 if v >> 6 else 0)]
    v >>= 6
    while v:
        out.append((v & 0x7F) | (0x80 if v >> 7 else 0))
        v >>= 7
    return bytes(out)


def fstring(text):
    """An FString as the original writes one: its length with the
    terminator, then its 8-bit characters and the terminator."""
    return compact_index(len(text) + 1) + text.encode("latin-1") + b"\0"


def read_fstrings(data):
    texts, pos = [], 0
    while pos < len(data):
        b0 = data[pos]
        pos += 1
        value, shift = b0 & 0x3F, 6
        more = b0 & 0x40
        while more and pos < len(data):
            b = data[pos]
            pos += 1
            value |= (b & 0x7F) << shift
            shift += 7
            more = b & 0x80
        count = -value if b0 & 0x80 else value
        if count >= 0:
            raw = data[pos:pos + count]
            pos += count
            texts.append(raw.split(b"\0")[0].decode("latin-1"))
        else:
            raw = data[pos:pos - 2 * count]
            pos += -2 * count
            texts.append(raw.decode("utf-16-le").split("\0")[0])
    return texts


def decode(data):
    """(packet id, bunches) of a datagram; each bunch a dict with its data."""
    if not data or data[-1] == 0:
        raise ValueError("no trailing bit")
    last, num = data[-1], len(data) * 8 - 1
    while not last & 0x80:
        last = (last << 1) & 0xFF
        num -= 1
    r = Reader(data, num)
    packet = r.int(MAX_PACKET_ID)
    bunches = []
    while r.pos < r.num:
        if r.bit():
            r.int(MAX_PACKET_ID)
            continue
        b = {"control": r.bit()}
        b["open"] = r.bit() if b["control"] else 0
        b["close"] = r.bit() if b["control"] else 0
        b["reliable"] = r.bit()
        b["ch"] = r.int(MAX_CHANNELS)
        b["seq"] = r.int(MAX_CH_SEQUENCE) if b["reliable"] else None
        b["type"] = r.int(MAX_CH_TYPE) if (b["reliable"] or b["open"]) else None
        bits = r.int(MAX_PACKET * 8)
        b["data"] = r.bytes(bits)
        bunches.append(b)
    return packet, bunches


def challenge_response(challenge):
    """The engine's answer to a challenge (DXEngine's NetChallengeResponse)."""
    c = challenge & 0xFFFFFFFF
    v = ((c * 237) ^ (challenge >> 16) ^ (c << 16) ^ 0x93FE92CE) & 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


class Client:
    def __init__(self):
        self.out_id = 1
        self.out_reliable = 0
        self.in_reliable = 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", type=int, default=7790)
    ap.add_argument("--mode", choices=["upgrade", "values"], default="upgrade")
    ap.add_argument("--minver", type=int, default=400)
    ap.add_argument("--ver", type=int, default=436)
    ap.add_argument("--secs", type=float, default=120.0)
    args = ap.parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", args.port))
    s.settimeout(0.1)
    clients = {}
    start = time.monotonic()
    print("server: %s mode on %d" % (args.mode, args.port), flush=True)
    while time.monotonic() - start < args.secs:
        try:
            data, addr = s.recvfrom(65536)
        except socket.timeout:
            continue
        c = clients.get(addr)
        if c is None:
            c = clients[addr] = Client()
            print("server: client %s:%d" % addr, flush=True)
        try:
            packet, bunches = decode(data)
        except ValueError as e:
            print("server: undecoded packet (%s)" % e, flush=True)
            continue
        replies = []
        for b in bunches:
            if b["ch"] != 0:
                continue
            if b["close"]:
                print("server: client closed its control channel", flush=True)
            if not b["reliable"] or b["seq"] != c.in_reliable + 1:
                continue
            c.in_reliable = b["seq"]
            for text in read_fstrings(b["data"]):
                print("server: client: %s" % text, flush=True)
                word = text.split(" ", 1)[0].upper()
                if word == "HELLO" and args.mode == "upgrade":
                    replies.append("UPGRADE MINVER=%d VER=%d" % (args.minver, args.ver))
                elif word == "HELLO":
                    replies.append("CHALLENGE VER= 1100 CHALLENGE= 7 STATS= 1")
                elif word == "LOGIN":
                    for part in text.split(" "):
                        if part.upper().startswith("RESPONSE="):
                            r = int(part[9:])
                            print("server: the response is the answer to %s" % (
                                "7" if r == challenge_response(7) else "0" if r == challenge_response(0) else "neither 7 nor 0"), flush=True)
                    replies.append("\tFAILURE\t\tspaced text")
        w = Writer()
        w.int(c.out_id, MAX_PACKET_ID)
        w.bit(1)
        w.int(packet, MAX_PACKET_ID)
        for text in replies:
            print("server: sent: %r" % text, flush=True)
            data = fstring(text)
            c.out_reliable += 1
            w.bit(0)
            w.bit(0)
            w.bit(1)
            w.int(0, MAX_CHANNELS)
            w.int(c.out_reliable, MAX_CH_SEQUENCE)
            w.int(CONTROL, MAX_CH_TYPE)
            w.int(len(data) * 8, MAX_PACKET * 8)
            w.bytes(data)
        s.sendto(w.packet(), addr)
        c.out_id = (c.out_id + 1) % MAX_PACKET_ID
    print("server: exiting", flush=True)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
