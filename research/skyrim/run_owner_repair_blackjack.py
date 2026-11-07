"""Send the Skyrim ownership repair payload once to the PS5 ELF loader.
--mode dry (default) changes nothing; --mode apply performs the repair.
Output is saved to evidence/blackjack-owner-repair-<mode>.json. No retries, no kills."""
import argparse, hashlib, json, socket, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", required=True); ap.add_argument("--port", type=int, default=9021)
    ap.add_argument("--mode", choices=["dry", "apply"], default="dry"); ap.add_argument("--run", action="store_true")
    a = ap.parse_args()
    elf = ROOT / f"build/skyrim-owner-repair/bj-repair-apply{1 if a.mode == 'apply' else 0}.elf"
    data = elf.read_bytes()
    if not data.startswith(b"\x7fELF") or len(data) > 1024 * 1024: raise RuntimeError("bad artifact")
    digest = hashlib.sha256(data).hexdigest()
    print("mode:", a.mode, "sha256:", digest)
    if not a.run: print("Plan only; pass --run."); return
    out = bytearray(); deadline = time.monotonic() + 60
    with socket.create_connection((a.host, a.port), timeout=3) as c:
        c.sendall(data); c.shutdown(socket.SHUT_WR)
        while time.monotonic() < deadline:
            c.settimeout(min(1.0, max(.01, deadline - time.monotonic())))
            try: part = c.recv(4096)
            except socket.timeout: continue
            if not part: break
            out.extend(part)
    text = out.decode(errors="replace"); print(text)
    (ROOT / f"evidence/blackjack-owner-repair-{a.mode}.json").write_text(json.dumps(
        {"mode": a.mode, "payload_sha256": digest, "output": text, "complete": "OBS done" in text}, indent=1))
    if "OBS done" not in text: raise RuntimeError("completion not observed; no retry attempted")
if __name__ == "__main__": main()
