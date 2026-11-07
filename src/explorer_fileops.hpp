// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace ww::fileops {
struct Info {
  uint64_t device = 0, inode = 0, size = 0;
  int64_t modified = 0, changed = 0;
  long modified_ns = 0, changed_ns = 0;
  unsigned mode = 0;
  bool regular = false, directory = false;
};
// No operation deletes, truncates, or replaces an existing path. A failed or
// cancelled exclusive destination is retained and reported as incomplete.
struct Io {
  void *context = nullptr;
  int (*open)(void *, const char *, bool exclusive_destination) = nullptr;
  int64_t (*read)(void *, int, void *, size_t) = nullptr;
  int64_t (*write)(void *, int, const void *, size_t) = nullptr;
  int (*rewind)(void *, int) = nullptr;
  int (*info)(void *, int, Info &) = nullptr;
  int (*permissions)(void *, int, unsigned) = nullptr;
  int (*sync)(void *, int) = nullptr;
  int (*close)(void *, int) = nullptr;
};
const Io &system_io();
int inspect(const char *, Info &, const Io * = nullptr);
bool identity(const Info &, const Info &);
bool unchanged(const Info &, const Info &);
struct Clipboard {
  bool ready = false;
  char path[1024]{};
  Info info;
  int error = 0;
  bool copy(const char *, const Io * = nullptr);
};
enum class State { idle, copying, verifying, complete, failed, cancelled };
struct Transfer {
  static constexpr size_t Chunk = 64 * 1024;
  static constexpr uint64_t MaximumBytes = uint64_t(8) * 1024 * 1024 * 1024;
  State state = State::idle;
  uint64_t copied = 0, verified = 0, total = 0;
  char source[1024]{}, destination[1024]{}, directory[1024]{}, stage[32]{};
  int error = 0, input = -1, output = -1;
  bool destination_created = false;
  Info initial, created, parent;
  const Io *io = nullptr;
  unsigned char bytes[Chunk]{}, compare[Chunk]{};
  bool busy() const {
    return state == State::copying || state == State::verifying;
  }
  bool begin(const Clipboard &, const char *, const Io * = nullptr);
  void step();
  void cancel();
  bool fail(const char *, int);
  void close_handles();
  ~Transfer() { close_handles(); }
};
} // namespace ww::fileops
