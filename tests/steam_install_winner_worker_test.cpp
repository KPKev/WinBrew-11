#include "steam/backend/depot_installer.h"
#include "steam/backend/depot_chunk.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
#include "../src/steam_integration/install_generation.hpp"
#include <fstream>
#include <iterator>
#endif
using namespace playden;
using namespace playden::steam;
static std::atomic<bool> fail_record{false}, fail_directory{false}, fail_content{false}, content_synced{false}, record_synced{false}, deep_synced{false}, parent_synced{false};
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  char descriptor[64], observed[512]{}; std::snprintf(descriptor, sizeof(descriptor), "/proc/self/fd/%d", fd);
  const auto path_length = readlink(descriptor, observed, sizeof(observed) - 1);
  if (path_length > 0 && std::string(observed).ends_with("/dir/deep")) deep_synced = true;
  if (path_length > 0 && std::string(observed).ends_with("/dir")) parent_synced = true;
  if (readlink(descriptor, observed, sizeof(observed) - 1) > 0 && std::strstr(observed, "/game.bin")) {
    content_synced = true;
    if (fail_content.exchange(false)) { errno = EIO; return -1; }
  }
  struct stat info{};
  if (fail_directory.load() && record_synced.load() && fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) && fail_directory.exchange(false)) {
    errno = EIO; return -1;
  }
  if (fail_record.load()) {
    char link[64], path[512]{}; std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    if (readlink(link, path, sizeof(path) - 1) > 0 && std::strstr(path, ".playden-install.json") && fail_record.exchange(false)) {
      errno = EIO; return -1;
    }
  }
  const int result = __real_fsync(fd);
  if (result == 0 && std::strstr(observed, ".playden-install.json")) record_synced = true;
  return result;
}
namespace playden::steam {
ChunkError ProcessDepotChunk(const std::vector<std::uint8_t>&, const std::array<std::uint8_t,32>&,
                             std::size_t size, std::uint32_t, const std::array<std::uint8_t,20>&,
                             std::vector<std::uint8_t>* out) { out->assign(size, 0x41); return ChunkError::kNone; }
}
int main() {
  char base[] = "/tmp/wine11-winner-worker-XXXXXX"; assert(mkdtemp(base));
  DepotManifest manifest; manifest.depot_id = 441; manifest.gid = 11;
  ManifestFile file; file.path = "dir/deep/game.bin"; file.size = 1;
  ManifestChunk chunk; chunk.offset = 0; chunk.original_size = 1; chunk.compressed_size = 1;
  file.chunks.push_back(chunk); manifest.files.push_back(file);
  InstallerHooks hooks;
  hooks.access = [](const StoredSession&, std::uint32_t, const std::vector<DepotRequest>& requests, const std::atomic<bool>&) {
    DepotAccessResult out; out.error = CmError::kNone; out.grants.push_back({requests[0].depot_id, requests[0].manifest_gid, {}, 77}); return out;
  };
  hooks.servers = [](std::uint32_t, const std::atomic<bool>&, std::vector<ContentServer>* out) {
    *out = {{"a.example.invalid", "a.example.invalid", 1}}; return CdnError::kNone;
  };
  hooks.manifest = [&](const ContentServer&, const DepotGrant&, const std::atomic<bool>&, DepotManifest* out) {
    *out = manifest; return InstallError::kNone;
  };
  hooks.free_bytes = [](const std::string&) { return std::uint64_t{1} << 40; };
  hooks.chunk = [](const ContentServer&, std::uint32_t, const std::string&, std::size_t,
                   const std::atomic<bool>&, std::vector<std::uint8_t>* out) {
    out->assign(1, 0); return CdnError::kNone;
  };
  DepotInfo depot; depot.id = 441; depot.manifest_gid = 11;
  std::atomic<bool> cancel{false};
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
  {
    InstallTarget missing; missing.app_id = 440; missing.install_root = std::string(base) + "/missing"; missing.depots.push_back(depot);
    assert(std::filesystem::create_directory(missing.install_root));
    DepotInstaller installer(hooks);
    assert(installer.Run({}, missing, cancel).error == InstallError::kStorage);
    assert(!std::filesystem::exists(missing.install_root + "/.playden-install.json"));
  }
#endif
  for (int mode : {1, 2, 3, 0}) {
    const bool fail = mode != 0;
    InstallTarget target; target.app_id = 440;
    target.install_root = std::string(base) + (mode == 1 ? "/failure-file" : mode == 2 ? "/failure-directory" : mode == 3 ? "/failure-content" : "/success");
    target.depots.push_back(depot);
    assert(std::filesystem::create_directory(target.install_root));
    if (fail) {
      const auto old_record = target.install_root + "/.playden-install.json";
      const int fd = open(old_record.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
      const std::string stale = "{\"version\":1,\"app_id\":440,\"install_bytes\":777}\n";
      assert(write(fd, stale.data(), stale.size()) == static_cast<ssize_t>(stale.size()) && close(fd) == 0);
    }
    fail_record = mode == 1; fail_directory = mode == 2; fail_content = mode == 3; content_synced = false; record_synced = false; deep_synced = false; parent_synced = false;
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
    const std::string generation = mode == 1 ? "11111111-1111-4111-8111-111111111111" :
        mode == 2 ? "22222222-2222-4222-8222-222222222222" :
        mode == 3 ? "44444444-4444-4444-8444-444444444444" : "33333333-3333-4333-8333-333333333333";
    ww::steam_integration::install_generation::Scope scope(generation);
#endif
    DepotInstaller installer(hooks);
    const auto outcome = installer.Run({}, target, cancel);
    if (fail) {
      assert(outcome.error == InstallError::kStorage);
      assert(std::filesystem::exists(target.install_root + "/.playden-download"));
      assert(std::filesystem::exists(target.install_root + "/.playden-install.json"));
    } else {
      assert(outcome.error == InstallError::kNone);
      assert(content_synced.load() && deep_synced.load() && parent_synced.load());
      assert(!std::filesystem::exists(target.install_root + "/.playden-download"));
      assert(std::filesystem::exists(target.install_root + "/.playden-install.json"));
    }
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
    std::ifstream record(target.install_root + "/.playden-install.json");
    const std::string contents((std::istreambuf_iterator<char>(record)), std::istreambuf_iterator<char>());
    if (mode == 3) assert(contents.find("\"wine11_operation\"") == std::string::npos);
    else assert(contents.find("\"wine11_operation\":\"" + generation + "\"") != std::string::npos);
#endif
  }
  std::filesystem::remove_all(base);
}
