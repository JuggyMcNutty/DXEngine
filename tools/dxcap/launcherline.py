#!/usr/bin/env -S python3 -I
"""Stands for DXLauncher on a dxe run's launcher line (DXL_LAUNCHER_FD), so
that a run hands the running engine what DXLauncher hands it for a second
start's line (bugs.md D.96):

    DXCAP_ENGINE=tools/dxcap/launcherline.py tools/dxcap.sh dxe <console> <map> [<secs>]

It runs the engine, DXCAP_LINE_ENGINE's, else the linux-x86_64 build beside
the workspace (build/linux-x86_64/engine/DXEngine in the repositories'
parent folder), with the run's words, its end of a socket pair in
DXL_LAUNCHER_FD, as DXLauncher does. Once the engine says "ready" it waits
DXCAP_LINE_AFTER seconds (10) and sends each line of DXCAP_LINE_SEND, split
at ';' ("TakeFocus;OpenLink 127.0.0.1:7790"). Each line either way goes to
standard error, the run's engine.log: "LINE: < ready", "LINE: > OpenLink
127.0.0.1:7790". The process becomes the engine, its exit the engine's; the
line is kept by a child of its own, which ends with the engine.
"""
import os
import pathlib
import socket
import sys
import time


def main():
    engine = os.environ.get("DXCAP_LINE_ENGINE") or str(
        pathlib.Path(__file__).resolve().parents[3] / "build/linux-x86_64/engine/DXEngine")
    after = float(os.environ.get("DXCAP_LINE_AFTER", "10"))
    send = [line for line in os.environ.get(
        "DXCAP_LINE_SEND", "TakeFocus;OpenLink 127.0.0.1:7790").split(";") if line]
    ours, theirs = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    if os.fork() == 0:
        theirs.close()
        talk(ours, after, send)
        os._exit(0)
    ours.close()
    theirs.set_inheritable(True)
    env = dict(os.environ)
    env["DXL_LAUNCHER_FD"] = str(theirs.fileno())
    os.execve(engine, [engine] + sys.argv[1:], env)


def talk(line, after, send):
    """The engine's lines, logged; after its ready and the wait, ours."""
    pending = b""
    sent = False
    ready_at = None
    line.settimeout(0.25)
    while True:
        if ready_at is not None and not sent and time.monotonic() >= ready_at + after:
            sent = True
            try:
                for text in send:
                    print("LINE: > " + text, file=sys.stderr, flush=True)
                    line.sendall(text.encode() + b"\n")
            except OSError:
                return
        try:
            got = line.recv(4096)
        except socket.timeout:
            continue
        except OSError:
            return
        if not got:
            return
        pending += got
        while b"\n" in pending:
            text, pending = pending.split(b"\n", 1)
            text = text.decode(errors="replace").rstrip("\r")
            print("LINE: < " + text, file=sys.stderr, flush=True)
            if text == "ready" and ready_at is None:
                ready_at = time.monotonic()


if __name__ == "__main__":
    main()
