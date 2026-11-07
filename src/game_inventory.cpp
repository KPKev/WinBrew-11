// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only adapter for Playden's install registry wire format. Reference:
// playden-ps5 9f5a7cbebeaf07fa3ea5324dce5ad8f374ca0aec,
// src/installs/install_service.cpp and src/library/store.cpp (GPL-3.0-or-later).
#include "game_inventory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_INVENTORY_TEST)
extern "C" {
int sceKernelOpen(const char *, int, int);
int64_t sceKernelRead(int, void *, size_t);
int sceKernelClose(int);
}
#endif

namespace ww::games {
namespace {
using View = std::string_view;
// libc++ string_view::substr retains an out_of_range path even when exceptions
// are disabled. The minimal native runtime has no verbose-abort implementation.
// Clamp these internal slices explicitly, without calling a throwing API.
View slice(View text, size_t begin = 0, size_t length = View::npos) noexcept {
  if (begin >= text.size())
    return {};
  size_t available = text.size() - begin;
  return View(text.data() + begin, length < available ? length : available);
}
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_INVENTORY_TEST)
int kernel_error(int64_t code) {
  uint32_t value = uint32_t(code);
  if ((value & 0xffff0000u) == 0x80020000u && (value & 0xffffu) &&
      (value & 0xffffu) < 4096)
    return int(value & 0xffffu);
  return errno > 0 ? errno : EIO;
}
#endif
bool decimal(View text, uint64_t &number) {
  if (text.empty() || (text.size() > 1 && text[0] == '0'))
    return false;
  number = 0;
  for (char c : text) {
    if (c < '0' || c > '9' ||
        number > (std::numeric_limits<uint64_t>::max() - unsigned(c - '0')) / 10)
      return false;
    number = number * 10 + unsigned(c - '0');
  }
  return true;
}
bool game_key(View key, uint32_t &steam_app) {
  steam_app = 0;
  if (slice(key, 0, 6) == "local-") {
    if (key.size() != 42)
      return false;
    for (size_t n = 6; n < key.size(); ++n) {
      bool dash = n == 14 || n == 19 || n == 24 || n == 29;
      char c = key[n];
      if (dash ? c != '-' : !((c >= '0' && c <= '9') ||
                              (c >= 'a' && c <= 'f')))
        return false;
    }
    return true;
  }
  uint64_t value = 0;
  if (!decimal(key, value) || value == 0 || value > UINT32_MAX)
    return false;
  steam_app = uint32_t(value);
  return true;
}
bool volume_id(View id) {
  if (id.empty() || id.size() > 32)
    return false;
  for (char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
      return false;
  return true;
}
bool root_path(const char *path) {
  if (!path || path[0] != '/')
    return false;
  size_t size = std::strlen(path);
  if (size < 2 || size > 900 || path[size - 1] == '/')
    return false;
  View view(path);
  size_t begin = 1;
  while (begin < size) {
    size_t end = view.find('/', begin);
    if (end == View::npos)
      end = size;
    View part = slice(view, begin, end - begin);
    if (part.empty() || part == "." || part == "..")
      return false;
    for (unsigned char c : part)
      if (c < 32 || c == 127 || c == '\\')
        return false;
    begin = end + 1;
  }
  return true;
}
template <size_t N> bool copy(char (&to)[N], View value) {
  if (value.size() >= N)
    return false;
  std::memcpy(to, value.data(), value.size());
  to[value.size()] = 0;
  return true;
}
bool registry_entry(View line, Entry &entry) {
  View fields[4];
  int count = 0;
  while (count < 4) {
    size_t tab = line.find('\t');
    fields[count++] = slice(line, 0, tab);
    if (tab == View::npos)
      break;
    line.remove_prefix(tab + 1);
    if (count == 4)
      return false;
  }
  if (!game_key(fields[0], entry.steam_app) || !copy(entry.key, fields[0]))
    return false;
  if (count == 1)
    return copy(entry.volume, "internal");
  if (!volume_id(fields[1]) || !copy(entry.volume, fields[1]))
    return false;
  if (count >= 3 && !decimal(fields[2], entry.install_bytes))
    return false;
  if (count == 4) {
    if (fields[3] == "paused")
      entry.state = State::paused;
    else if (fields[3] == "removing")
      entry.state = State::removing;
    else
      return false;
  }
  return true;
}
const char *volume_root(const char *id, const char *primary) {
  if (!std::strcmp(id, "internal")) return primary;
  if (!std::strcmp(id, "m2")) return "/mnt/ext1/playden/games";
  if (!std::strcmp(id, "extended")) return "/mnt/ext0/playden/games";
  if (!std::strcmp(id, "usb0")) return "/mnt/usb0/playden/games";
  if (!std::strcmp(id, "usb1")) return "/mnt/usb1/playden/games";
  return nullptr;
}
// Small bounded JSON reader: only install-record identity/size fields are kept.
// Unknown JSON values are checked/skipped; nesting is bounded, escaped keys and
// duplicate identity fields are refused rather than being interpreted loosely.
struct Json {
  View text;
  size_t at = 0;
  void space() { while (at < text.size() &&
    (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n')) ++at; }
  bool take(char c) { space(); if (at == text.size() || text[at] != c) return false; ++at; return true; }
  bool string(View &value, bool plain = false) {
    if (!take('"')) return false;
    size_t begin = at;
    while (at < text.size()) {
      unsigned char c = text[at++];
      if (c == '"') { value = slice(text, begin, at - begin - 1); return true; }
      if (c < 32) return false;
      if (c == '\\') {
        if (plain || at == text.size()) return false;
        char escape = text[at++];
        if (escape == 'u') {
          for (int n = 0; n < 4; ++n) {
            if (at == text.size()) return false;
            char hex = text[at++];
            if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
                  (hex >= 'A' && hex <= 'F'))) return false;
          }
        } else if (!std::strchr("\"\\/bfnrt", escape)) return false;
      }
    }
    return false;
  }
  bool integer(uint64_t &value) {
    space(); size_t begin = at;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
    return decimal(slice(text, begin, at - begin), value);
  }
  bool skip(int depth = 0) {
    if (depth > 16) return false;
    space(); if (at == text.size()) return false;
    if (text[at] == '"') { View ignored; return string(ignored); }
    if (text[at] == '[') {
      ++at; if (take(']')) return true;
      do { if (!skip(depth + 1)) return false; } while (take(','));
      return take(']');
    }
    if (text[at] == '{') {
      ++at; if (take('}')) return true;
      do { View ignored; if (!string(ignored) || !take(':') || !skip(depth + 1)) return false; } while (take(','));
      return take('}');
    }
    const View literals[] = {"true", "false", "null"};
    for (View literal : literals)
      if (slice(text, at, literal.size()) == literal) { at += literal.size(); return true; }
    size_t begin = at;
    if (text[at] == '-') {
      ++at;
      if (at == text.size() || text[at] < '0' || text[at] > '9') return false;
    }
    uint64_t ignored;
    if (!integer(ignored)) return false;
    if (at < text.size() && text[at] == '.') {
      ++at; size_t digits = at;
      while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
      if (at == digits) return false;
    }
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
      ++at; if (at < text.size() && (text[at] == '-' || text[at] == '+')) ++at;
      size_t digits = at;
      while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
      if (at == digits) return false;
    }
    return at > begin;
  }
};
bool marker(View text, Entry &entry) {
  Json json{text};
  if (!json.take('{')) return false;
  unsigned seen = 0;
  uint64_t version = 0, app = 0, bytes = 0;
  View store, game;
  do {
    View key;
    if (!json.string(key, true) || !json.take(':')) return false;
    unsigned bit = key == "version" ? 1 : key == "app_id" ? 2 :
                   key == "install_bytes" ? 4 : key == "store" ? 8 : key == "game" ? 16 : 0;
    if (seen & bit) return false;
    seen |= bit;
    if (bit == 1) { if (!json.integer(version)) return false; }
    else if (bit == 2) { if (!json.integer(app)) return false; }
    else if (bit == 4) { if (!json.integer(bytes)) return false; }
    else if (bit == 8) { if (!json.string(store, true)) return false; }
    else if (bit == 16) { if (!json.string(game, true)) return false; }
    else if (!json.skip()) return false;
  } while (json.take(','));
  if (!json.take('}')) return false;
  json.space();
  if (json.at != text.size() || (seen & 7) != 7 || version != 1 ||
      app == 0 || app > UINT32_MAX || bytes == 0) return false;
  if (entry.steam_app) {
    if (app != entry.steam_app || (seen & 24)) return false;
  } else if (store != "local" || game != slice(View(entry.key), 6)) return false;
  entry.install_bytes = bytes;
  return true;
}
ReadResult checked_read(ReadFile reader, void *context, const char *path,
                        char *output, size_t capacity) {
  ReadResult result = reader(context, path, output, capacity);
  if (result.status == ReadStatus::ok) {
    if (result.bytes >= capacity)
      return {ReadStatus::too_large, 0, EFBIG};
    if (std::memchr(output, 0, result.bytes))
      return {ReadStatus::failed, 0, EILSEQ};
    output[result.bytes] = 0;
  }
  return result;
}
} // namespace

ReadResult read_file(void *, const char *path, char *output, size_t capacity) {
  if (!path || !output || capacity < 2)
    return {ReadStatus::failed, 0, EINVAL};
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_INVENTORY_TEST)
  // Use native IO directly. The loader's libc companion deliberately provides
  // only a partial C library; file observation must not depend on inert stubs.
  errno = 0;
  int fd = sceKernelOpen(path, 0, 0); // O_RDONLY; never create/truncate a file
  if (fd < 0) {
    int error = kernel_error(fd);
    return {error == ENOENT ? ReadStatus::missing : ReadStatus::failed, 0, error};
  }
  size_t bytes = 0;
  int error = 0;
  bool eof = false;
  while (bytes < capacity - 1) {
    size_t request = capacity - 1 - bytes;
    errno = 0;
    int64_t count = sceKernelRead(fd, output + bytes, request);
    if (count < 0) { error = kernel_error(count); break; }
    if (uint64_t(count) > request) { error = EIO; break; }
    if (count == 0) { eof = true; break; }
    bytes += size_t(count);
  }
  bool too_large = false;
  if (!error && !eof) {
    char extra;
    errno = 0;
    int64_t count = sceKernelRead(fd, &extra, 1);
    if (count < 0) error = kernel_error(count);
    else if (count > 1) error = EIO;
    else too_large = count == 1;
  }
  errno = 0;
  int closed = sceKernelClose(fd);
  if (closed < 0 && !error) error = kernel_error(closed);
  output[bytes] = 0;
  if (error) return {ReadStatus::failed, 0, error};
  if (too_large) return {ReadStatus::too_large, 0, EFBIG};
  return {ReadStatus::ok, bytes, 0};
#else
  errno = 0;
  FILE *file = std::fopen(path, "rb");
  if (!file) {
    int error = errno ? errno : EIO;
    return {error == ENOENT ? ReadStatus::missing : ReadStatus::failed, 0, error};
  }
  size_t bytes = std::fread(output, 1, capacity - 1, file);
  bool failed = std::ferror(file) != 0;
  int extra = failed ? EOF : std::fgetc(file);
  failed = failed || std::ferror(file) != 0;
  int error = failed ? (errno ? errno : EIO) : 0;
  if (std::fclose(file) != 0) { failed = true; error = errno ? errno : EIO; }
  output[bytes] = 0;
  if (failed) return {ReadStatus::failed, 0, error};
  if (extra != EOF) return {ReadStatus::too_large, 0, EFBIG};
  return {ReadStatus::ok, bytes, 0};
#endif
}

void refresh(Inventory &inventory, const char *primary, ReadFile reader, void *context) {
  inventory.count = inventory.skipped = inventory.error = 0;
  inventory.truncated = false;
  inventory.status = ScanStatus::missing;
  if (!root_path(primary) || !reader) {
    inventory.status = ScanStatus::invalid_root;
    inventory.error = EINVAL;
    return;
  }
  char path[1200], registry[RegistryBytes], record[MarkerBytes];
  std::snprintf(path, sizeof(path), "%s/.playden-games", primary);
  ReadResult read = checked_read(reader, context, path, registry, sizeof(registry));
  if (read.status != ReadStatus::ok) {
    inventory.status = read.status == ReadStatus::missing ? ScanStatus::missing :
      read.status == ReadStatus::too_large ? ScanStatus::too_large : ScanStatus::failed;
    inventory.error = read.error;
    return;
  }
  inventory.status = ScanStatus::ready;
  View remaining(registry, read.bytes);
  while (!remaining.empty()) {
    size_t end = remaining.find('\n');
    View line = slice(remaining, 0, end);
    remaining = end == View::npos ? View{} : slice(remaining, end + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) continue;
    Entry entry;
    if (!registry_entry(line, entry)) { ++inventory.skipped; continue; }
    bool duplicate = false;
    for (int i = 0; i < inventory.count; ++i)
      if (!std::strcmp(inventory.entries[i].key, entry.key)) duplicate = true;
    if (duplicate) { ++inventory.skipped; continue; }
    if (inventory.count == MaximumGames) { inventory.truncated = true; continue; }
    const char *root = volume_root(entry.volume, primary);
    if (!root) entry.state = State::unavailable;
    else {
      std::snprintf(entry.root, sizeof(entry.root), "%s/%s", root, entry.key);
      if (entry.state != State::removing) {
        std::snprintf(path, sizeof(path), "%s/.playden-install.json", entry.root);
        ReadResult result = checked_read(reader, context, path, record, sizeof(record));
        if (result.status == ReadStatus::ok) {
          entry.state = marker(View(record, result.bytes), entry) ? State::installed : State::invalid;
          if (entry.state == State::invalid) entry.error = EILSEQ;
        } else if (result.status == ReadStatus::missing) {
          if (entry.install_bytes) entry.state = State::unavailable;
          else if (entry.state != State::paused) entry.state = State::queued;
        } else {
          entry.state = State::unavailable;
          entry.error = result.error ? result.error : EIO;
        }
      }
    }
    inventory.entries[inventory.count++] = entry;
  }
}
const char *state_label(State state) {
  switch (state) {
  case State::installed: return "Installed";
  case State::unavailable: return "Storage unavailable";
  case State::queued: return "Download queued";
  case State::paused: return "Download paused";
  case State::removing: return "Removal in progress";
  case State::invalid: return "Installation needs checking";
  }
  return "Installation needs checking";
}
} // namespace ww::games
