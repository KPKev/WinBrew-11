"""Run one isolated, read-only /data anchor diagnostic on the PS5."""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "build/steam-data-anchor-probe/probe.elf"
EVIDENCE = ROOT / "evidence/steam-data-anchor-console-probe.json"
EXPECTED = "f2b6db5e1534818bf9119a53422c68be2cdd9547784a2dec9789fe24c9ea1f93"

def main():
    data = PROBE.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED or not data.startswith(b"\x7fELF"):
        raise RuntimeError("Diagnostic differs from reviewed binary")
    if EVIDENCE.exists(): raise RuntimeError("Existing result: inspect it instead of retrying")
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as response:
        processes = json.load(response).get("processes", [])
    if any(str(p.get("name", "")).lower() == "payload.elf" for p in processes):
        raise RuntimeError("Another loader payload is active")
    output = bytearray(); error = ""; completed = False
    try:
        with socket.create_connection(("192.168.7.26", 9021), timeout=5) as connection:
            connection.sendall(data); connection.shutdown(socket.SHUT_WR)
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                connection.settimeout(min(1, deadline-time.monotonic()))
                try: chunk = connection.recv(4096)
                except socket.timeout: continue
                if not chunk: break
                output.extend(chunk)
                if len(output) > 4096: raise RuntimeError("Diagnostic output exceeded limit")
                if b"DATA_ANCHOR_PROBE done" in output: completed = True; break
        if not completed: error = "Completion not observed; do not retry without inspecting the console"
    except (OSError, RuntimeError) as exc: error = str(exc)
    text = output.decode("utf-8", errors="replace")
    evidence = {"probe_sha256": EXPECTED, "console_host": "192.168.7.26", "loader_port": 9021,
                "completed": completed, "output": text, "error": error,
                "writes": False, "elevation": False, "automatic_retry": False,
                "note": "Payload context only; does not prove access from the Wine 11 app sandbox."}
    with EVIDENCE.open("x", encoding="utf-8") as file: json.dump(evidence, file, indent=2); file.write("\n")
    print(text, end="" if text.endswith("\n") else "\n")
    if error: raise RuntimeError(error)

if __name__ == "__main__": main()
