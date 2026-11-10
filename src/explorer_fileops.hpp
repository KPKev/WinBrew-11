// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iterator>
#include "recovery_snapshot.hpp"
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
// Cancellable, read-only discovery of old per-folder recovery stores. Work is
// spread across calls so traversing a large drive does not freeze the UI.
struct RecoveryWalker {
  struct Pending { std::string path; uint64_t device, inode; };
  std::vector<Pending> pending;
  std::vector<std::string> stores, names;
  std::string current, excluded;
  Info current_info{};
  uint64_t current_device = 0;
  size_t name_index = 0, directories = 0;
  size_t name_offset = 0;
  bool names_more = false;
  bool active = false, complete = false, failed = false;
  bool begin(const std::vector<std::string> &roots, const char *exclude);
  void step(size_t budget = 64);
  void cancel();
};
// Delete is a same-volume rename into a fresh, private recovery container.
// It never recursively unlinks data or replaces another item.
bool trash(const char *, const Info &, char *recovery, size_t capacity, const char *store = nullptr);
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
  bool begin_recovery(const Clipboard &, const char *, const Io * = nullptr);
  bool begin_transfer(const Clipboard &, const char *, const Io *, bool recovery);
  void step();
  void cancel();
  bool fail(const char *, int);
  void close_handles();
  ~Transfer() { close_handles(); }
};
// A fallible, compact list of paths used by large recovery transfers. Allocation
// failures are reported before the original item is removed.
struct CheckedTransferEntries {
  struct Entry { char *relative; Info info; };
  Entry *items=nullptr;
  size_t count=0, capacity=0;
  CheckedTransferEntries()=default;
  CheckedTransferEntries(const CheckedTransferEntries&)=delete;
  CheckedTransferEntries& operator=(const CheckedTransferEntries&)=delete;
  ~CheckedTransferEntries(){clear();}
  void clear() noexcept {for(size_t i=0;i<count;++i)std::free(items[i].relative);std::free(items);items=nullptr;count=capacity=0;}
  bool append(const char *path,const Info &info) noexcept {
    if(!path)return false;
    const size_t n=std::strlen(path);
    if(n>=1024)return false;
    if(count==capacity){
      if(capacity>std::numeric_limits<size_t>::max()/2)return false;
      const size_t next=capacity?capacity*2:64;
      if(next>std::numeric_limits<size_t>::max()/sizeof(Entry))return false;
      void *p=std::realloc(items,next*sizeof(Entry));if(!p)return false;
      items=static_cast<Entry *>(p);capacity=next;
    }
    char *copy=static_cast<char *>(std::malloc(n+1));if(!copy)return false;
    std::memcpy(copy,path,n+1);items[count++]={copy,info};return true;
  }
  size_t size() const noexcept{return count;}
  bool empty() const noexcept{return !count;}
  Entry &operator[](size_t i) noexcept{return items[i];}
  const Entry &operator[](size_t i) const noexcept{return items[i];}
  Entry *begin() noexcept{return items;}
  Entry *end() noexcept{return items?items+count:items;}
  const Entry *begin() const noexcept{return items;}
  const Entry *end() const noexcept{return items?items+count:items;}
  auto rbegin() noexcept{return std::reverse_iterator<Entry *>(end());}
  auto rend() noexcept{return std::reverse_iterator<Entry *>(begin());}
};
static_assert(sizeof(CheckedTransferEntries)==sizeof(std::vector<int>) && alignof(CheckedTransferEntries)==alignof(std::vector<int>));
struct TreeTransfer {
  struct Entry { std::string relative; Info info; };
  State state = State::idle;
  uint64_t copied = 0, verified = 0, total = 0;
  char source[1024]{}, destination[1024]{}, directory[1024]{}, stage[32]{};
  char recovery_store[1024]{};
  int error = 0;
  bool destination_created = false, moving = false, include_recovery = false;
  CheckedTransferEntries entries, outputs;
  size_t index = 0;
  std::unique_ptr<Transfer> child;
  bool busy() const { return state == State::copying || state == State::verifying; }
  bool begin(const Clipboard &, const char *, bool recovery = false, const char *store = nullptr);
  void step();
  void cancel();
  bool fail(const char *, int);
};
// Cross-volume delete: first make and verify a visible copy in the bin, then
// remove only unchanged original entries. A stopped or failed copy keeps the
// original; a later removal failure leaves the verified copy in the bin.
struct CrossVolumeRecycle {
  State state = State::idle;
  TreeTransfer transfer;
  bool removing = false;
  size_t remaining = 0, removed = 0;
  int error = 0, parent_fd = -1, store_fd = -1;
  std::string parent, leaf, store_path;
  Info parent_info{}, store_info{};
  bool busy() const { return state == State::copying || state == State::verifying; }
  bool begin(const Clipboard &, const char *store);
  void step();
  void cancel();
  bool fail(int);
  void close();
  ~CrossVolumeRecycle() { close(); }
};
// Permanent removal is confined to a selected recovery store. Preparation
// snapshots the complete contents; confirmation revalidates before deletion.
struct Purge {
  std::string root;
  std::string selected;
  RecoverySnapshot<Info> entries;
  Info root_info;
  State state = State::idle;
  uint64_t bytes = 0;
  size_t remaining = 0, removed = 0;
  int error = 0, root_fd = -1;
  bool ready = false;
  bool busy() const { return state == State::copying; }
  bool prepare(const char *);
  bool prepare_target(const char *store, const char *relative);
  bool prepare_item(const char *store, const char *item);
  bool begin();
  void step();
  void cancel();
  bool fail(int);
  void close();
  ~Purge() { close(); }
};
// Returns the store for this folder, or the enclosing store when viewing it.
bool recovery_location(const char *, char *, size_t);
// Return the original item name from an old generated recovery container.
bool legacy_recovery_name(const char *, std::string &);
// Read-only, bounded discovery of legacy per-folder recovery stores. The
// output is published only after a complete scan of the requested tree.
bool discover_recovery_stores(const char *, std::vector<std::string> &);
// Snapshot the items in legacy stores for a later cancellable migration.
// No files are moved and output changes only after a complete successful scan.
bool collect_recovery_items(const char *root, const char *destination, std::vector<Clipboard> &output);
bool collect_recovery_stores(const std::vector<std::string> &stores, const char *destination, std::vector<Clipboard> &output);
// Collect up to one bounded batch. Repeating after moving the batch eventually
// drains all stores, including stores containing many empty old containers.
bool collect_recovery_batch(const std::vector<std::string> &stores, const char *destination, std::vector<Clipboard> &output, bool &more);
// Move legacy recovery containers into the selected same-volume bin. Failure
// retains unmoved entries; already moved entries remain in the destination.
bool consolidate_recovery_stores(const char *profile, const char *destination, size_t &moved);
bool ensure_folder(const char *);
bool list_folders(const char *, std::vector<std::string> &, size_t limit=128);
bool rename_item(const char *, const Info &, const char *, int &error);
bool personal_files_snapshot(const char *, std::vector<TreeTransfer::Entry> &);
bool prepare_user_profile(const char *legacy, const char *console, const char *target, std::string &error);
} // namespace ww::fileops
