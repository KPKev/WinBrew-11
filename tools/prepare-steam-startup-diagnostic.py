"""Prepare an isolated diagnostic title build without changing the running app."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
generated = root / "build/steam-native-experiment/ps5.cpp"
source = generated.read_text()
if source.count("PPSA77381") != 2 or source.count("Steam data anchor error") != 1 or source.count("Steam game startup state") != 1:
    raise ValueError("Unexpected native diagnostic source")
out = root / "build/steam-startup-diagnostic"
out.mkdir(exist_ok=True)
(out / "ps5-after-root-test.cpp").write_text(source.replace("PPSA77381", "PPSA77382"))
original = (root / "tools/probe-steam-native-pie.sh").read_text()
old = '  for object in "$out"/fresh-ui/*.o; do objects+=("$object"); done'
new = '''  for object in "$out"/fresh-ui/*.o; do
    [[ ${object##*/} == ps5.o ]] || objects+=("$object")
  done
  objects+=("$root/build/steam-startup-diagnostic/ps5-after-root-test.o")'''
old_output = '-e _start -o "$out/experimental-pie.elf"'
new_output = '-e _start -o "$root/build/steam-startup-diagnostic/test-pie-after-root.elf"'
if original.count(old) != 1 or original.count(old_output) != 1:
    raise ValueError("Unexpected native link script")
link = original.replace(old, new).replace(old_output, new_output)
(root / "tools/probe-steam-startup-after-root.sh").write_text(link)
print("Prepared isolated startup diagnostic sources and link script.")
