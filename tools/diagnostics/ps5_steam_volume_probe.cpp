// Read-only space measurement using the same Playden code as downloads.
#include "steam/install_volumes.h"
#include <cstdio>

int main() {
  constexpr const char* paths[] = {"/mnt/sandbox/PPSA77382_000/download0/wine11-steam/wine11-games", "/data"};
  std::puts("STEAM_VOLUME_PROBE start writes=none"); std::fflush(stdout);
  for (int i = 0; i < 2; ++i) {
    auto raw = playden::steam::QueryVolumeSpace(paths[i], 0);
    auto reserved = playden::steam::QueryVolumeSpace(paths[i], playden::steam::kConsoleSystemReserve);
    std::printf("STEAM_VOLUME_PROBE volume=%s raw_ok=%d raw_free=%llu reserved_ok=%d usable=%llu\n",
                i ? "data" : "game-storage", bool(raw), raw ? static_cast<unsigned long long>(raw->free_bytes) : 0,
                bool(reserved), reserved ? static_cast<unsigned long long>(reserved->free_bytes) : 0);
    std::fflush(stdout);
  }
  std::puts("STEAM_VOLUME_PROBE done"); std::fflush(stdout);
  return 0;
}
