#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
python3 tests/test_vapor_native_module_modes.py
python3 tests/test_prepare_steam_registration_reconcile.py
python3 tests/test_prepare_vapor_title_remover.py
python3 tests/test_prepare_vapor_file_shims.py
python3 tests/test_prepare_vapor_title_trash.py
python3 tests/test_prepare_vapor_publication_permissions.py
bash tools/test-steam-depot-estimates.sh
python3 tests/test_prepare_steam_quiescence.py
python3 tools/prepare_steam_quiescence.py build/quiescence-overlay
g++ -std=c++20 -O2 -pthread -DWINE11_SERVICE_QUIESCENCE=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src src/steam_integration/downloads.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_worker_quiescence_test.cpp -o build/steam-worker-quiescence-test
./build/steam-worker-quiescence-test
g++ -std=c++20 -O2 -pthread -DWINE11_SERVICE_QUIESCENCE=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src src/steam_integration/downloads.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_download_maintenance_test.cpp -o build/steam-download-maintenance-test
./build/steam-download-maintenance-test
g++ -std=c++20 -O2 -pthread -DWINE11_SERVICE_QUIESCENCE=1 -DWINE11_DISABLE_UNSAFE_CANCEL=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src src/steam_integration/downloads.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_cancel_guard_test.cpp -o build/steam-cancel-guard-test
./build/steam-cancel-guard-test
python3 tools/prepare_steam_cancel_failure_experiment.py build/cancel-failure-experiment
g++ -std=c++20 -O2 -pthread -Ibuild/cancel-failure-experiment -Ivendor/playden-source/src build/cancel-failure-experiment/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_cancel_failure_test.cpp -o build/steam-cancel-failure-test
./build/steam-cancel-failure-test
python3 tools/prepare_steam_cancel_cleanup.py build/cancel-overlay
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -Ibuild/cancel-overlay -Ivendor/playden-source/src src/steam_integration/downloads.cpp build/cancel-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_cancel_cleanup_regression_test.cpp -o build/steam-cancel-cleanup-test
./build/steam-cancel-cleanup-test
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -DWINE11_DURABLE_CANCEL_QUIESCE=1 -Ibuild/cancel-overlay -Ivendor/playden-source/src build/cancel-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/platform/effective_uid.cpp tests/steam_cancel_quiesce_test.cpp -o build/steam-cancel-quiesce-test
./build/steam-cancel-quiesce-test
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -DWINE11_DURABLE_CANCEL_QUIESCE=1 -Ibuild/cancel-overlay -Ivendor/playden-source/src src/steam_integration/cancel_cleanup_journal.cpp src/steam_integration/cancel_winner_evidence.cpp vendor/playden-source/src/steam/backend/json.cpp src/steam_integration/cancel_target_evidence.cpp src/steam_integration/cancel_registry_plan.cpp src/steam_integration/cancel_registry_reconciliation.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp build/cancel-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_cancel_quiesce_journal_test.cpp -o build/steam-cancel-quiesce-journal-test
./build/steam-cancel-quiesce-journal-test
g++ -std=c++20 -O2 -pthread tests/steam_install_generation_scope_test.cpp -o build/steam-install-generation-scope-test
./build/steam-install-generation-scope-test
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -Ibuild/cancel-overlay -Ivendor/playden-source/src src/steam_integration/cancel_cleanup_journal.cpp src/steam_integration/cancel_winner_evidence.cpp vendor/playden-source/src/steam/backend/json.cpp src/steam_integration/cancel_target_evidence.cpp src/steam_integration/cancel_registry_plan.cpp src/steam_integration/cancel_registry_reconciliation.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp build/cancel-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_cancel_cleanup_journal_test.cpp -Wl,--wrap=fsync -Wl,--wrap=renameat -o build/steam-cancel-cleanup-journal-test
./build/steam-cancel-cleanup-journal-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/cancel_target_evidence.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_cancel_target_evidence_test.cpp -Wl,--wrap=fsync -o build/steam-cancel-target-evidence-test
./build/steam-cancel-target-evidence-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/cancel_winner_evidence.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/steam/backend/json.cpp tests/steam_cancel_winner_evidence_test.cpp -Wl,--wrap=fsync -o build/steam-cancel-winner-evidence-test
./build/steam-cancel-winner-evidence-test
g++ -std=c++20 -O2 tests/steam_anchored_fs_probe_test.cpp -o build/steam-anchored-fs-probe-test
./build/steam-anchored-fs-probe-test
g++ -std=c++20 -O2 src/steam_integration/cancel_registry_plan.cpp tests/steam_cancel_registry_plan_test.cpp -o build/steam-cancel-registry-plan-test
./build/steam-cancel-registry-plan-test
g++ -std=c++20 -O2 tests/download_tuner_test.cpp -o build/download-tuner-test
./build/download-tuner-test
# Playden credential model: hand-over of root-created files and app-user startup.
# Both need real root for their fixtures and print SKIP otherwise (run with sudo to exercise them).
g++ -std=c++20 -O2 -Wall -Wextra -Werror src/ownership_adopt.cpp tests/ownership_adopt_test.cpp -o build/ownership-adopt-test
./build/ownership-adopt-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror src/ownership_adopt.cpp src/steam_integration/app_user_identity.cpp tests/app_user_identity_test.cpp -o build/app-user-identity-test
./build/app-user-identity-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror src/privilege.cpp tests/privilege_test.cpp -o build/privilege-test
./build/privilege-test
# Vapor per-game controller layouts: values Playden accepts and the [input] profile they render.
cat > build/no-stub-probe.cpp <<'CPP'
// Test-only link placeholder: the controller-layout test never inspects programs.
#include "runtime/steam_stub.h"
namespace playden::runtime { SteamStubInfo InspectSteamStub(const std::string&) { return {}; } }
CPP
g++ -std=c++20 -O1 -pthread -Ivendor/playden-source/src tests/vapor_controller_layouts_test.cpp src/steam_integration/controller_layouts.cpp vendor/playden-source/src/runtime/game_settings.cpp vendor/playden-source/src/runtime/game_probe.cpp vendor/playden-source/src/runtime/prospero_profile.cpp build/no-stub-probe.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/steam/backend/app_info.cpp vendor/playden-source/src/steam/backend/vdf.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/library/store.cpp -o build/vapor-controller-layouts-test
./build/vapor-controller-layouts-test vendor/playden-source > /dev/null
python3 tests/test_prepare_steam_uid_guard.py
python3 tools/prepare_steam_uid_guard.py vendor/playden-source/src/platform/effective_uid.cpp build/steam-guarded-effective-uid.cpp
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src -Dgeteuid=fake_geteuid -Dseteuid=fake_seteuid build/steam-guarded-effective-uid.cpp tests/steam_uid_exception_test.cpp -o build/steam-uid-exception-test
./build/steam-uid-exception-test
g++ -std=c++20 -O2 -pthread tests/steam_network_traffic_test.cpp tests/steam_network_scope_helper.cpp -o build/steam-network-traffic-test
./build/steam-network-traffic-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror tests/steam_safe_remove_tree_test.cpp -o build/steam-safe-remove-tree-test
./build/steam-safe-remove-tree-test
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/uninstall_journal.cpp src/steam_integration/uninstall_registry.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/library/store.cpp tests/steam_uninstall_journal_test.cpp -Wl,--wrap=fsync -Wl,--wrap=renameat -o build/steam-uninstall-journal-test
./build/steam-uninstall-journal-test
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/uninstall_quarantine.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_uninstall_quarantine_test.cpp -Wl,--wrap=fsync -Wl,--wrap=renameat -o build/steam-uninstall-quarantine-test
./build/steam-uninstall-quarantine-test
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/uninstall_registry.cpp src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/library/store.cpp tests/steam_uninstall_registry_test.cpp -Wl,--wrap=fsync -Wl,--wrap=renameat -o build/steam-uninstall-registry-test
./build/steam-uninstall-registry-test
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/protected_uninstall.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_protected_uninstall_test.cpp -Wl,--wrap=fsync -Wl,--wrap=unlinkat -o build/steam-protected-uninstall-test
if [[ $(id -u) == 0 ]]; then ./build/steam-protected-uninstall-test; else echo 'SKIP: protected-uninstall credential test requires a root test process'; fi
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/vapor_game_cleanup.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_vapor_game_cleanup_test.cpp -o build/steam-vapor-game-cleanup-test
./build/steam-vapor-game-cleanup-test
g++ -std=c++20 -O1 -pthread -DWINE11_SERVICE_QUIESCENCE=1 -DWINE11_DISABLE_UNSAFE_CANCEL=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src src/steam_integration/uninstall_journal.cpp src/steam_integration/uninstall_registry.cpp src/steam_integration/protected_uninstall.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_uninstall_recovery_integration_test.cpp -Wl,--wrap=fsync -Wl,--wrap=renameat -Wl,--wrap=unlinkat -o build/steam-uninstall-recovery-integration-test
if [[ $(id -u) == 0 ]]; then ./build/steam-uninstall-recovery-integration-test; else echo 'SKIP: uninstall recovery integration requires a root test process'; fi
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ivendor/playden-source/src tests/steam_uninstall_next_step_test.cpp -o build/steam-uninstall-next-step-test
./build/steam-uninstall-next-step-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ivendor/playden-source/src src/steam_integration/title_removal.cpp vendor/playden-source/src/library/store.cpp tests/steam_title_removal_test.cpp -o build/steam-title-removal-test
./build/steam-title-removal-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror src/steam_integration/title_absence.cpp tests/steam_title_absence_test.cpp -o build/steam-title-absence-test
./build/steam-title-absence-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread -Ivendor/playden-source/src src/steam_integration/uninstall_flow.cpp src/steam_integration/title_removal.cpp vendor/playden-source/src/library/store.cpp tests/steam_uninstall_flow_test.cpp -o build/steam-uninstall-flow-test
./build/steam-uninstall-flow-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Ivendor/playden-source/src src/steam_integration/vapor_title_remover.cpp tests/steam_vapor_title_remover_test.cpp -o build/steam-vapor-title-remover-test
./build/steam-vapor-title-remover-test
# The receiver test uses the same bounded HTTP source and tested curl headers.
python3 tools/prepare_steam_http_observer.py build/test-observed-http.cpp
if [[ -f build/steam-network-tested-snapshot/include/curl/curl.h ]]; then
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include tests/steam_network_observer_test.cpp -Wl,--gc-sections -o build/steam-network-observer-test
  ./build/steam-network-observer-test
  # Actual generated downloader workers with a controlled tuning clock and fake content.
  python3 tools/prepare-steam-depot-tuning.py build/test-observed-depot.cpp
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-observed-depot.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/install_volumes.cpp tests/steam_architecture_collision_test.cpp -Wl,--gc-sections -o build/steam-architecture-collision-test
  ./build/steam-architecture-collision-test
  python3 tools/prepare_steam_cleanup_remover.py build/test-observed-depot.cpp build/test-cleanup-remover.cpp
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-cleanup-remover.cpp tests/steam_cleanup_remover_test.cpp -Wl,--gc-sections -Wl,--wrap=lstat -o build/steam-cleanup-remover-test
  ./build/steam-cleanup-remover-test
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-cleanup-remover.cpp tests/steam_install_winner_sync_test.cpp -Wl,--gc-sections -Wl,--wrap=fsync -o build/steam-install-winner-sync-test
  ./build/steam-install-winner-sync-test
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-cleanup-remover.cpp tests/steam_install_data_sync_test.cpp -Wl,--gc-sections -o build/steam-install-data-sync-test
  ./build/steam-install-data-sync-test
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-cleanup-remover.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/install_volumes.cpp tests/steam_install_winner_worker_test.cpp -Wl,--gc-sections -Wl,--wrap=fsync -o build/steam-install-winner-worker-test
  ./build/steam-install-winner-worker-test
  g++ -std=c++20 -O1 -pthread -DWINE11_INSTALL_GENERATION_EXPERIMENT=1 -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-cleanup-remover.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/install_volumes.cpp tests/steam_install_winner_worker_test.cpp -Wl,--gc-sections -Wl,--wrap=fsync -o build/steam-install-generation-worker-test
  ./build/steam-install-generation-worker-test
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-observed-depot.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/library/store.cpp tests/steam_free_space_query_test.cpp -Wl,--gc-sections -o build/steam-free-space-query-test
  ./build/steam-free-space-query-test
  python3 tools/prepare_steam_tuner_test.py build/test-observed-depot.cpp build/test-slots-depot.cpp
  g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include build/test-slots-depot.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/install_volumes.cpp tests/steam_download_workers_test.cpp -Wl,--gc-sections -o build/steam-download-workers-test
  ./build/steam-download-workers-test
else
  echo 'SKIP: HTTP receiver and downloader worker tests need the tested Steam curl headers.'
fi
g++ -std=c++20 -O2 src/vapor_shortcuts.cpp tests/vapor_shortcuts_test.cpp -o build/vapor-shortcuts-test
./build/vapor-shortcuts-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/artwork.cpp vendor/playden-source/src/steam/store_details_service.cpp vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/library/store.cpp tests/steam_artwork_service_test.cpp -o build/steam-artwork-service-test
./build/steam-artwork-service-test
g++ -std=c++20 -O2 src/image_codec.cpp src/privilege.cpp tests/steam_artwork_decode_test.cpp -o build/steam-artwork-decode-test
./build/steam-artwork-decode-test
python3 tests/startup_order_test.py
# Exercise the coordinator against Playden's real worker services with fake account/network boundaries.
gcc -std=c11 -c vendor/playden-source/third_party/qrcodegen/qrcodegen.c -o build/qrcodegen-test.o
g++ -std=c++20 -pthread -DWINE11_SERVICE_QUIESCENCE=1 -DWINE11_DISABLE_UNSAFE_CANCEL=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen src/steam_integration/ui_adapter.cpp src/steam_integration/remember_setting.cpp src/steam_integration/controller_layouts.cpp src/steam_integration/preparation.cpp src/steam_integration/cloud_preflight.cpp vendor/playden-source/src/steam/cloud_service.cpp src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp src/steam_integration/ready_launch.cpp src/steam_integration/downloads.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp build/qrcodegen-test.o src/steam_integration/controller.cpp vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/library/store.cpp tests/steam_quiescence_adapter_test.cpp -o build/steam-quiescence-adapter-test
./build/steam-quiescence-adapter-test
g++ -std=c++20 -O1 -pthread -DWINE11_EXPERIMENT_UNINSTALL=1 -DWINE11_SERVICE_QUIESCENCE=1 -DWINE11_DISABLE_UNSAFE_CANCEL=1 -Ibuild/quiescence-overlay -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen src/steam_integration/ui_adapter.cpp src/steam_integration/remember_setting.cpp src/steam_integration/controller_layouts.cpp src/steam_integration/preparation.cpp src/steam_integration/cloud_preflight.cpp vendor/playden-source/src/steam/cloud_service.cpp src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp src/steam_integration/ready_launch.cpp src/steam_integration/downloads.cpp src/steam_integration/title_removal.cpp src/steam_integration/uninstall_flow.cpp build/quiescence-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp build/qrcodegen-test.o src/steam_integration/controller.cpp vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/library/store.cpp tests/steam_uninstall_adapter_test.cpp -o build/steam-uninstall-adapter-test
./build/steam-uninstall-adapter-test
g++ -std=c++20 -pthread -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen src/steam_integration/ui_adapter.cpp src/steam_integration/remember_setting.cpp src/steam_integration/controller_layouts.cpp src/steam_integration/preparation.cpp src/steam_integration/cloud_preflight.cpp vendor/playden-source/src/steam/cloud_service.cpp src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp src/steam_integration/ready_launch.cpp src/steam_integration/downloads.cpp vendor/playden-source/src/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp build/qrcodegen-test.o src/steam_integration/controller.cpp vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/library/store.cpp tests/steam_controller_test.cpp -o build/steam-controller-test
./build/steam-controller-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/downloads.cpp vendor/playden-source/src/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp vendor/playden-source/src/library/store.cpp tests/steam_downloads_test.cpp -o build/steam-downloads-test
./build/steam-downloads-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/library/store.cpp tests/steam_download_guard_test.cpp -o build/steam-download-guard-test
./build/steam-download-guard-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_native_paths_test.cpp -o build/steam-native-paths-test
./build/steam-native-paths-test
g++ -std=c++20 -O2 -pthread src/steam_integration/remember_setting.cpp tests/steam_remember_setting_test.cpp -o build/steam-remember-setting-test
./build/steam-remember-setting-test
python3 tools/prepare-steam-session-experiment.py build/steam-guarded-session.cpp
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src build/steam-guarded-session.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/steam/session_store.cpp tests/steam_session_uid_order_test.cpp -o build/steam-session-uid-order-test
./build/steam-session-uid-order-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/native_paths.cpp src/steam_integration/remember_setting.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/steam/session_store.cpp build/steam-guarded-session.cpp tests/steam_remember_session_integration_test.cpp -o build/steam-remember-session-integration-test
./build/steam-remember-session-integration-test
g++ -std=c++20 -O2 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -Ivendor/playden-source/src src/explorer_fileops.cpp src/privilege.cpp src/steam_integration/native_startup.cpp src/steam_integration/native_bridge.cpp tests/steam_native_startup_test.cpp -o build/steam-native-startup-test
./build/steam-native-startup-test
g++ -std=c++20 -O2 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -Ivendor/playden-source/src src/explorer_fileops.cpp src/privilege.cpp src/steam_integration/native_game_startup.cpp tests/steam_native_game_startup_test.cpp -o build/steam-native-game-startup-test
./build/steam-native-game-startup-test
g++ -std=c++20 -O2 -Ivendor/playden-source/src tests/steam_cloud_location_test.cpp vendor/playden-source/src/library/store.cpp -o build/steam-cloud-location-test
./build/steam-cloud-location-test
g++ -std=c++20 -O2 src/steam_integration/native_app_mount.cpp tests/steam_native_app_mount_test.cpp -o build/steam-native-app-mount-test
./build/steam-native-app-mount-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/ready_launch.cpp vendor/playden-source/src/library/store.cpp tests/steam_ready_launch_test.cpp -o build/steam-ready-launch-test
./build/steam-ready-launch-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/preparation.cpp vendor/playden-source/src/library/store.cpp tests/steam_preparation_test.cpp -o build/steam-preparation-test
./build/steam-preparation-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp vendor/playden-source/src/library/store.cpp tests/steam_launch_details_test.cpp -o build/steam-launch-details-test
./build/steam-launch-details-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/cloud_preflight.cpp src/steam_integration/cloud_launch_gate.cpp src/steam_integration/ready_launch.cpp vendor/playden-source/src/steam/cloud_service.cpp vendor/playden-source/src/library/store.cpp tests/steam_cloud_preflight_test.cpp -o build/steam-cloud-preflight-test
./build/steam-cloud-preflight-test
python3 tools/prepare-steam-cloud-experiment.py build/steam-cloud-guard-service.cpp
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src build/steam-cloud-guard-service.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/library/store.cpp tests/steam_cloud_uid_guard_test.cpp -o build/steam-cloud-uid-guard-test
./build/steam-cloud-uid-guard-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src build/steam-cloud-guard-service.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp vendor/playden-source/src/library/store.cpp tests/steam_uid_worker_coordination_test.cpp -o build/steam-uid-worker-coordination-test
./build/steam-uid-worker-coordination-test
g++ -std=c++20 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections -Ivendor/playden-source/src src/steam_integration/redist_fetch.cpp tests/steam_redist_fetch_test.cpp -o build/steam-redist-fetch-test
./build/steam-redist-fetch-test
g++ -std=c++20 -O2 -Ivendor/playden-source/src src/steam_integration/runtime_install_id.cpp vendor/playden-source/src/library/store.cpp tests/steam_runtime_install_id_test.cpp -o build/steam-runtime-install-id-test
./build/steam-runtime-install-id-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src tests/steam_runtime_identity_repository_test.cpp src/steam_integration/runtime_install_id.cpp vendor/playden-source/src/runtime/game_install.cpp vendor/playden-source/src/library/store.cpp vendor/playden-source/src/platform/storage.cpp vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/steam/backend/game_path.cpp vendor/playden-source/src/steam/backend/app_info.cpp vendor/playden-source/src/steam/backend/vdf.cpp vendor/playden-source/src/steam/backend/binary_kv.cpp -o build/steam-runtime-identity-repository-test
./build/steam-runtime-identity-repository-test
gcc -std=c11 -D_DEFAULT_SOURCE -Ivendor/playden-lapy/source -c vendor/playden-lapy/source/title_rule.c -o build/steam-title-rule-test.o
g++ -std=c++20 -O2 -Ivendor/playden-lapy/source src/steam_integration/title_list_merge.cpp tests/steam_title_list_merge_test.cpp build/steam-title-rule-test.o -o build/steam-title-list-test
./build/steam-title-list-test
g++ -std=c++20 -O2 src/steam_integration/title_list_merge.cpp src/steam_integration/title_list_writer.cpp tests/steam_title_list_writer_test.cpp -o build/steam-title-list-writer-test
./build/steam-title-list-writer-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src -Ivendor/playden-lapy/source src/steam_integration/dedicated_title_list.cpp src/steam_integration/title_list_writer.cpp src/steam_integration/title_list_merge.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_dedicated_title_list_test.cpp build/steam-title-rule-test.o -o build/steam-dedicated-title-list-test
./build/steam-dedicated-title-list-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/native_host_config.cpp src/steam_integration/title_list_writer.cpp src/steam_integration/title_list_merge.cpp vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp tests/steam_native_host_config_test.cpp -o build/steam-native-host-config-test
./build/steam-native-host-config-test
g++ -std=c++20 -O2 -Ivendor/playden-source/src src/steam_integration/native_kit_admission.cpp tests/steam_native_kit_admission_test.cpp -o build/steam-native-kit-admission-test
./build/steam-native-kit-admission-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/shell_test.cpp -o build/shell-test
g++ -std=c++20 -O2 tests/launch_test.cpp -o build/launch-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/storage_test.cpp -o build/storage-test
./build/shell-test
./build/launch-test
./build/storage-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/preferences_browser_test.cpp -o build/preferences-browser-test
./build/preferences-browser-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/desktop3_test.cpp -o build/desktop3-test
./build/desktop3-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/desktop_context_test.cpp -o build/desktop-context-test
./build/desktop-context-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/input_test.cpp -o build/input-test
./build/input-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/controller_test.cpp -o build/controller-test
./build/controller-test
python3 tests/branding_test.py
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/explorer_test.cpp -o build/explorer-test
./build/explorer-test
python3 tests/test_steam_fresh_inputs.py
python3 tests/test_steam_review_assets.py
python3 tests/test_prospero_validator_source.py
python3 tests/test_game_launcher_source_archive.py
g++ -std=c++20 -Wall -Wextra -Werror tests/vapor_game_details_test.cpp -o build/vapor-game-details-test
./build/vapor-game-details-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/steam_ui_test.cpp -o build/steam-ui-test
./build/steam-ui-test
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -Ibuild/cancel-overlay -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp src/steam_integration/controller.cpp src/steam_integration/ui_adapter.cpp src/steam_integration/remember_setting.cpp src/steam_integration/controller_layouts.cpp src/steam_integration/preparation.cpp src/steam_integration/cloud_preflight.cpp vendor/playden-source/src/steam/cloud_service.cpp src/steam_integration/launch_details.cpp vendor/playden-source/src/steam/app_details_service.cpp src/steam_integration/ready_launch.cpp src/steam_integration/downloads.cpp build/cancel-overlay/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp build/qrcodegen-test.o vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/library/store.cpp tests/steam_ui_integration_test.cpp -o build/steam-ui-integration-test
./build/steam-ui-integration-test
g++ -std=c++20 -O2 -DWINE11_NATIVE_EXPLORER_TEST src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/explorer_native_test.cpp -o build/explorer-native-test
./build/explorer-native-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp src/ps5_hid.cpp tests/native_input_test.cpp -o build/native-input-test
./build/native-input-test
g++ -std=c++20 -O2 src/ps5_browser.cpp tests/native_browser_test.cpp -o build/native-browser-test
./build/native-browser-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp src/host.cpp -o build/preview
for state in desktop start library calendar settings personalization browser keyboard dark about mouse quick tray input search-keyboard keyboard-dark ps5-controller controller-test; do ./build/preview "$state" "build/$state.ppm"; done

bash tools/test-game-inventory.sh
g++ -std=c++20 -O2 src/notepad_document.cpp src/image_codec.cpp src/privilege.cpp tests/notepad_document_test.cpp -o build/notepad-document-test
./build/notepad-document-test
g++ -std=c++20 -O2 src/notepad_document.cpp src/image_codec.cpp src/privilege.cpp tests/notepad_saveas_test.cpp -o build/notepad-saveas-test
./build/notepad-saveas-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/notepad_input_test.cpp -o build/notepad-input-test
./build/notepad-input-test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/paint_test.cpp -o build/paint_test
./build/paint_test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/paint_input_test.cpp -o build/paint_input_test
./build/paint_input_test

g++ -std=c++20 -O2 src/image_codec.cpp src/privilege.cpp tests/paint_save_failure_test.cpp -Wl,--wrap=write -o build/paint-save-failure-test
./build/paint-save-failure-test

g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/explorer_fileops_test.cpp -o build/explorer-fileops-test
./build/explorer-fileops-test
g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/explorer_treeops_test.cpp -o build/explorer-treeops-test
./build/explorer-treeops-test
g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/explorer_purge_test.cpp -o build/explorer-purge-test
./build/explorer-purge-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/explorer_context_test.cpp -o build/explorer-context-test
./build/explorer-context-test

g++ -std=c++20 -O2 -DWINE11_NATIVE_IMAGE_TEST src/image_codec.cpp src/privilege.cpp tests/image_codec_native_test.cpp -o build/image-codec-native-test
./build/image-codec-native-test
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/paint_large_image_test.cpp -o build/paint-large-image-test
./build/paint-large-image-test

g++ -std=c++20 -O2 src/web_history.cpp tests/web_history_test.cpp -o build/web-history-test
./build/web-history-test
g++ -std=c++20 -O2 src/web_history.cpp tests/web_history_save_failure_test.cpp -Wl,--wrap=write -o build/web-history-save-failure-test
./build/web-history-save-failure-test
g++ -std=c++20 -O2 -DWINE11_NATIVE_HISTORY_TEST src/web_history.cpp tests/web_history_native_test.cpp -o build/web-history-native-test
./build/web-history-native-test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/web_history_ui_test.cpp -o build/web-history-ui-test
./build/web-history-ui-test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/paint_history_test.cpp -o build/paint-history-test
./build/paint-history-test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/owner_check_ui_test.cpp -o build/owner-check-ui-test
./build/owner-check-ui-test
bash tools/test-owner-admission.sh

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/office_apps_test.cpp -o build/office-apps-test
./build/office-apps-test

g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/office_xlsx_test.cpp -o build/office-xlsx-test
./build/office-xlsx-test

# Actual Save As / close-name / recycle confirmation UI fixture.
g++ -std=c++20 -O2 src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp tests/saveas_recycle_ui_test.cpp -o build/saveas-recycle-ui-test
./build/saveas-recycle-ui-test

# Actual Windows-style Explorer navigation and operation fixture.
sources='src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp'
g++ -std=c++20 -O1 $sources tests/explorer_browser_test.cpp -o build/explorer-browser-test
./build/explorer-browser-test

# Actual console C/user profile migration and failure-retention checks.
sources='src/shell.cpp src/vapor_shortcuts.cpp src/desktop_ui.cpp src/preferences.cpp src/pointer.cpp src/controller_input.cpp src/explorer.cpp src/privilege.cpp src/explorer_fileops.cpp src/explorer_context.cpp src/notepad_document.cpp src/notepad_ui.cpp src/paint.cpp src/image_codec.cpp src/game_inventory.cpp src/web_history.cpp src/tray_ui.cpp src/icons.cpp src/text_input.cpp src/keyboard_ui.cpp src/hid_input.cpp'
g++ -std=c++20 -O1 $sources tests/console_profile_test.cpp -o build/console-profile-test
./build/console-profile-test

# Standalone recovery-tree, confirmed purge and Save As model checks.
g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/explorer_treeops_test.cpp -o build/explorer-treeops-test
./build/explorer-treeops-test
g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/explorer_purge_test.cpp -o build/explorer-purge-test
./build/explorer-purge-test
g++ -std=c++20 -O2 src/notepad_document.cpp src/image_codec.cpp src/privilege.cpp tests/notepad_saveas_test.cpp -o build/notepad-saveas-test
./build/notepad-saveas-test
