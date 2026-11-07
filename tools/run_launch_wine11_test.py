"""Start the already-installed isolated Wine 11 Steam Test title once."""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request
from install_console import WFM

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "build/wine11-test-launch/launch.elf"
EVIDENCE = ROOT / "evidence/wine11-test-title-launch.json"
EXPECTED = "db642b851b23e45e0478bae667e4c5582284227cb91b8e71310c5d5df169af6b"

def main():
    if EVIDENCE.exists(): raise RuntimeError("Launch already attempted; inspect before any retry")
    data=PROGRAM.read_bytes()
    if hashlib.sha256(data).hexdigest()!=EXPECTED or not data.startswith(b"\x7fELF"):
        raise RuntimeError("Launcher differs from reviewed binary")
    status=json.loads((ROOT/"build/steam-test-title-update-status.json").read_text())
    if status.get("phase")!="updated": raise RuntimeError("Updated test title is not verified")
    wfm=WFM("192.168.7.26",8888)
    if hashlib.sha256(wfm.read("/data/homebrew/PPSA77382/eboot.bin")).hexdigest()!=status["new_eboot_sha256"]:
        raise RuntimeError("Test executable changed")
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list",timeout=5) as response:
        processes=json.load(response).get("processes",[])
    if any(str(p.get("name","")).lower() in ("payload.elf","eboot.bin") for p in processes):
        raise RuntimeError("Another app or loader payload is active; no launch attempted")
    state={"status":"dispatching","launcher_sha256":EXPECTED,"title":"PPSA77382","automatic_retry":False}
    with EVIDENCE.open("x") as file: json.dump(state,file,indent=2)
    output=bytearray(); error=""; done=False
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
                if b"WINE11_TEST_LAUNCH done" in output: done=True; break
        if not done: error="Launch outcome unknown; inspect console before any retry"
    except (OSError,RuntimeError) as exc: error=str(exc)
    text=output.decode("utf-8",errors="replace")
    state.update(status="accepted" if "WINE11_TEST_LAUNCH ok=1 " in text else "inspect",output=text,error=error,completed=done)
    EVIDENCE.write_text(json.dumps(state,indent=2)+"\n")
    print(text,end="" if text.endswith("\n") else "\n")
    if error or state["status"]!="accepted": raise RuntimeError(error or "Launch not accepted")

if __name__=="__main__": main()
