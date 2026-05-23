#!/usr/bin/env python3
"""Integration test for the C++ chat server.

Starts the compiled chat-server, opens several raw TCP connections (acting as
clients), and verifies multi-client broadcast, usernames, private messages,
the /who command, and join/leave notifications.
"""
import socket
import subprocess
import sys
import time

PORT = 5599
HOST = "127.0.0.1"


def recv_lines(sock, expected, timeout=2.0):
    """Read until we have `expected` newline-terminated lines or time out."""
    sock.settimeout(timeout)
    buf = b""
    lines = []
    deadline = time.time() + timeout
    while len(lines) < expected and time.time() < deadline:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                lines.append(line.decode().rstrip("\r"))
        except socket.timeout:
            break
    return lines


def main():
    server = subprocess.Popen(
        ["./bin/chat-server", str(PORT)],
        stderr=subprocess.PIPE,
        stdout=subprocess.PIPE,
    )
    time.sleep(0.5)  # give the server time to bind

    failures = []

    def check(cond, label):
        status = "PASS" if cond else "FAIL"
        print(f"  [{status}] {label}")
        if not cond:
            failures.append(label)

    try:
        # --- Connect alice ---
        alice = socket.create_connection((HOST, PORT))
        alice.sendall(b"/nick alice\n")
        time.sleep(0.3)
        alice_lines = recv_lines(alice, 10, timeout=0.8)
        check(any("alice" in l for l in alice_lines), "alice gets welcome/nick ack")

        # --- Connect bob (alice's buffer is now empty, so the join line
        #     that follows is captured cleanly) ---
        bob = socket.create_connection((HOST, PORT))
        bob.sendall(b"/nick bob\n")
        time.sleep(0.3)
        recv_lines(bob, 10, timeout=0.8)

        # alice should see bob arrive (a join line for the guest id, then a
        # rename line revealing the chosen name "bob")
        alice_join = recv_lines(alice, 10, timeout=0.8)
        joined_text = " ".join(alice_join)
        check("joined" in joined_text and "bob" in joined_text,
              "alice notified of bob joining")

        # --- Broadcast from alice ---
        alice.sendall(b"hello everyone\n")
        time.sleep(0.3)
        bob_recv = recv_lines(bob, 10, timeout=0.8)
        check(any("[alice] hello everyone" in l for l in bob_recv),
              "bob receives alice's broadcast")

        # --- /who command ---
        bob.sendall(b"/who\n")
        time.sleep(0.3)
        who = recv_lines(bob, 10, timeout=0.8)
        check(any("alice" in l and "bob" in l for l in who),
              "/who lists both users")

        # --- Private message bob -> alice ---
        bob.sendall(b"/msg alice secret\n")
        time.sleep(0.3)
        alice_pm = recv_lines(alice, 10, timeout=0.8)
        check(any("pm from bob" in l and "secret" in l for l in alice_pm),
              "alice receives private message from bob")

        # --- Duplicate username rejected ---
        carol = socket.create_connection((HOST, PORT))
        recv_lines(carol, 10, timeout=0.8)
        carol.sendall(b"/nick alice\n")
        time.sleep(0.3)
        carol_lines = recv_lines(carol, 10, timeout=0.8)
        check(any("taken" in l for l in carol_lines),
              "duplicate username rejected")

        # --- Leave notification ---
        bob.sendall(b"/quit\n")
        time.sleep(0.3)
        bob.close()
        time.sleep(0.3)
        alice_leave = recv_lines(alice, 10, timeout=0.8)
        check(any("bob" in l and "left" in l for l in alice_leave),
              "alice notified of bob leaving")

        alice.close()
        carol.close()

    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()

    print()
    if failures:
        print(f"{len(failures)} test(s) FAILED")
        sys.exit(1)
    print("All tests PASSED")


if __name__ == "__main__":
    main()
