#!/usr/bin/env python3
"""A demo file's records, as the original's demo driver writes them
(UDemoRecConnection::LowLevelSend, Engine.dll 0x10406a70): each packet's frame
(int32), the driver's time (double), its size (int32) and its bytes. Each
packet is decoded as the game's protocol has it (dx-reverse-info/network.md,
packets): its number, then acks and bunches; the control channel's bunches
are read as their lines of text.

  demdump.py <file.dem>              the text lines, then a summary
  demdump.py --packets <file.dem>    every packet and bunch as well
  demdump.py --summary <file.dem>    the summary alone

Draft for bugs.md WIN.30's proof: run it on the original's demo and on
DXEngine's, recorded by the same console, and compare the text lines (the
USES, WELCOME, DYNAMICRATE and STATICRATE), the channels each opens in its
first frames, and the frames and times.
"""
import struct
import sys

MAX_PACKET_ID = 16384
MAX_CHANNELS = 1023
MAX_CH_SEQUENCE = 1024
MAX_CH_TYPE = 8
MAX_PACKET_BITS = 512 * 8
TYPES = {0: 'none', 1: 'control', 2: 'actor', 3: 'file'}


class Bits:
    def __init__(self, data, num_bits):
        self.data = data
        self.num = num_bits
        self.pos = 0
        self.error = False

    def bit(self):
        if self.pos >= self.num:
            self.error = True
            return 0
        b = (self.data[self.pos >> 3] >> (self.pos & 7)) & 1
        self.pos += 1
        return b

    def int_max(self, value_max):
        value = 0
        mask = 1
        while value + mask < value_max and mask:
            if self.bit():
                value |= mask
            if self.error:
                break
            mask <<= 1
        return value

    def bytes_(self, count):
        out = bytearray()
        for _ in range(count):
            v = 0
            for i in range(8):
                v |= self.bit() << i
            out.append(v)
        return bytes(out)

    def sub(self, num_bits):
        if self.pos + num_bits > self.num:
            self.error = True
            return Bits(b'', 0)
        start = self.pos
        out = bytearray((num_bits + 7) >> 3)
        for i in range(num_bits):
            if (self.data[(start + i) >> 3] >> ((start + i) & 7)) & 1:
                out[i >> 3] |= 1 << (i & 7)
        self.pos += num_bits
        return Bits(bytes(out), num_bits)

    def at_end(self):
        return self.error or self.pos >= self.num

    def compact_index(self):
        b0 = self.bytes_(1)[0]
        value = b0 & 0x3f
        if b0 & 0x40:
            shift = 6
            for _ in range(4):
                b = self.bytes_(1)[0]
                value |= (b & 0x7f) << shift
                shift += 7
                if not b & 0x80:
                    break
        return -value if b0 & 0x80 else value

    def string(self):
        count = self.compact_index()
        if self.error or count == 0:
            return ''
        if abs(count) > 1 << 20:
            self.error = True
            return ''
        chars = []
        for _ in range(abs(count)):
            if count > 0:
                c = self.bytes_(1)[0]
            else:
                lo, hi = self.bytes_(2)
                c = lo | (hi << 8)
            chars.append(c)
        text = []
        for c in chars:
            if c == 0:
                break
            text.append(chr(c))
        return ''.join(text)


def read_records(path):
    data = open(path, 'rb').read()
    pos = 0
    records = []
    while pos < len(data):
        if pos + 16 > len(data):
            records.append(('short', pos, None))
            break
        frame, time, size = struct.unpack_from('<idi', data, pos)
        pos += 16
        if size < 0 or pos + size > len(data):
            records.append(('bad', frame, time, size))
            break
        records.append(('packet', frame, time, data[pos:pos + size]))
        pos += size
    return records


def decode(packet):
    if not packet:
        return None, [], 'empty'
    last = packet[-1]
    if last == 0:
        return None, [], 'no trailing bit'
    num_bits = len(packet) * 8 - 1
    while not last & 0x80:
        last = (last << 1) & 0xff
        num_bits -= 1
    r = Bits(packet, num_bits)
    packet_id = r.int_max(MAX_PACKET_ID)
    items = []
    while not r.at_end():
        if r.bit():
            items.append(('ack', r.int_max(MAX_PACKET_ID)))
            continue
        control = r.bit()
        b_open = r.bit() if control else 0
        b_close = r.bit() if control else 0
        reliable = r.bit()
        ch = r.int_max(MAX_CHANNELS)
        seq = r.int_max(MAX_CH_SEQUENCE) if reliable else None
        ch_type = r.int_max(MAX_CH_TYPE) if (reliable or b_open) else None
        num = r.int_max(MAX_PACKET_BITS)
        body = r.sub(num)
        if r.error:
            return packet_id, items, 'bunch overflowed'
        items.append(('bunch', ch, ch_type, b_open, b_close, reliable, seq, num, body))
    return packet_id, items, 'error' if r.error else None


def main(argv):
    show_packets = '--packets' in argv
    summary_only = '--summary' in argv
    paths = [a for a in argv if not a.startswith('--')]
    if len(paths) != 1:
        print(__doc__)
        return 2
    records = read_records(paths[0])
    channel_types = {}
    opens = {}
    closes = {}
    packets = 0
    bunches = 0
    acks = 0
    frames = set()
    first_time = last_time = None
    texts = []
    for rec in records:
        if rec[0] != 'packet':
            print('END:', rec)
            continue
        _, frame, time, packet = rec
        packets += 1
        frames.add(frame)
        first_time = time if first_time is None else first_time
        last_time = time
        packet_id, items, err = decode(packet)
        if show_packets:
            print(f'packet {packet_id} frame {frame} time {time:.4f} bytes {len(packet)}' + (f' [{err}]' if err else ''))
        for item in items:
            if item[0] == 'ack':
                acks += 1
                if show_packets:
                    print(f'  ack {item[1]}')
                continue
            _, ch, ch_type, b_open, b_close, reliable, seq, num, body = item
            bunches += 1
            if ch_type is not None:
                channel_types[ch] = ch_type
            kind = TYPES.get(channel_types.get(ch, 0), str(channel_types.get(ch)))
            if b_open:
                opens[kind] = opens.get(kind, 0) + 1
            if b_close:
                closes[kind] = closes.get(kind, 0) + 1
            if show_packets:
                print(f'  bunch ch {ch} {kind}' + (' open' if b_open else '') + (' close' if b_close else '') +
                      (f' reliable {seq}' if reliable else '') + f' bits {num}')
            if channel_types.get(ch) == 1 and num:
                while not body.at_end():
                    line = body.string()
                    if body.error:
                        break
                    texts.append((frame, line))
                    if show_packets:
                        print(f'    text: {line}')
    if not summary_only:
        for frame, line in texts:
            print(f'TEXT frame {frame}: {line}')
    print(f'SUMMARY packets {packets} bunches {bunches} acks {acks} frames {len(frames)}'
          f' ({min(frames) if frames else "-"}..{max(frames) if frames else "-"})'
          f' time {first_time}..{last_time}')
    print(f'SUMMARY channels opened {dict(sorted(opens.items()))} closed {dict(sorted(closes.items()))}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
