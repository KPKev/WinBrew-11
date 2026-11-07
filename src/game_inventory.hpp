// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace ww::games {
constexpr int MaximumGames = 64;
constexpr size_t RegistryBytes = 32768;
constexpr size_t MarkerBytes = 8192;
enum class State { installed, unavailable, queued, paused, removing, invalid };
enum class ScanStatus { ready, missing, failed, too_large, invalid_root };
enum class ReadStatus { ok, missing, failed, too_large };
struct ReadResult {
  ReadStatus status = ReadStatus::failed;
  size_t bytes = 0;
  int error = 0;
};
// The sole IO boundary is bounded, read-only access to an explicit filename.
// capacity includes the terminating NUL. Successful reads report exact bytes.
using ReadFile = ReadResult (*)(void *context, const char *path, char *output,
                                size_t capacity);
struct Entry {
  char key[44]{}, volume[33]{}, root[1024]{};
  uint32_t steam_app = 0; // zero for a local-<uuid> installation
  uint64_t install_bytes = 0;
  State state = State::invalid;
  int error = 0;
};
struct Inventory {
  Entry entries[MaximumGames]{};
  int count = 0, skipped = 0;
  bool truncated = false;
  ScanStatus status = ScanStatus::missing;
  int error = 0;
};
ReadResult read_file(void *, const char *path, char *output, size_t capacity);
// This observes Playden's registry/markers. It never constructs InstallService:
// that service starts workers and can reconcile/write existing installation state.
// No titles, executable paths, ownership, or playable status are inferred here.
// Native permission failures are surfaced; directory enumeration is not required.
void refresh(Inventory &, const char *primary_root = "/data/playden-games",
             ReadFile reader = read_file, void *context = nullptr);
const char *state_label(State);
} // namespace ww::games
