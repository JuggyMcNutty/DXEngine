#!/usr/bin/env python3
"""lingerpeer.py: accepts TCP connections on 127.0.0.1:7879 and reads each
until it ends, then prints what it brought and how it ended: an end of file
(the link's FIN) or a reset (its RST). After an end of file it waits a second
and sends "bye", then prints whether the send went and what came back, so a
link still reading takes it and one closed abortively has reset it: the peer
of LingerConsole's links (docs/DEVELOPMENT.md). Serves until killed."""
import socket, threading, time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', 7879))
s.listen(4)
print('listening', flush=True)

def serve(c, a):
    got = b''
    c.settimeout(10.0)
    try:
        while True:
            data = c.recv(256)
            if not data:
                how = 'an end of file'
                break
            got += data
    except OSError as e:
        how = 'a reset' if isinstance(e, ConnectionResetError) else type(e).__name__
    print('%s brought %r, then %s' % (a[1], got, how), flush=True)
    if how == 'an end of file':
        time.sleep(1.0)
        try:
            c.sendall(b'bye')
            c.settimeout(2.0)
            back = c.recv(16)
            print('%s: bye sent, then %s' % (a[1], 'an end of file' if not back else repr(back)), flush=True)
        except socket.timeout:
            print('%s: bye sent, nothing back' % a[1], flush=True)
        except OSError as e:
            print('%s: bye refused, %s' % (a[1], type(e).__name__), flush=True)
    c.close()

while True:
    c, a = s.accept()
    print('accepted', a, flush=True)
    threading.Thread(target=serve, args=(c, a), daemon=True).start()
