#!/usr/bin/env bash
# Research link only. Uses previously compiled probe objects, does not create
# an app package, and must not be deployed or executed on the console.
set -euo pipefail
if [[ "${WINE11_EXPERIMENT_UNINSTALL:-0}" == 1 ]]; then
  WINE11_PAYLOAD_GAME_CLEANUP="${WINE11_PAYLOAD_GAME_CLEANUP:-1}"
  if [[ "$WINE11_PAYLOAD_GAME_CLEANUP" != 1 && "${WINE11_OWNER_INSPECT_ONLY:-0}" != 1 ]]; then
    echo 'Game removal requires the tested cleanup helper.' >&2; exit 1
  fi
fi
if [[ "${WINE11_OWNER_BLENDER_ONESHOT:-0}" == 1 && "${WINE11_EXPERIMENT_UNINSTALL:-0}" != 1 ]]; then
  echo 'Owner Blender request requires the uninstall experiment.' >&2; exit 1
fi
if [[ "${WINE11_OWNER_INSPECT_ONLY:-0}" == 1 && ( "${WINE11_EXPERIMENT_UNINSTALL:-0}" != 1 || "${WINE11_OWNER_BLENDER_ONESHOT:-0}" == 1 ) ]]; then
  echo 'Inspection requires uninstall and cannot run with owner removal.' >&2; exit 1
fi
if [[ "${WINE11_OWNER_RETIRE_RECOVERY:-0}" == 1 && ( "${WINE11_EXPERIMENT_UNINSTALL:-0}" != 1 || "${WINE11_OWNER_BLENDER_ONESHOT:-0}" == 1 || "${WINE11_OWNER_INSPECT_ONLY:-0}" == 1 ) ]]; then
  echo 'Owner retirement recovery requires uninstall and cannot run with other owner modes.' >&2; exit 1
fi
if [[ "${WINE11_OWNER_CLEANUP_PROBE:-0}" == 1 && "${WINE11_OWNER_INSPECT_ONLY:-0}" != 1 ]]; then
  echo 'Cleanup probe requires inspection-only mode.' >&2; exit 1
fi
if [[ "${WINE11_PAYLOAD_GAME_CLEANUP:-0}" == 1 && "${WINE11_EXPERIMENT_UNINSTALL:-0}" != 1 ]]; then
  echo 'Payload cleanup requires the uninstall experiment.' >&2; exit 1
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 && -f build/steam-snapshot-native-link-probe/payload-cleanup-mode ]]; then
  [[ $(cat build/steam-snapshot-native-link-probe/payload-cleanup-mode) == "${WINE11_PAYLOAD_GAME_CLEANUP:-0}" ]] || {
    echo 'Backend and PIE payload cleanup modes differ; rebuild the backend.' >&2; exit 1;
  }
elif [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 && "${WINE11_PAYLOAD_GAME_CLEANUP:-0}" == 1 ]]; then
  echo 'Backend payload cleanup mode is missing; rebuild the backend.' >&2; exit 1
fi
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
python3 tools/verify-steam-tested-inputs.py
python3 tools/fetch-steam-compiler-rt.py
out="$root/build/steam-native-experiment"
mkdir -p "$out"
host_source="$root/build/steam-host-source"
host_build="$root/build/steam-host-cmake"
if [[ "${WINE11_PROBE_DEDICATED_HOST:-0}" == 1 ]]; then
  [[ "${WINE11_PROBE_GAME_HOST:-0}" == 1 ]] || { echo 'Dedicated host requires the game host probe.' >&2; exit 1; }
  host_source="$root/build/steam-host-dedicated-source"
  host_build="$root/build/steam-host-dedicated-cmake"
  grep -Fq '/data/wine11/kits/playden-kit' "$host_source/src/runtime/kit_trust.h" || { echo 'Dedicated kit path is missing.' >&2; exit 1; }
  grep -Fq '/data/wine11/title-rule/allow-list' "$host_source/src/runtime/package_installer.h" || { echo 'Dedicated title-list path is missing.' >&2; exit 1; }
fi
native="$root/vendor/ps5-native-app-boilerplate/tooling/native"
gl="$root/vendor/playden-source/third_party/ps5-opengl/native-app"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -c "$native/app_crt.cpp" -o "$out/app_crt.o"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -c tools/diagnostics/ps5_allocation_probe.cpp -o "$out/allocation_probe.o"
for source in runtime_shims app_heap; do
  "$sdk/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections -c "$gl/$source.c" -o "$out/$source.o"
done
objects=("$out/app_crt.o" "$out/runtime_shims.o" "$out/app_heap.o" "$out/allocation_probe.o")
if [[ "${WINE11_FRESH_UI:-0}" == 1 ]]; then
  [[ -f "$out/ui-ready" && -f "$out/fresh-ui/ps5.o" && -f "$out/fresh-owner/admission_observer.c.o" ]] || { echo 'Fresh UI snapshot is incomplete.' >&2; exit 1; }
  python3 tools/hash-steam-native-snapshot.py verify
  for object in "$out"/fresh-ui/*.o; do objects+=("$object"); done
else
  objects+=("$out/ps5.o")
  for object in "$root"/build/steam-probe/ui/*.o; do
    [[ ${object##*/} == ps5.o ]] || objects+=("$object")
  done
fi
steam_objects="$root/build/steam-tested-native-link-probe"
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  [[ "${WINE11_FRESH_UI:-0}" == 1 ]] || { echo 'Snapshot backend requires the fresh UI snapshot.' >&2; exit 1; }
  steam_objects="$root/build/steam-snapshot-native-link-probe"
fi
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  [[ -f "$steam_objects/objects.list" ]] || { echo 'Current backend object list is missing.' >&2; exit 1; }
  guarded_uid_seen=0
  while IFS= read -r -d '' object; do
    [[ -f "$object" && "$object" == "$steam_objects/"* ]] || { echo 'Backend object list is invalid.' >&2; exit 1; }
    [[ ${object##*/} == guarded_effective_uid-* ]] && guarded_uid_seen=1
    [[ ${object##*/} == effective_uid-* ]] && { echo 'Unpatched UID guard found in backend objects.' >&2; exit 1; }
    [[ ${object##*/} == steam_runtime_link_probe-* ]] || objects+=("$object")
  done < "$steam_objects/objects.list"
  [[ "$guarded_uid_seen" == 1 ]] || { echo 'Rebuild the backend with the guarded UID source.' >&2; exit 1; }
else
  guarded_uid_seen=0
  for object in "$steam_objects"/*.o; do
    [[ ${object##*/} == guarded_effective_uid-* ]] && guarded_uid_seen=1
    [[ ${object##*/} == effective_uid-* ]] && { echo 'Unpatched UID guard found in backend objects.' >&2; exit 1; }
    [[ ${object##*/} == steam_runtime_link_probe-* ]] || objects+=("$object")
  done
  [[ "$guarded_uid_seen" == 1 ]] || { echo 'Rebuild the backend with the guarded UID source.' >&2; exit 1; }
fi
# Verify both the compiled backend guard and its generated source before linking.
[[ -f "$steam_objects/guarded-effective-uid.sha256" ]] || { echo 'Rebuild the backend UID guard.' >&2; exit 1; }
sha256sum -c "$steam_objects/guarded-effective-uid.sha256" >/dev/null
python3 - "$steam_objects/guarded_effective_uid.cpp" <<'PYCODE'
import sys
from pathlib import Path
sys.path.insert(0, 'tools')
from prepare_steam_uid_guard import transform
if Path(sys.argv[1]).read_text() != transform(Path('vendor/playden-source/src/platform/effective_uid.cpp').read_text()):
    raise SystemExit('Backend UID guard source is stale.')
PYCODE
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  src="$out/src"; owner="$src/owner_admission/third_party/mbedtls-publication"
  startup_flags=()
  if [[ "${WINE11_EXPERIMENT_UNINSTALL:-0}" == 1 ]]; then startup_flags+=(-DWINE11_EXPERIMENT_UNINSTALL=1); fi
  if [[ "${WINE11_OWNER_BLENDER_ONESHOT:-0}" == 1 ]]; then startup_flags+=(-DWINE11_OWNER_BLENDER_ONESHOT=1); fi
  if [[ "${WINE11_OWNER_INSPECT_ONLY:-0}" == 1 ]]; then startup_flags+=(-DWINE11_OWNER_INSPECT_ONLY=1); fi
  if [[ "${WINE11_OWNER_CLEANUP_PROBE:-0}" == 1 ]]; then startup_flags+=(-DWINE11_OWNER_CLEANUP_PROBE=1); fi
  if [[ "${WINE11_OWNER_RETIRE_RECOVERY:-0}" == 1 ]]; then startup_flags+=(-DWINE11_OWNER_RETIRE_RECOVERY=1); fi
  mkdir -p "$out/fresh-startup"
  for name in native_trust native_startup native_bridge native_app_mount; do
    "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 "${startup_flags[@]}" \
      -I"$src" -Ivendor/playden-source/src -I"$owner/include" -I"$owner" \
      -c "$src/steam_integration/$name.cpp" -o "$out/fresh-startup/$name.o"
    objects+=("$out/fresh-startup/$name.o")
  done
  if [[ "${WINE11_PROBE_GAME_HOST:-0}" == 1 ]]; then
    [[ -f "$host_build/libplayden_homebrew_runtime_host.a" ]] || { echo 'Game host libraries are missing.' >&2; exit 1; }
    cmp -s "$src/steam_integration/runtime_install_id.cpp" "$host_source/src/runtime_install_id.cpp" || { echo 'Game identity source differs from the host build.' >&2; exit 1; }
    cmp -s "$src/steam_integration/runtime_install_id.hpp" "$host_source/src/runtime_install_id.hpp" || { echo 'Game identity header differs from the host build.' >&2; exit 1; }
    if [[ "${WINE11_PROBE_DEDICATED_HOST:-0}" == 1 ]]; then
      python3 - "$root/build/steam-native-experiment/runtime-host/runtime_host.cpp" "$host_source/src/homebrew/runtime_host.cpp" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'tools')
from prepare_vapor_title_trash import transform as title_trash
from prepare_cs2_runtime_installer import runtime_host
def transform(source): return runtime_host(title_trash(source))
source = Path(sys.argv[1]).read_text()
old = 'const auto cache = HostPath(config_.package_paths.console_data_root + "/playden/redist").string();'
new = 'const auto cache = (config_.state_root / "redist").string();'
if source.count(old) != 1 or transform(source.replace(old, new)) != Path(sys.argv[2]).read_text():
    raise SystemExit('Dedicated game host differs from the reviewed cache and title-retirement overlays.')
from prepare_vapor_publication_permissions import transform as publication_permissions, transform_coordinator
upstream = Path('vendor/playden-source/src/runtime/settings_publication.cpp').read_text()
dedicated = Path('build/steam-host-dedicated-source/src/runtime/settings_publication.cpp').read_text()
if publication_permissions(upstream) != dedicated:
    raise SystemExit('Dedicated settings publication differs from the reviewed permission overlay.')
upstream = Path('vendor/playden-source/src/platform/ps5/native_coordinator.cpp').read_text()
dedicated = Path('build/steam-host-dedicated-source/src/platform/ps5/native_coordinator.cpp').read_text()
if transform_coordinator(upstream) != dedicated:
    raise SystemExit('Dedicated title ownership differs from the reviewed permission overlay.')
PY
    else
      cmp -s "$root/build/steam-native-experiment/runtime-host/runtime_host.cpp" "$host_source/src/homebrew/runtime_host.cpp" || { echo 'Game host source differs from the host build.' >&2; exit 1; }
    fi
    for name in native_game_runtime playden_launch_host playden_preparation_host cloud_launch_gate native_host_config native_kit_admission title_list_writer title_list_merge; do
      "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 "${startup_flags[@]}" \
        -I"$src" -I"$host_source/src" -I"$owner/include" -I"$owner" \
        -c "$src/steam_integration/$name.cpp" -o "$out/fresh-startup/$name.o"
      objects+=("$out/fresh-startup/$name.o")
    done
    if [[ "${WINE11_PROBE_DEDICATED_HOST:-0}" == 1 ]]; then
      "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 \
        -I"$src" -I"$host_source/src" -c "$src/steam_integration/dedicated_title_list.cpp" -o "$out/fresh-startup/dedicated_title_list.o"
      objects+=("$out/fresh-startup/dedicated_title_list.o")
    fi
    if [[ "${WINE11_EXPERIMENT_GAME_HOST:-0}" == 1 ]]; then
      [[ "${WINE11_PROBE_DEDICATED_HOST:-0}" == 1 ]] || { echo 'Native game startup requires the dedicated host.' >&2; exit 1; }
      for name in native_game_startup native_title_provision app_user_identity vapor_privilege; do
        "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 \
          -I"$src" -I"$host_source/src" -c "$src/steam_integration/$name.cpp" -o "$out/fresh-startup/$name.o"
        objects+=("$out/fresh-startup/$name.o")
      done
      # ownership_adopt.cpp (used by app_user_identity) is compiled with the fresh UI snapshot.
      "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 \
        -I"$host_source/src" -I"$root/build/steam-network-tested-snapshot/include" \
        -c "$host_source/src/platform/ps5/console_services.cpp" -o "$out/fresh-startup/console_services.o"
      objects+=("$out/fresh-startup/console_services.o")
    fi
    "$sdk/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 \
      -I"$src" -I"$src/steam_integration" -Ivendor/playden-source/src \
      -c tools/diagnostics/ps5_game_host_link_probe.cpp -o "$out/fresh-startup/host-anchor.o"
    objects+=("$out/fresh-startup/host-anchor.o")
  fi
else
  for object in "$root"/build/steam-native-startup-probe/native_{trust,startup,bridge,app_mount}.o; do objects+=("$object"); done
fi
if [[ "${WINE11_FRESH_UI:-0}" == 1 ]]; then
  for object in "$out"/fresh-owner/*.o; do objects+=("$object"); done
else
  while IFS= read -r -d '' object; do objects+=("$object"); done < <(find "$root/vendor/ps5-native-app-boilerplate/build/obj/wine11/src/owner_admission" -name '*.o' -print0 | sort -z)
fi
network="$root/build/steam-network-tested-snapshot/lib"
host_archives=()
host_anchor=()
if [[ "${WINE11_PROBE_GAME_HOST:-0}" == 1 ]]; then
  [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]] || { echo 'Game host probe requires a snapshot.' >&2; exit 1; }
  for archive in "$host_build"/lib*.a; do host_archives+=("$archive"); done
  host_anchor=(-u wine11_game_host_link_probe)
fi
cp "$native/ps5-pie.ld" "$out/ps5-pie.ld"
cat >> "$out/ps5-pie.ld" <<'EOF'
PROVIDE(__eh_frame_start = ADDR(.eh_frame));
PROVIDE(__eh_frame_end = ADDR(.eh_frame) + SIZEOF(.eh_frame));
PROVIDE(__eh_frame_hdr_start = ADDR(.eh_frame_hdr));
PROVIDE(__eh_frame_hdr_end = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr));
EOF
uninstall_wraps=()
if [[ "${WINE11_EXPERIMENT_UNINSTALL:-0}" == 1 ]]; then uninstall_wraps=(--wrap=fstatat --wrap=unlinkat); fi
"$sdk/bin/prospero-lld" -T "$out/ps5-pie.ld" --eh-frame-hdr --gc-sections -Map="$out/experimental-pie.map" "${uninstall_wraps[@]}" \
  --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free --wrap=posix_memalign --wrap=malloc_usable_size \
  --wrap=openat --wrap=mkdirat -L "$sdk/target/lib" "${host_anchor[@]}" --version-script "$native/app-symbols.map" -e _start -o "$out/experimental-pie.elf" \
  "${objects[@]}" --start-group "${host_archives[@]}" "$sdk/target/lib/libunwind.a" "$sdk/target/lib/libc++abi.a" "$sdk/target/lib/libc++.a" \
  "$network/libcurl.a" "$network/libmbedtls.a" "$network/libmbedx509.a" "$network/libmbedcrypto.a" \
  "$network/libeverest.a" "$network/libp256m.a" "$out/compiler-rt/libclang_rt.builtins-x86_64.a" --end-group --as-needed "$root"/build/native-imports/*.so "$sdk"/target/lib/*.so
echo 'PASS: research PIE link only; runtime host and console behavior remain unverified. Do not deploy.'
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then python3 tools/hash-steam-native-snapshot.py verify; fi
