#!/usr/bin/env python3
"""Host-only, byte-oriented Unix-socket to FT232H SPI bridge."""
import argparse
import os
import socket
import sys


def exchange(port, data):
    if not 1 <= len(data) <= 6:
        raise ValueError("transaction length must be 1..6 bytes")
    try:
        received = bytes(port.exchange(data, duplex=True, start=True, stop=True))
        if len(received) != len(data):
            raise OSError("short SPI transfer")
        return b"\x00" + received
    except Exception as error:
        print(f"SPI transfer failed: {error}", file=sys.stderr)
        return b"\x01" + bytes(len(data))


def receive_exact(connection, size):
    result = bytearray()
    while len(result) < size:
        part = connection.recv(size - len(result))
        if not part:
            raise EOFError("connection closed during request")
        result.extend(part)
    return bytes(result)


def handle_connection(connection, port):
    connection.settimeout(2.0)
    while True:
        header = connection.recv(1)
        if not header:
            return
        size = header[0]
        if not 1 <= size <= 6:
            raise ValueError("invalid transaction length")
        connection.sendall(exchange(port, receive_exact(connection, size)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--socket", required=True, help="new socket path in a private directory")
    parser.add_argument("--url", default="ftdi://ftdi:232h/1")
    parser.add_argument("--frequency", type=int, default=1_000_000)
    parser.add_argument("--mode", type=int, choices=(0, 1, 2, 3), default=2)
    args = parser.parse_args()
    if not 1 <= args.frequency <= 10_000_000:
        parser.error("frequency must be 1..10000000 Hz")
    # Lazy import: reusable C++ modules and protocol tests need no USB packages.
    from pyftdi.spi import SpiController
    controller = SpiController(cs_count=1)
    bound = False
    try:
        controller.configure(args.url)
        port = controller.get_port(cs=0, freq=args.frequency, mode=args.mode)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
            server.bind(args.socket)  # Never unlink an existing user's path.
            bound = True
            os.chmod(args.socket, 0o600)
            server.listen(1)
            print(f"FT232H bridge ready: {args.socket}", flush=True)
            while True:
                connection, _ = server.accept()
                with connection:
                    try:
                        handle_connection(connection, port)
                    except (OSError, ValueError, EOFError) as error:
                        print(f"Client disconnected: {error}", file=sys.stderr)
    except KeyboardInterrupt:
        pass
    finally:
        controller.terminate()
        if bound:
            os.unlink(args.socket)


if __name__ == "__main__":
    main()
