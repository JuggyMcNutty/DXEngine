#!/usr/bin/env python3
"""Asks a listen server on this machine what its LAN beacon and GameSpy query
answerer say -- the same questions for either engine's server, so that the
two can be diffed (docs/DEVELOPMENT.md, scripted runs: net tests).

    tools/dxcap/netquery.py [beacon port] [query port] [address]

The ports default to the beacon's 8777 and the query answerer's 7791 (the
first free one after the net tests' game port, 7790), the address to
127.0.0.1 (another of the loopback's, 127.0.0.2, for a server with
MULTIHOME=). Each question's replies are printed on one line.
"""
import socket
import sys
import time


def ask(port, text, host='127.0.0.1', wait=1.5):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(wait)
    s.sendto(text.encode('latin1'), (host, port))
    replies = []
    end = time.time() + wait
    while time.time() < end:
        try:
            data, _ = s.recvfrom(4096)
            replies.append(data.decode('latin1', 'replace'))
        except socket.timeout:
            break
    s.close()
    return replies


def main():
    beacon = int(sys.argv[1]) if len(sys.argv) > 1 else 8777
    query = int(sys.argv[2]) if len(sys.argv) > 2 else 7791
    host = sys.argv[3] if len(sys.argv) > 3 else '127.0.0.1'
    questions = [(beacon, 'REPORT'), (beacon, 'REPORTQUERY')]
    questions += [(query, q) for q in ('\\basic\\', '\\info\\', '\\rules\\', '\\players\\')]
    for port, text in questions:
        print(f'{port} {text!r}: {ask(port, text, host)!r}')


if __name__ == '__main__':
    main()
