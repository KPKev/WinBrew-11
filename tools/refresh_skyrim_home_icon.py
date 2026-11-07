"""Safely refresh only Skyrim's existing PS5 title icon, with backups.

This one-time repair keeps the game, bottle and title identity intact. Each
target is staged and read back before any original is renamed. Do not retry an
interrupted run without inspecting its journal and the console paths.
"""
import hashlib
import json
import urllib.parse
import urllib.request
from pathlib import Path
from install_console import WFM
from probe_ps5_kit_extraction import post, wait

ROOT = Path(__file__).resolve().parents[1]
ICON = ROOT / "build/skyrim-crash-investigation/icon0.png"
STATE = ROOT / "build/skyrim-crash-investigation/icon-refresh.json"
TARGETS = (
    ("/data/playden/games/wine11-steam-489830/title-metadata/sce_sys", "0600", False),
    ("/data/homebrew/FAKE78005/sce_sys", "0644", True),
    ("/user/appmeta/FAKE78005", "0666", True),
)
TAG = "wine11-skyrim-icon"


def sha(data): return hashlib.sha256(data).hexdigest()


def require_stopped():
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as response:
        processes = json.load(response).get("processes", [])
    if any(str(p.get("name", "")).lower() == "eboot.bin" for p in processes):
        raise RuntimeError("Close the running PS5 app before updating the icon")


def main():
    if STATE.exists(): raise RuntimeError("Inspect the existing icon-refresh journal before retrying")
    require_stopped()
    w = WFM("192.168.7.26")
    descriptor = json.loads(w.read("/data/homebrew/FAKE78005/game-launch.json"))
    if descriptor.get("title_id") != "FAKE78005" or descriptor.get("app_id") != 489830 or descriptor.get("install_id") != "wine11-steam-489830":
        raise RuntimeError("The title does not identify the expected Skyrim installation")
    new = ICON.read_bytes()
    if not new.startswith(b"\x89PNG\r\n\x1a\n") or len(new) > 4 * 1024 * 1024:
        raise RuntimeError("The prepared image is not a bounded PNG")
    originals = []
    for parent, mode, existing in TARGETS:
        entries = {entry["name"]: entry for entry in w.listing(parent)}
        if ("icon0.png" in entries) != existing or "icon0.png." + TAG + ".stage" in entries or "icon0.png.before-" + TAG + ".bak" in entries:
            raise RuntimeError("Icon path differs from the expected state: " + parent)
        originals.append(sha(w.read(parent + "/icon0.png")) if existing else None)
    existing_hashes = {digest for digest in originals if digest is not None}
    if len(existing_hashes) > 1: raise RuntimeError("Existing icons differ; inspect before refreshing")
    state = {"phase": "staging", "expected": sha(new), "originals": originals, "staged": [], "changes": []}
    STATE.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    try:
        for parent, mode, existing in TARGETS:
            name = "icon0.png." + TAG + ".stage"
            task = post('/api/upload/prepare', {'path': parent, 'src': name, 'total': str(len(new)), 'count': '1', 'rels': name, 'sizes': str(len(new)), 'overwrite': '0'})['task_id']
            post('/api/upload-file', new, headers={'Content-Type': 'application/octet-stream', 'X-WFM-Task-ID': str(task), 'X-WFM-Path': urllib.parse.quote(parent, safe=''), 'X-WFM-Rel': name, 'X-WFM-Size': str(len(new)), 'X-WFM-Overwrite': '0'})
            post('/api/upload/finish', params={'task_id': task}); wait(task)
            staged = parent + '/' + name
            if sha(w.read(staged)) != state['expected']: raise RuntimeError('Staged icon differs')
            task = post('/api/chmod', {'paths': staged, 'mode': mode, 'recursive': '0'})['task_id']; wait(task)
            state['staged'].append(parent); STATE.write_text(json.dumps(state, indent=2) + '\n')
        require_stopped()
        # Check originals again immediately before any replacement.
        for i, (parent, _, existing) in enumerate(TARGETS):
            if existing and sha(w.read(parent + '/icon0.png')) != originals[i]: raise RuntimeError('Original icon changed while staging')
        state['phase'] = 'applying'; STATE.write_text(json.dumps(state, indent=2) + '\n')
        for parent, _, existing in TARGETS:
            require_stopped()
            if existing:
                post('/api/rename', params={'path': parent + '/icon0.png', 'name': 'icon0.png.before-' + TAG + '.bak'})
                state['changes'].append([parent, 'backup']); STATE.write_text(json.dumps(state, indent=2) + '\n')
            post('/api/rename', params={'path': parent + '/icon0.png.' + TAG + '.stage', 'name': 'icon0.png'})
            state['changes'].append([parent, 'applied']); STATE.write_text(json.dumps(state, indent=2) + '\n')
            if sha(w.read(parent + '/icon0.png')) != state['expected']: raise RuntimeError('Applied icon differs')
        state['phase'] = 'updated'; STATE.write_text(json.dumps(state, indent=2) + '\n')
        print('Skyrim icon updated and read-back verified; original icons retained as backups.')
    except Exception as exc:
        state.update(phase='inspect-before-retry', error=str(exc)); STATE.write_text(json.dumps(state, indent=2) + '\n')
        raise


if __name__ == '__main__': main()
