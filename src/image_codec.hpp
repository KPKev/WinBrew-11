// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace ww::image {
constexpr int MaxWidth = 1024, MaxHeight = 768;
constexpr size_t MaxPixels = size_t(MaxWidth) * MaxHeight;
constexpr size_t WorkspaceBytes = 8 * 1024 * 1024;
constexpr size_t TemporaryWorkspaceLimit = 112 * 1024 * 1024;
constexpr int MaxSourceDimension = 4096;
constexpr size_t MaxSourcePixels = size_t(4096) * 2160;
struct EditingBuffer {
  uint32_t *pixels = nullptr, *undo = nullptr;
  size_t bytes = 0;
};
// Full-resolution on-demand canvas and Undo. A replacement is returned only
// after decoding/IO succeeds; callers keep their previous buffer until then.
bool read_editable(const char *path, EditingBuffer &replacement, int &width,
                   int &height, char *error, size_t capacity);
// Zero on success; on failure the ownership record remains intact for retry.
int release_editable(EditingBuffer &);
constexpr size_t HistoryBytes = 32 * 1024 * 1024;
struct HistoryBuffer {
  unsigned char *data = nullptr;
  size_t bytes = 0;
};
int allocate_history(HistoryBuffer &);
int release_history(HistoryBuffer &);
enum class SavePhase {
  idle,
  create,
  header,
  rows,
  trailer,
  flush,
  close,
  reopen,
  verify,
  verify_close,
  cleanup,
  done,
  failed,
  canceled
};
struct PngSaveJob {
  SavePhase phase = SavePhase::idle;
  int fd = -1, width = 0, height = 0, row = 0, error = 0;
  const uint32_t *pixels = nullptr;
  bool created = false, cancel_requested = false;
  size_t queued = 0, sent = 0, written = 0, verified = 0, total = 0;
  uint32_t crc = 0xffffffffu, adler_a = 1, adler_b = 0;
  uint64_t hash = 14695981039346656037ull,
           verify_hash = 14695981039346656037ull;
  char path[1024]{}, message[200]{}, failure_stage[32]{};
  bool begin(const char *, const uint32_t *, int width, int height);
  void step(size_t byte_budget = 512 * 1024);
  void cancel() {
    if (busy())
      cancel_requested = true;
  }
  bool busy() const {
    return phase >= SavePhase::create && phase <= SavePhase::cleanup;
  }
  unsigned progress() const;
  const char *stage() const;
};
// Explicit preview API: fits larger sources into 1024x768 and reports source
// dimensions. Paint uses read_editable() to preserve full-resolution output.
// A failure never changes the destination pixels.
bool read(const char *path, uint32_t *pixels, int &width, int &height,
          char *error, size_t capacity, int *source_width = nullptr,
          int *source_height = nullptr);
// Decode bounded in-memory artwork on the application thread.
bool read_memory(const unsigned char* bytes, size_t length, uint32_t* pixels, int& width, int& height);
// Exclusively creates a PNG; an existing file is never opened for writing.
// Failed writes remove only the newly created incomplete copy.
bool write_new_png(const char *path, const uint32_t *pixels, int width,
                   int height, char *error, size_t capacity);
size_t workspace_peak();
// Normalized native/POSIX error from the last codec operation (zero on
// success).
int error_code();
} // namespace ww::image
