"""Run a single read-only content-hash check of the staged kit on the PS5."""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "build/steam-staged-hash-probe/probe.elf"
EVIDENCE = ROOT / "evidence/steam-staged-kit-console-hashes.json"
EXPECTED = "290ecc3849c916fd40793a94d4e00f1b5d0ffeaeb54d02f264886b38f45ee655"

def main():
    data = PROBE.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED or not data.startswith(b"\x7fELF"):
        raise RuntimeError("Diagnostic differs from reviewed binary")
    if EVIDENCE.exists(): raise RuntimeError("Inspect existing evidence before retrying")
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as response:
        processes = json.load(response).get("processes", [])
    if any(str(p.get("name", "")).lower() == "payload.elf" for p in processes):
        raise RuntimeError("Another loader payload is active")
    output = bytearray(); completed = False; error = ""
    try:
        with socket.create_connection(("192.168.7.26", 9021), timeout=5) as conn:
            conn.sendall(data); conn.shutdown(socket.SHUT_WR)
            deadline = time.monotonic() + 300
            while time.monotonic() < deadline:
                conn.settimeout(min(1, deadline - time.monotonic()))
                try: chunk = conn.recv(4096)
                except socket.timeout: continue
                if not chunk: break
                output.extend(chunk)
                print(chunk.decode("utf-8", errors="replace"), end="", flush=True)
                if len(output) > 16384: raise RuntimeError("Output limit exceeded")
                if b"STAGED_HASH_PROBE done verified=" in output or b"STAGED_HASH_PROBE failure=" in output:
                    completed = True; break
        if not completed: error = "Completion not observed; inspect the console before any retry"
    except (OSError, RuntimeError) as exc: error = str(exc)
    text = output.decode("utf-8", errors="replace")
    verified = completed and "STAGED_HASH_PROBE done verified=4348" in text
    evidence = {"probe_sha256": EXPECTED, "console_host": "192.168.7.26", "loader_port": 9021,
                "completed": completed, "hashes_verified": verified, "output": text, "error": error,
                "writes": False, "elevation": False, "automatic_retry": False,
                "note": "Staged kit content validation only; helper and app launch are not console-qualified."}
    with EVIDENCE.open("x", encoding="utf-8") as file: json.dump(evidence, file, indent=2); file.write("\n")
    if error or not verified: raise RuntimeError(error or "Kit hash verification failed")

if __name__ == "__main__": main()
