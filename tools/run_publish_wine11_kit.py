"""Publish the previously hash-verified kit into its new dedicated location once."""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "build/steam-kit-publish/publish.elf"
EVIDENCE = ROOT / "evidence/steam-kit-publication-console.json"
EXPECTED = "fe9636be4c67adaa0776017de26aecb5bbeac193bc028cdde31c124c282d0d54"

def main():
    data = PROBE.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED or not data.startswith(b"\x7fELF"):
        raise RuntimeError("Publication program differs from reviewed binary")
    if EVIDENCE.exists(): raise RuntimeError("Inspect existing publication record; no automatic retry")
    admission = json.loads((ROOT / "evidence/steam-staged-kit-console-admission.json").read_text())
    hashes = json.loads((ROOT / "evidence/steam-staged-kit-console-hashes.json").read_text())
    modes = json.loads((ROOT / "build/steam-kit-mode-recovery.json").read_text())
    if not admission.get("admission_verified") or not hashes.get("hashes_verified") or modes.get("phase") != "inventory-and-modes-verified":
        raise RuntimeError("Console staging verification is incomplete")
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as response:
        processes = json.load(response).get("processes", [])
    if any(str(p.get("name", "")).lower() == "payload.elf" for p in processes):
        raise RuntimeError("Another loader payload is active")
    state = {"probe_sha256":EXPECTED, "status":"dispatching", "source":"/data/wine11-kit-stage-0f803ada2eee/playden-kit",
             "destination":"/data/wine11/kits/playden-kit", "overwrite":False, "automatic_retry":False}
    with EVIDENCE.open("x", encoding="utf-8") as file: json.dump(state,file,indent=2)
    output=bytearray(); error=""; complete=False
    try:
        with socket.create_connection(("192.168.7.26",9021),timeout=5) as conn:
            conn.sendall(data); conn.shutdown(socket.SHUT_WR); deadline=time.monotonic()+30
            while time.monotonic()<deadline:
                conn.settimeout(min(1,deadline-time.monotonic()))
                try: chunk=conn.recv(4096)
                except socket.timeout: continue
                if not chunk: break
                output.extend(chunk)
                if len(output)>4096: raise RuntimeError("Output limit exceeded")
                if b"WINE11_KIT_PUBLISH done" in output or b"WINE11_KIT_PUBLISH failed=" in output: complete=True; break
        if not complete: error="Completion unknown; inspect before any retry"
    except (OSError,RuntimeError) as exc: error=str(exc)
    text=output.decode("utf-8",errors="replace")
    state.update(status="published" if "WINE11_KIT_PUBLISH done installed=1" in text else "inspect",
                 output=text,error=error,completed=complete)
    EVIDENCE.write_text(json.dumps(state,indent=2)+"\n")
    print(text,end="" if text.endswith("\n") else "\n")
    if error or state["status"]!="published": raise RuntimeError(error or "Publication not confirmed")

if __name__ == "__main__": main()
