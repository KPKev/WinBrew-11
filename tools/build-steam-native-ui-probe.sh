#!/usr/bin/env bash
# Compile a fresh, isolated snapshot of Wine 11's native UI and Steam bridge.
# No title package is produced or deployed.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
WINE11_EXPERIMENT_GAME_HOST=1 python3 tools/prepare-steam-native-experiment.py
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
out="$root/build/steam-native-experiment"
src="$out/src"
mkdir -p "$src" "$out/fresh-ui" "$out/fresh-owner"
rm -f "$out/ui-ready"
find "$out/fresh-ui" "$out/fresh-owner" -maxdepth 1 -type f -name '*.o' -delete
python3 - "$root" "$src" <<'PY'
import ast, pathlib, shutil, sys
root, dest = map(pathlib.Path, sys.argv[1:])
tree = ast.parse((root/'tools/stage_build.py').read_text())
lists = [node.iter for node in ast.walk(tree) if isinstance(node, ast.For)
         and isinstance(node.target, ast.Name) and node.target.id == 'name'
         and isinstance(node.iter, ast.List)]
if len(lists) != 1:
    raise SystemExit('Could not identify the native source staging list')
for name in ast.literal_eval(lists[0]):
    if name != 'ps5.cpp': shutil.copyfile(root/'src'/name, dest/name)
shutil.copyfile(root/'build/steam-native-experiment/ps5.cpp', dest/'ps5.cpp')
for subtree in ('owner_admission', 'steam_integration'):
    source_root = root/'src'/subtree
    target_root = dest/subtree
    # A previous owner-only diagnostic may have been removed from the source.
    # Trim isolated staging before copying so it cannot survive into a new app.
    target_root.resolve().relative_to((root/'build/steam-native-experiment').resolve())
    authoritative = {p.relative_to(source_root) for p in source_root.rglob('*') if p.is_file()}
    if target_root.exists():
        for stale in target_root.rglob('*'):
            if stale.is_file() and stale.relative_to(target_root) not in authoritative:
                if stale.is_symlink(): raise SystemExit('Unexpected staged source symlink')
                stale.unlink()
    for source in source_root.rglob('*'):
        if source.is_file():
            target = dest/subtree/source.relative_to(source_root)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
v = root/'vendor/ps5-native-app-boilerplate'
h = (v/'src/demo_renderer.hpp').read_text().replace('    void clear(Color color) noexcept;', '    void blit(const std::uint32_t *linear) noexcept;\n    void clear(Color color) noexcept;')
c = (v/'src/demo_renderer.cpp').read_text().replace('"demo_renderer.hpp"', '"ps5_video.hpp"')
c = c.replace('void Canvas::clear(Color color) noexcept', 'void Canvas::blit(const std::uint32_t *linear) noexcept\n{\n    for (unsigned y=0;y<1080;y++)\n        for (unsigned x=0;x<1920;x++)\n            put_pixel_unchecked(pixels_,x,y,static_cast<Color>(linear[y*1920+x]));\n}\n\nvoid Canvas::clear(Color color) noexcept').replace('Hello World:', 'WinBrew 11:')
(dest/'ps5_video.hpp').write_text(h); (dest/'ps5_video.cpp').write_text(c)
e = v/'examples/sandbox-elevation'
for name in ('elevation.hpp','protocol.hpp'): shutil.copyfile(e/name,dest/name)
(dest/'elevation.cpp').write_text((e/'src/elevation.cpp').read_text().replace('"../elevation.hpp"','"elevation.hpp"').replace('"../protocol.hpp"','"protocol.hpp"'))
PY
python3 tools/prepare_steam_depot_estimates.py vendor/playden-source/src/steam/backend/cm_app_details.cpp "$src/steam_integration/guarded_cm_app_details.cpp"
python3 tools/prepare_blender_depot_selection.py "$src/steam_integration/blender_depot_app_info.cpp"
python3 tools/prepare-steam-cloud-experiment.py "$src/steam_integration/guarded_cloud_service.cpp"
python3 tools/prepare-steam-session-experiment.py "$src/steam_integration/guarded_local_session_repository.cpp"
python3 tools/prepare-steam-depot-tuning.py "$src/steam_integration/tuned_depot_installer.cpp"
python3 tools/prepare_steam_space_runner.py "$src/steam_integration/space_aware_install_runner.cpp"
python3 tools/prepare_steam_http_observer.py "$src/steam_integration/observed_http_client.cpp"
if [[ "${WINE11_EXPERIMENT_CANCEL_CLEANUP:-0}" == 1 ]]; then
  python3 tools/prepare_steam_cancel_cleanup.py "$src"
  python3 tools/prepare_steam_cleanup_remover.py "$src/steam_integration/tuned_depot_installer.cpp" "$src/steam_integration/tuned_depot_installer.cpp"
else
  python3 tools/prepare_steam_quiescence.py "$src"
fi
python3 tools/hash-steam-native-snapshot.py record
for source in "$src"/*.cpp; do
  "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
    -DPLAYDEN_PLATFORM_PS5=1 -I"$src" -Ivendor/playden-source/src -c "$source" -o "$out/fresh-ui/$(basename "${source%.cpp}").o"
done
owner="$src/owner_admission"
while IFS= read -r -d '' source; do
  relative=${source#"$owner/"}
  object="$out/fresh-owner/${relative//\//_}.o"
  "$sdk/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -I"$owner" -I"$owner/native" -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" \
    -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h" -c "$source" -o "$object"
done < <(find "$owner" -name '*.c' ! -path '*/tests/*' -print0 | sort -z)
python3 tools/hash-steam-native-snapshot.py verify
touch "$out/ui-ready"
echo 'PASS: isolated Wine 11 UI and owner integration compiled from the current workspace snapshot.'
