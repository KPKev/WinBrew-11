"""Plan, or explicitly run, the read-only Vapor adoption preview (lstat only) on the PS5.

Default mode prints a local plan. --run sends exactly the built probe once to
an existing ELF loader, observes bounded stdout, and never retries or kills a
process. The probe itself returns after four timeout-limited public lookups.
"""
import argparse
import hashlib
from pathlib import Path
import socket
import time

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "build/vapor-adoption-preview/observer.elf"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=9021)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    data = PROBE.read_bytes()
    if not data.startswith(b"\x7fELF") or len(data) > 1024 * 1024:
        raise RuntimeError("Unexpected probe artifact; rebuild and inspect it")
    print("Reads: lstat/readdir of fixed title and data roots only")
    print("Console file writes: none; credential changes: none")
    print("Probe SHA-256:", hashlib.sha256(data).hexdigest())
    print("ELF loader:", args.host, args.port)
    if not args.run:
        print("Plan only. --run is required to send the probe.")
        return
    output = bytearray()
    deadline = time.monotonic() + 60
    with socket.create_connection((args.host, args.port), timeout=3) as connection:
        connection.sendall(data)
        connection.shutdown(socket.SHUT_WR)
        while time.monotonic() < deadline:
            connection.settimeout(min(1.0, max(.01, deadline - time.monotonic())))
            try:
                part = connection.recv(4096)
            except socket.timeout:
                continue
            if not part:
                break
            if len(output) + len(part) > 256 * 1024:
                raise RuntimeError("Probe output exceeded the diagnostic limit; inspect before retrying")
            output.extend(part)
    text = output.decode(errors="replace")
    import json
    (ROOT / "evidence/vapor-adoption-preview.json").write_text(json.dumps({"probe_sha256": hashlib.sha256(data).hexdigest(), "output": text, "complete": "OBS done" in text}, indent=1))
    print(text, end="" if text.endswith("\n") else "\n")
    if "OBS done" not in text:
        raise RuntimeError("Completion was not observed. No retry or process termination was attempted.")

if __name__ == "__main__":
    main()
