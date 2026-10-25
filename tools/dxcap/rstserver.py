#!/usr/bin/env python3
"""rstserver.py: accepts TCP connections on 127.0.0.1:7877, reads each for a
second, then resets it (SO_LINGER 0): the peer of RstConsole's link
(docs/DEVELOPMENT.md). Serves until killed."""
import socket, struct, sys, time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', 7877))
s.listen(4)
print('listening', flush=True)
while True:
    c, a = s.accept()
    print('accepted', a, flush=True)
    c.settimeout(1.0)
    try:
        data = c.recv(256)
        print('got', data, flush=True)
    except socket.timeout:
        print('nothing read', flush=True)
    c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    c.close()
    print('reset', flush=True)
