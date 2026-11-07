"""Run one read-only kit admission probe against the isolated console staging folder."""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "build/steam-staged-kit-probe/probe.elf"
EVIDENCE = ROOT / "evidence/steam-staged-kit-console-admission.json"
EXPECTED = "09899ae3f44442a36bee5dd0c9a9c0a1553f253227b623d2b250746cc8087014"
MANIFEST = "1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc"

def main():
    data = PROBE.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED or not data.startswith(b"\x7fELF"):
        raise RuntimeError("Probe differs from reviewed binary")
    if EVIDENCE.exists(): raise RuntimeError("Inspect existing probe before any retry")
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as r:
        processes = json.load(r).get("processes", [])
    if any(str(p.get("name", "")).lower() == "payload.elf" for p in processes):
        raise RuntimeError("Another loader payload is active")
    output = bytearray(); completed = False; error = ""
    try:
        with socket.create_connection(("192.168.7.26", 9021), timeout=5) as conn:
            conn.sendall(data); conn.shutdown(socket.SHUT_WR)
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                conn.settimeout(min(1, deadline - time.monotonic()))
                try: chunk = conn.recv(4096)
                except socket.timeout: continue
                if not chunk: break
                output.extend(chunk)
                if len(output) > 8192: raise RuntimeError("Output limit exceeded")
                if b"STAGED_KIT_PROBE done" in output: completed = True; break
        if not completed: error = "Completion not observed; inspect the process before any retry"
    except (OSError, RuntimeError) as exc: error = str(exc)
    text = output.decode("utf-8", errors="replace")
    verified = completed and "STAGED_KIT_PROBE status=0 " in text and MANIFEST in text
    evidence = {"probe_sha256": EXPECTED, "console_host": "192.168.7.26", "loader_port": 9021,
                "completed": completed, "admission_verified": verified, "output": text, "error": error,
                "writes": False, "elevation": False, "automatic_retry": False,
                "note": "Payload-context structural admission only; not an app launch or full file-hash verification."}
    with EVIDENCE.open("x", encoding="utf-8") as file: json.dump(evidence, file, indent=2); file.write("\n")
    print(text, end="" if text.endswith("\n") else "\n")
    if error or not verified: raise RuntimeError(error or "Kit admission failed")

if __name__ == "__main__": main()
