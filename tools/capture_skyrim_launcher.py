"""Capture one Skyrim launcher diagnostic on the local network.

This listener does not start a game or alter the PS5. Run only after installing
the temporary, title-scoped dev.conf and remove that file after the test.
"""
import json
import socket
import time
from pathlib import Path

OUTPUT = Path(__file__).resolve().parents[1] / "build/skyrim-crash-investigation/launcher-capture.json"


def main():
    deadline = time.monotonic() + 300
    records = bytearray()
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", 9300))
    server.listen(1)
    server.settimeout(1)
    print("Listening for Skyrim launcher diagnostics", flush=True)
    try:
        while time.monotonic() < deadline:
            try:
                peer, address = server.accept()
            except socket.timeout:
                continue
            with peer:
                # Accept only the user's console, and bound all retained data.
                if address[0] != "192.168.7.26":
                    continue
                peer.settimeout(2)
                while len(records) < 256 * 1024 and time.monotonic() < deadline:
                    try:
                        data = peer.recv(min(4096, 256 * 1024 - len(records)))
                    except socket.timeout:
                        break
                    if not data:
                        break
                    records.extend(data)
                if records:
                    break
    finally:
        server.close()
    # Launcher logs can contain local paths. Keep them in the private workspace;
    # print only counts, and inspect narrowly for startup reasons.
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps({"received": bool(records), "text": records.decode("utf-8", "replace")}, indent=2), encoding="utf-8")
    print("Diagnostic bytes received:", len(records), flush=True)


if __name__ == "__main__":
    main()
