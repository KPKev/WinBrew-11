#!/usr/bin/env bash
# Host test: builds owner_repair against a temp fixture as real root (in a user+mount namespace via unshare if available).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
mkdir -p "$tmp/title/win/sub" "$tmp/data/.playden-control" "$tmp/games/489830"
echo x > "$tmp/title/game-launch.json"; chmod 0600 "$tmp/title/game-launch.json"
echo y > "$tmp/title/win/sub/a.dll"; chmod 0644 "$tmp/title/win/sub/a.dll"
ln -s /etc/passwd "$tmp/title/link"
echo z > "$tmp/data/.playden-control/publication.lock"; chmod 0600 "$tmp/data/.playden-control/publication.lock"
chmod 0700 "$tmp/games"
for apply in 0 1; do
  cc -std=c11 -O2 -Wall -Wextra -Werror -DREPAIR_APPLY=$apply -DROOT_TITLE="\"$tmp/title\"" -DROOT_DATA="\"$tmp/data\"" -DTRAVERSE_DIR="\"$tmp/games\"" "$here/owner_repair.c" -o "$tmp/repair$apply"
done
python3 - "$tmp" <<'PY'
import socket,subprocess,sys,os
tmp=sys.argv[1]
def run(apply):
    a,b=socket.socketpair(); p=subprocess.Popen([f"{tmp}/repair{apply}"],stdout=b.fileno()); b.close(); p.wait()
    a.settimeout(2); d=b''
    while True:
        try: x=a.recv(65536)
        except Exception: break
        if not x: break
        d+=x
    return p.returncode,d.decode()
rc,o=run(0); print("DRY",rc); print(o)
assert "would_change=" in o and "apply=0" in o
st=os.lstat(f"{tmp}/title/game-launch.json"); assert st.st_uid==0, "dry run must not change"
rc,o=run(1); print("APPLY",rc); print(o)
for p in ["title","title/game-launch.json","title/win/sub/a.dll","data/.playden-control/publication.lock"]:
    st=os.lstat(f"{tmp}/{p}"); assert st.st_uid==1, p
assert oct(os.lstat(f"{tmp}/title/game-launch.json").st_mode & 0o7777)=="0o600"
assert os.lstat(f"{tmp}/title/link").st_uid==0, "symlink must be left alone"
assert oct(os.lstat(f"{tmp}/games").st_mode & 0o7777)=="0o711"
rc,o=run(1); print("REAPPLY",rc); print(o); assert "changed=0" in o
print("HOST TESTS PASS")
PY
rm -rf "$tmp"
