// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace ww::fileops {
struct Info {
  uint64_t device = 0, inode = 0, size = 0;
  int64_t modified = 0, changed = 0;
  long modified_ns = 0, changed_ns = 0;
  unsigned mode = 0;
  bool regular = false, directory = false;
};
// The copy primitive never truncates or replaces an existing path. A failed or
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
bool volume_mount_matches(const char *expected, const char *actual);
bool volume_space(const char *, uint64_t &total, uint64_t &available, const char *required_mount = nullptr);
const Io &system_io();
int inspect(const char *, Info &, const Io * = nullptr);
bool identity(const Info &, const Info &);
bool unchanged(const Info &, const Info &);
struct Clipboard {
  bool ready = false, move = false;
  char path[1024]{};
  Info info;
  int error = 0;
  bool copy(const char *, const Io * = nullptr);
};
// Delete is a same-volume rename into a fresh, private recovery container.
// It never recursively unlinks data or replaces another item.
bool trash(const char *, const Info &, char *recovery, size_t capacity);
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
struct TreeTransfer {
  struct Entry { std::string relative; Info info; };
  State state = State::idle;
  uint64_t copied = 0, verified = 0, total = 0;
  char source[1024]{}, destination[1024]{}, directory[1024]{}, stage[32]{};
  int error = 0;
  bool destination_created = false, moving = false, include_recovery = false;
  std::vector<Entry> entries, outputs;
  size_t index = 0;
  std::unique_ptr<Transfer> child;
  bool busy() const { return state == State::copying || state == State::verifying; }
  bool begin(const Clipboard &, const char *, bool recovery = false);
  void step();
  void cancel();
  bool fail(const char *, int);
};
// Permanent removal is confined to a selected recovery store. Preparation
// snapshots the complete contents; confirmation revalidates before deletion.
struct Purge {
  std::string root;
  std::vector<TreeTransfer::Entry> entries;
  Info root_info;
  State state = State::idle;
  uint64_t bytes = 0;
  size_t remaining = 0, removed = 0;
  int error = 0, root_fd = -1;
  bool ready = false;
  bool busy() const { return state == State::copying; }
  bool prepare(const char *);
  bool begin();
  void step();
  void cancel();
  bool fail(int);
  void close();
  ~Purge() { close(); }
};
// Returns the store for this folder, or the enclosing store when viewing it.
bool recovery_location(const char *, char *, size_t);
bool ensure_folder(const char *);
bool list_folders(const char *, std::vector<std::string> &, size_t limit=128);
bool rename_item(const char *, const Info &, const char *, int &error);
bool personal_files_snapshot(const char *, std::vector<TreeTransfer::Entry> &);
bool prepare_user_profile(const char *legacy, const char *console, const char *target, std::string &error);
} // namespace ww::fileops
