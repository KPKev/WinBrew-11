#!/usr/bin/env bash
# Link-only test of Playden's real clients and workers. Never executed on the PS5.
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
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 && "${WINE11_TESTED_LINK_PROBE:-0}" != 1 ]]; then
  echo 'A snapshot link requires the separately verified tested inputs.' >&2; exit 1
fi
if [[ "${WINE11_TESTED_LINK_PROBE:-0}" == 1 ]]; then
  echo 'DIAGNOSTIC ONLY: using the preserved console-probed archives; output is not a native app or release.' >&2
  network="$root/build/steam-network-tested-snapshot"
  python3 tools/verify-steam-tested-inputs.py --prefix "$network"
elif [[ "${WINE11_UNVERIFIED_LINK_PROBE:-0}" == 1 ]]; then
  echo 'DIAGNOSTIC ONLY: binary archives do not match Playden lock; output must not be run or deployed.' >&2
  python3 tools/verify-steam-network.py --headers-only
  network="$root/vendor/playden-source/scripts/network-deps/.deps/ps5"
else
  python3 tools/verify-steam-network.py
  network="$root/vendor/playden-source/scripts/network-deps/.deps/ps5"
fi
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$root/vendor/playden-source/deps/ps5-payload-sdk}"
llvm_root="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LD_LIBRARY_PATH="$llvm_root/lib/x86_64-linux-gnu:$llvm_root/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LLVM_CONFIG="${LLVM_CONFIG:-$llvm_root/lib/llvm-18/bin/llvm-config}"
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  build="$root/build/steam-snapshot-native-link-probe"
elif [[ "${WINE11_TESTED_LINK_PROBE:-0}" == 1 ]]; then
  build="$root/build/steam-tested-native-link-probe"
else
  build="$root/build/steam-native-link-probe"
fi
mkdir -p "$build"
# This explicit backend object is linked ahead of host archives; rebuilding it
# prevents a stale vendor UID guard from silently winning symbol resolution.
python3 tools/prepare_steam_uid_guard.py vendor/playden-source/src/platform/effective_uid.cpp "$build/guarded_effective_uid.cpp"
python3 tools/prepare_steam_depot_estimates.py vendor/playden-source/src/steam/backend/cm_app_details.cpp "$build/guarded_cm_app_details.cpp"
sources=(tests/steam_runtime_link_probe.cpp src/steam_integration/runtime.cpp src/steam_integration/artwork.cpp src/steam_integration/controller.cpp src/steam_integration/downloads.cpp src/steam_integration/ready_launch.cpp src/steam_integration/preparation.cpp src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp vendor/playden-source/src/steam/store_details_service.cpp "$build/guarded_cm_app_details.cpp" src/steam_integration/ui_adapter.cpp src/steam_integration/remember_setting.cpp src/steam_integration/controller_layouts.cpp
 vendor/playden-source/src/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp
 vendor/playden-source/src/steam/local_session_repository.cpp vendor/playden-source/src/steam/session_store.cpp
 vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp
 vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp
 "$build/guarded_effective_uid.cpp" vendor/playden-source/src/platform/random_id.cpp
 vendor/playden-source/src/library/cache.cpp
 vendor/playden-source/src/steam/backend/cm_client.cpp vendor/playden-source/src/steam/backend/entitled_library.cpp
 vendor/playden-source/src/steam/backend/family_groups.cpp vendor/playden-source/src/steam/backend/ws_client.cpp
 vendor/playden-source/src/steam/backend/binary_kv.cpp vendor/playden-source/src/steam/backend/gzip.cpp
 vendor/playden-source/src/steam/backend/app_info.cpp vendor/playden-source/src/steam/backend/vdf.cpp
 vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp
 vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/backend/auth_client.cpp
 vendor/playden-source/src/steam/backend/auth_messages.cpp vendor/playden-source/src/steam/backend/cm_messages.cpp
 vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/steam/backend/token_claims.cpp
 vendor/playden-source/src/steam/backend/proto_wire.cpp vendor/playden-source/src/steam/backend/password_crypto.cpp
 vendor/playden-source/src/steam/backend/library_client.cpp vendor/playden-source/src/steam/backend/http_client.cpp
 vendor/playden-source/src/steam/backend/native_network.cpp)
sources+=(src/steam_integration/cloud_runtime.cpp src/steam_integration/cloud_preflight.cpp
  vendor/playden-source/src/steam/cloud_service.cpp vendor/playden-source/src/steam/backend/cloud_messages.cpp
  vendor/playden-source/src/steam/backend/cloud_mapping.cpp vendor/playden-source/src/steam/backend/cloud_sync.cpp
  vendor/playden-source/src/steam/backend/steam_cloud_runner.cpp)
flags=()
linkflags=()
if [[ "${WINE11_EXPERIMENT_GAME_HOST:-0}" == 1 ]]; then
  [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 && "${WINE11_PROBE_DOWNLOADS:-0}" == 1 ]] || { echo 'Dedicated game host requires the complete snapshot backend.' >&2; exit 1; }
  grep -Fq 'const auto cache = (config_.state_root / "redist").string();' build/steam-host-dedicated-source/src/homebrew/runtime_host.cpp || { echo 'Dedicated dependency cache is missing.' >&2; exit 1; }
  flags+=(-DWINE11_DEDICATED_HOST=1)
  sources+=(src/steam_integration/redist_fetch.cpp vendor/playden-source/src/homebrew/artwork_backend.cpp vendor/playden-source/src/homebrew/title_icon.cpp)
  if [[ "${WINE11_EXPERIMENT_UNINSTALL:-0}" == 1 ]]; then
    flags+=(-DWINE11_EXPERIMENT_UNINSTALL=1)
    sources+=(src/steam_integration/vapor_title_remover.cpp src/steam_integration/uninstall_journal.cpp
      src/steam_integration/uninstall_registry.cpp src/steam_integration/uninstall_quarantine.cpp
      src/steam_integration/protected_uninstall.cpp src/steam_integration/title_removal.cpp
      src/steam_integration/playden_title_removal_host.cpp src/steam_integration/uninstall_flow.cpp
      src/steam_integration/title_absence.cpp src/steam_integration/native_title_absence.cpp vendor/playden-source/src/platform/ps5/title_remover.cpp)
    if [[ "${WINE11_PAYLOAD_GAME_CLEANUP:-0}" == 1 ]]; then
      flags+=(-DWINE11_PAYLOAD_GAME_CLEANUP=1)
      python3 tools/build-vapor-game-cleaner.py "$build/vapor-embedded-cleaner.cpp"
      sources+=(src/steam_integration/vapor_game_cleanup.cpp "$build/vapor-embedded-cleaner.cpp")
    fi
    linkflags+=(-Wl,--wrap=fstatat -Wl,--wrap=unlinkat)
    if [[ "${WINE11_OWNER_BLENDER_ONESHOT:-0}" == 1 ]]; then
      flags+=(-DWINE11_OWNER_BLENDER_ONESHOT=1)
      sources+=(src/steam_integration/owner_blender_request.cpp)
    fi
    if [[ "${WINE11_OWNER_INSPECT_ONLY:-0}" == 1 ]]; then
      flags+=(-DWINE11_OWNER_INSPECT_ONLY=1)
      sources+=(src/steam_integration/owner_blender_inspection.cpp)
      if [[ "${WINE11_OWNER_CLEANUP_PROBE:-0}" == 1 ]]; then
        flags+=(-DWINE11_OWNER_CLEANUP_PROBE=1)
        sources+=(src/steam_integration/owner_cleanup_probe.cpp)
      fi
    fi
    if [[ "${WINE11_OWNER_RETIRE_RECOVERY:-0}" == 1 ]]; then
      flags+=(-DWINE11_OWNER_RETIRE_RECOVERY=1)
      sources+=(src/steam_integration/owner_title_recovery.cpp)
    fi
  fi
fi
if [[ "${WINE11_PROBE_CLOUD:-0}" == 1 ]]; then flags+=(-DWINE11_PROBE_CLOUD=1); fi
if [[ "${WINE11_PROBE_DOWNLOADS:-0}" == 1 ]]; then
  flags+=(-DWINE11_PROBE_DOWNLOADS=1 -DWINE11_PROBE_NATIVE_STARTUP=1)
  sources+=(src/steam_integration/native_paths.cpp src/steam_integration/native_session.cpp)
  sources+=(vendor/playden-source/src/steam/backend/steam_install_runner.cpp
            vendor/playden-source/src/steam/backend/depot_installer.cpp
            vendor/playden-source/src/steam/backend/cdn_client.cpp
            vendor/playden-source/src/steam/backend/download_server_source.cpp
            vendor/playden-source/src/steam/backend/game_path.cpp
            vendor/playden-source/src/steam/backend/depot_chunk.cpp
            vendor/playden-source/src/steam/backend/depot_manifest.cpp
            vendor/playden-source/src/runtime/steamworks.cpp)
fi
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  snapshot="build/steam-native-experiment/src/steam_integration"
  if [[ "${WINE11_EXPERIMENT_CANCEL_CLEANUP:-0}" == 1 ]]; then
    [[ "${WINE11_PROBE_DOWNLOADS:-0}" == 1 ]] || { echo 'Experimental cancellation requires the guarded download backend.' >&2; exit 1; }
    [[ -f build/steam-native-experiment/src/installs/install_service.h && -f build/steam-native-experiment/src/installs/install_service.cpp ]] || { echo 'Experimental cancellation source snapshot is missing.' >&2; exit 1; }
    flags+=(-DWINE11_CANCEL_CLEANUP_OVERLAY=1 -DWINE11_DURABLE_CANCEL_QUIESCE=1 -DWINE11_INSTALL_GENERATION_EXPERIMENT=1)
    linkflags+=(-Wl,--wrap=openat -Wl,--wrap=mkdirat)
    sources+=(src/steam_integration/cancel_cleanup_journal.cpp src/steam_integration/cancel_winner_evidence.cpp src/steam_integration/cancel_target_evidence.cpp
              src/steam_integration/cancel_registry_plan.cpp src/steam_integration/cancel_registry_reconciliation.cpp)
  else
    [[ -f build/steam-native-experiment/src/installs/install_service.h && -f build/steam-native-experiment/src/installs/install_service.cpp ]] || { echo 'Quiescence source snapshot is missing.' >&2; exit 1; }
    flags+=(-DWINE11_SERVICE_QUIESCENCE=1 -DWINE11_DISABLE_UNSAFE_CANCEL=1)
  fi
  [[ -f build/steam-native-experiment/ui-ready && -f "$snapshot/runtime.cpp" && -f "$snapshot/ui_adapter.cpp" ]] || { echo 'Steam source snapshot is missing or incomplete.' >&2; exit 1; }
  python3 tools/hash-steam-native-snapshot.py verify
  for i in "${!sources[@]}"; do
    if [[ ${sources[$i]} == src/steam_integration/* ]]; then
      sources[$i]="$snapshot/${sources[$i]#src/steam_integration/}"
    elif [[ ${sources[$i]} == "$build/guarded_cm_app_details.cpp" ]]; then
      sources[$i]="$snapshot/guarded_cm_app_details.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/backend/app_info.cpp ]]; then
      sources[$i]="$snapshot/blender_depot_app_info.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/cloud_service.cpp ]]; then
      sources[$i]="$snapshot/guarded_cloud_service.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/installs/install_service.cpp ]]; then
      sources[$i]="build/steam-native-experiment/src/installs/install_service.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/local_session_repository.cpp ]]; then
      sources[$i]="$snapshot/guarded_local_session_repository.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/backend/http_client.cpp ]]; then
      sources[$i]="$snapshot/observed_http_client.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/backend/steam_install_runner.cpp ]]; then
      sources[$i]="$snapshot/space_aware_install_runner.cpp"
    elif [[ ${sources[$i]} == vendor/playden-source/src/steam/backend/depot_installer.cpp ]]; then
      sources[$i]="$snapshot/tuned_depot_installer.cpp"
    fi
  done
fi
"$PS5_PAYLOAD_SDK/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections \
  -c vendor/playden-source/third_party/qrcodegen/qrcodegen.c -o "$build/qrcodegen.o"
objects=("$build/qrcodegen.o")
if [[ "${WINE11_PROBE_DOWNLOADS:-0}" == 1 ]]; then
  c_sources=(vendor/playden-source/src/platform/ps5/file_shims.c
    vendor/playden-source/third_party/lzma-sdk/LzmaDec.c
    vendor/playden-source/third_party/zstd/common/debug.c
    vendor/playden-source/third_party/zstd/common/entropy_common.c
    vendor/playden-source/third_party/zstd/common/error_private.c
    vendor/playden-source/third_party/zstd/common/fse_decompress.c
    vendor/playden-source/third_party/zstd/common/xxhash.c
    vendor/playden-source/third_party/zstd/common/zstd_common.c
    vendor/playden-source/third_party/zstd/decompress/huf_decompress.c
    vendor/playden-source/third_party/zstd/decompress/zstd_ddict.c
    vendor/playden-source/third_party/zstd/decompress/zstd_decompress.c
    vendor/playden-source/third_party/zstd/decompress/zstd_decompress_block.c)
  if [[ "${WINE11_EXPERIMENT_UNINSTALL:-0}" == 1 ]]; then
    python3 tools/prepare_vapor_file_shims.py "$build/vapor-file-shims.c"
    c_sources[0]="$build/vapor-file-shims.c"
  fi
  for source in "${c_sources[@]}"; do
    object="$build/$(basename "${source%.c}")-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
    "$PS5_PAYLOAD_SDK/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections \
      -DZSTD_DISABLE_ASM=1 -DZSTD_LEGACY_SUPPORT=0 -DZSTD_TRACE=0 \
      -Ivendor/playden-source/third_party/lzma-sdk -Ivendor/playden-source/third_party/zstd \
      -Ivendor/playden-source/third_party/zstd/common -c "$source" -o "$object"
    objects+=("$object")
  done
fi
compile_pids=()
wait_compiles() {
  local status=0 pid
  for pid in "${compile_pids[@]}"; do wait "$pid" || status=1; done
  compile_pids=()
  return "$status"
}
for source in "${sources[@]}"; do
  object="$build/$(basename "${source%.cpp}")-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
  "$PS5_PAYLOAD_SDK/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -DPLAYDEN_PLATFORM_PS5=1 "${flags[@]}" \
    -Ibuild/steam-native-experiment/src -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen -Ivendor/playden-source/third_party/stb -Ivendor/playden-source/third_party/lzma-sdk -Ivendor/playden-source/third_party/zstd -Ivendor/playden-source/third_party/zstd/common -I"$network/include" -c "$source" -o "$object" &
  compile_pids+=("$!")
  objects+=("$object")
  if (( ${#compile_pids[@]} >= 4 )); then wait_compiles; fi
done
wait_compiles
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  source="$snapshot/guarded_cm_app_details.cpp"
else
  source="$build/guarded_cm_app_details.cpp"
fi
object="$build/guarded_cm_app_details-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
"$llvm_root/lib/llvm-18/bin/llvm-nm" "$object" | grep ' wine11_verified_depot_estimates$' >/dev/null || {
  echo 'Guarded depot estimate object is missing.' >&2; exit 1;
}
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 && "${WINE11_EXPERIMENT_CANCEL_CLEANUP:-0}" == 1 ]]; then
  for entry in "downloads.cpp:wine11_cancel_cleanup_coordinator_enabled" "ui_adapter.cpp:wine11_cancel_cleanup_adapter_enabled"; do
    name=${entry%%:*}
    symbol=${entry#*:}
    source="$snapshot/$name"
    object="$build/${name%.cpp}-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
    "$llvm_root/lib/llvm-18/bin/llvm-nm" "$object" | grep " $symbol$" >/dev/null || {
      echo "Cancellation cleanup overlay is missing from $name" >&2; exit 1;
    }
  done
elif [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then
  for entry in "downloads.cpp:wine11_cancel_guard_coordinator_enabled" "ui_adapter.cpp:wine11_cancel_guard_adapter_enabled" "runtime.cpp:wine11_cancel_guard_runtime_enabled"; do
    name=${entry%%:*}; symbol=${entry#*:}; source="$snapshot/$name"
    object="$build/${name%.cpp}-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
    "$llvm_root/lib/llvm-18/bin/llvm-nm" "$object" | grep " $symbol$" >/dev/null || { echo "Cancellation guard missing from $name" >&2; exit 1; }
  done
  source="build/steam-native-experiment/src/installs/install_service.cpp"
  object="$build/install_service-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
  "$llvm_root/lib/llvm-18/bin/llvm-nm" "$object" | grep ' wine11_cancel_guard_service_enabled$' >/dev/null || { echo 'Cancellation guard missing from install service' >&2; exit 1; }
fi
uid_object="$build/guarded_effective_uid-$(printf '%s' "$build/guarded_effective_uid.cpp" | sha256sum | cut -c1-8).o"
sha256sum "$build/guarded_effective_uid.cpp" "$uid_object" > "$build/guarded-effective-uid.sha256"
"$PS5_PAYLOAD_SDK/bin/prospero-clang++" "${objects[@]}" "${linkflags[@]}" -o "$build/steam-client-probe.elf" \
  -Wl,--gc-sections -Wl,--start-group "$network/lib/libcurl.a" "$network/lib/libmbedtls.a" "$network/lib/libmbedx509.a" \
  "$network/lib/libmbedcrypto.a" "$network/lib/libeverest.a" "$network/lib/libp256m.a" -Wl,--end-group
printf '%s\n' 'PASS: linked Playden real Steam runtime, clients, worker services and Wine 11 coordinator for PS5 (link diagnostic only; not executed)'
if [[ "${WINE11_USE_SNAPSHOT:-0}" == 1 ]]; then python3 tools/hash-steam-native-snapshot.py verify; fi
# Record exactly the objects just linked. A previous diagnostic can leave
# differently named objects in this directory; a later PIE must not glob them.
printf '%s\0' "${objects[@]}" > "$build/objects.list.tmp"
mv "$build/objects.list.tmp" "$build/objects.list"
printf '%s\n' "${WINE11_PAYLOAD_GAME_CLEANUP:-0}" > "$build/payload-cleanup-mode"
