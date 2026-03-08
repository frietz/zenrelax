#!/usr/bin/env python3
"""Record a zenrelax mode to an asciicast v2 (.cast) file.

Creates a pty with the correct size, runs zenrelax inside it, captures
all output with timestamps, and writes asciicast v2 format. This avoids
conflicts between asciinema's pty handling and zenrelax's raw terminal mode.
"""
import argparse
import fcntl
import json
import os
import select
import signal
import struct
import sys
import termios
import time


def record(mode, duration, cols, rows, output):
    master_fd, slave_fd = os.openpty()

    # Set pty size before forking so zenrelax sees it immediately
    winsize = struct.pack("HHHH", rows, cols, 0, 0)
    fcntl.ioctl(slave_fd, termios.TIOCSWINSZ, winsize)

    pid = os.fork()
    if pid == 0:
        # Child: set up slave pty as stdin/stdout/stderr
        os.close(master_fd)
        os.setsid()
        fcntl.ioctl(slave_fd, termios.TIOCSCTTY, 0)
        os.dup2(slave_fd, 0)
        os.dup2(slave_fd, 1)
        os.dup2(slave_fd, 2)
        if slave_fd > 2:
            os.close(slave_fd)
        os.environ["TERM"] = "xterm-256color"
        os.execvp("./zenrelax", ["./zenrelax", str(mode)])

    # Parent: record output from master fd
    os.close(slave_fd)
    events = []
    start = time.time()

    while time.time() - start < duration:
        r, _, _ = select.select([master_fd], [], [], 0.05)
        if r:
            try:
                data = os.read(master_fd, 65536)
                if data:
                    t = time.time() - start
                    events.append(
                        [round(t, 6), "o", data.decode("utf-8", errors="replace")]
                    )
            except OSError:
                break

    # Stop zenrelax cleanly
    os.kill(pid, signal.SIGINT)

    # Drain remaining output
    deadline = time.time() + 0.5
    while time.time() < deadline:
        r, _, _ = select.select([master_fd], [], [], 0.1)
        if not r:
            break
        try:
            data = os.read(master_fd, 65536)
            if not data:
                break
        except OSError:
            break

    os.waitpid(pid, 0)
    os.close(master_fd)

    # Write asciicast v2
    with open(output, "w") as f:
        header = {"version": 2, "width": cols, "height": rows}
        f.write(json.dumps(header) + "\n")
        for event in events:
            f.write(json.dumps(event) + "\n")

    print(f"  Recorded {len(events)} events, {duration}s -> {output}")


def main():
    p = argparse.ArgumentParser(description="Record zenrelax to asciicast v2")
    p.add_argument("--mode", type=int, required=True)
    p.add_argument("--duration", type=float, default=5)
    p.add_argument("--cols", type=int, default=80)
    p.add_argument("--rows", type=int, default=24)
    p.add_argument("--output", required=True)
    args = p.parse_args()
    record(args.mode, args.duration, args.cols, args.rows, args.output)


if __name__ == "__main__":
    main()
