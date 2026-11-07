#include "../src/game_inventory.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace ww::games;
namespace {
struct Files {
  std::map<std::string, std::string> files;
  std::map<std::string, int> errors;
  std::vector<std::string> reads;
};
ReadResult read(void *context, const char *path, char *out, size_t capacity) {
  auto &fs = *static_cast<Files *>(context);
  fs.reads.emplace_back(path);
  if (fs.errors.count(path)) return {ReadStatus::failed, 0, fs.errors[path]};
  auto found = fs.files.find(path);
  if (found == fs.files.end()) return {ReadStatus::missing, 0, ENOENT};
  const auto &value = found->second;
  if (value.size() >= capacity) return {ReadStatus::too_large, 0, EFBIG};
  std::memcpy(out, value.data(), value.size());
  return {ReadStatus::ok, value.size(), 0};
}
std::string marker(unsigned app, unsigned long long bytes = 123) {
  return "{\"version\":1,\"app_id\":" + std::to_string(app) +
    ",\"install_bytes\":" + std::to_string(bytes) +
    ",\"files\":19,\"depots\":[{\"id\":1089131,\"manifest\":\"9026743911820486341\"}]}\n";
}
void scan(Inventory &view, Files &fs) { refresh(view, "/games", read, &fs); }
}
int main() {
  Inventory view;
  Files fs;
  fs.files["/games/.playden-games"] = "431240\tinternal\t6018004668\n1089130\tinternal\t1251498163\n";
  fs.files["/games/431240/.playden-install.json"] = marker(431240, 6018004668);
  fs.files["/games/1089130/.playden-install.json"] = marker(1089130, 1251498163);
  scan(view, fs);
  assert(view.status == ScanStatus::ready && view.count == 2 && !view.truncated);
  assert(view.entries[0].steam_app == 431240 && view.entries[0].state == State::installed);
  assert(!std::strcmp(view.entries[1].root, "/games/1089130"));
  assert(view.entries[1].install_bytes == 1251498163);
  assert(fs.reads.size() == 3); // registry plus one marker per game; no account/session reads

  fs.files["/games/.playden-games"] =
    "1\tinternal\t0\n2\tinternal\t0\tpaused\r\n3\tinternal\t999\tremoving\n"
    "4\tusb0\t999\n5\tfuture-drive\t999\n6\tinternal\t999\n7\n";
  scan(view, fs);
  assert(view.count == 7);
  assert(view.entries[0].state == State::queued);
  assert(view.entries[1].state == State::paused);
  assert(view.entries[2].state == State::removing);
  assert(view.entries[3].state == State::unavailable);
  assert(!std::strcmp(view.entries[3].root, "/mnt/usb0/playden/games/4"));
  assert(view.entries[4].state == State::unavailable && !view.entries[4].root[0]);
  assert(view.entries[5].state == State::unavailable);
  assert(view.entries[6].state == State::queued);
  fs.errors["/games/6/.playden-install.json"] = EPERM;
  scan(view, fs);
  assert(view.entries[5].state == State::unavailable && view.entries[5].error == EPERM);
  fs.errors.clear();

  // No traversal, noncanonical ID, overflow, malformed flag or duplicate reaches IO.
  fs.files["/games/.playden-games"] =
    "../1\n0\n01\n4294967296\n12\t../x\t1\n12\tinternal\t18446744073709551616\n"
    "12\tinternal\t1\tdone\n12\tinternal\t1\tpaused\textra\n"
    "12\tinternal\t0\n12\tinternal\t0\n";
  fs.reads.clear();
  scan(view, fs);
  assert(view.count == 1 && view.skipped == 9 && fs.reads.size() == 2);

  fs.files["/games/.playden-games"] = "1089130\tinternal\t1\n";
  fs.files["/games/1089130/.playden-install.json"] = marker(431240);
  scan(view, fs);
  assert(view.entries[0].state == State::invalid);
  fs.files["/games/1089130/.playden-install.json"] =
    "{\"version\":1,\"app_id\":1089130,\"install_bytes\":12,\"app_id\":1089130}";
  scan(view, fs);
  assert(view.entries[0].state == State::invalid);
  for (const char *bad : {
      "{\"fake\":{\"version\":1,\"app_id\":1089130,\"install_bytes\":12}}",
      "{\"version\":2,\"app_id\":1089130,\"install_bytes\":12}",
      "{\"version\":1,\"app_id\":1089130,\"install_bytes\":12}trailing",
      "{\"version\":1,\"app_id\":1089130,\"install_bytes\":18446744073709551616}"}) {
    fs.files["/games/1089130/.playden-install.json"] = bad;
    scan(view, fs);
    assert(view.entries[0].state == State::invalid);
  }
  fs.files["/games/1089130/.playden-install.json"] = marker(1089130) + std::string(1, '\0');
  scan(view, fs);
  assert(view.entries[0].state == State::unavailable && view.entries[0].error == EILSEQ);
  fs.files["/games/1089130/.playden-install.json"] = std::string(MarkerBytes, 'x');
  scan(view, fs);
  assert(view.entries[0].state == State::unavailable && view.entries[0].error == EFBIG);

  const char *local = "local-01234567-89ab-cdef-0123-456789abcdef";
  fs.files["/games/.playden-games"] = std::string(local) + "\tinternal\t99\n";
  fs.files[std::string("/games/") + local + "/.playden-install.json"] =
    "{\"version\":1,\"app_id\":3000000000,\"store\":\"local\",\"game\":\"01234567-89ab-cdef-0123-456789abcdef\",\"install_bytes\":99}";
  scan(view, fs);
  assert(view.count == 1 && !view.entries[0].steam_app && view.entries[0].state == State::installed);

  fs.files["/games/.playden-games"].clear();
  for (int i = 1; i <= MaximumGames + 1; ++i)
    fs.files["/games/.playden-games"] += std::to_string(i) + "\tinternal\t1\n";
  scan(view, fs);
  assert(view.count == MaximumGames && view.truncated);
  fs.files["/games/.playden-games"] = std::string(RegistryBytes, 'x');
  scan(view, fs);
  assert(view.status == ScanStatus::too_large && !view.count && view.error == EFBIG);
  fs.files["/games/.playden-games"].clear();
  scan(view, fs);
  assert(view.status == ScanStatus::ready && !view.count);
  fs.files.erase("/games/.playden-games");
  scan(view, fs);
  assert(view.status == ScanStatus::missing && !view.count);
  fs.errors["/games/.playden-games"] = EACCES;
  scan(view, fs);
  assert(view.status == ScanStatus::failed && !view.count && view.error == EACCES);
  fs.reads.clear();
  for (const char *root : {"relative", "/games/../other", "/games//x", "/games/", "/"}) {
    refresh(view, root, read, &fs);
    assert(view.status == ScanStatus::invalid_root && fs.reads.empty());
  }

  // Actual stdio reader enforces byte limits and never creates missing files.
  char output[5];
  auto missing = read_file(nullptr, "build/inventory-absent-fixture", output, sizeof(output));
  assert(missing.status == ReadStatus::missing);
  const char *fixture = "build/inventory-read-fixture";
  FILE *file = std::fopen(fixture, "wb");
  assert(file);
  assert(std::fwrite("12345", 1, 5, file) == 5);
  assert(std::fclose(file) == 0);
  assert(read_file(nullptr, fixture, output, sizeof(output)).status == ReadStatus::too_large);
  char larger[6];
  auto exact = read_file(nullptr, fixture, larger, sizeof(larger));
  assert(exact.status == ReadStatus::ok && exact.bytes == 5 && !std::strcmp(larger, "12345"));
  assert(std::remove(fixture) == 0);
  std::puts("PASS: read-only installed inventory, identity checks, permissions, absent drives, queued/removing state, malformed input and size limits");
}
