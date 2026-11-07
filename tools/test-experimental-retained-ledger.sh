#!/usr/bin/env bash
# Explicit host-only experiment; never included in native build or assembly.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src \
  tests/experimental_retained_ledger.cpp tests/experimental_retained_ledger_test.cpp \
  src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp \
  vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp \
  vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp \
  -Wl,--wrap=renameat -Wl,--wrap=fsync -o build/experimental-retained-ledger-test
./build/experimental-retained-ledger-test
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src \
  tests/experimental_retained_ledger.cpp tests/experimental_retained_ledger_process_test.cpp \
  src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp \
  vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp \
  vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp \
  -o build/experimental-retained-ledger-process-test
./build/experimental-retained-ledger-process-test
python3 tools/prepare_steam_cancel_cleanup.py build/experimental-retained-service
g++ -std=c++20 -O2 -pthread -DWINE11_CANCEL_CLEANUP_OVERLAY=1 -DWINE11_DURABLE_CANCEL_QUIESCE=1 \
  -Ibuild/experimental-retained-service -Ivendor/playden-source/src \
  tests/experimental_retained_ledger.cpp tests/experimental_retained_service_test.cpp \
  src/steam_integration/native_paths.cpp vendor/playden-source/src/platform/storage.cpp \
  vendor/playden-source/src/platform/durable_storage.cpp vendor/playden-source/src/platform/effective_uid.cpp \
  vendor/playden-source/src/platform/managed_directory.cpp vendor/playden-source/src/platform/managed_file_lock.cpp \
  build/experimental-retained-service/installs/install_service.cpp vendor/playden-source/src/steam/install_volumes.cpp \
  vendor/playden-source/src/library/store.cpp -o build/experimental-retained-service-test
./build/experimental-retained-service-test
