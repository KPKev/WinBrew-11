#include "steam/backend/depot_installer.h"
#include "steam/backend/depot_chunk.h"
#include <atomic>
#include <cassert>
#include <filesystem>
#include <unistd.h>
using namespace playden::steam;
using playden::StoredSession;
namespace playden::steam {
ChunkError ProcessDepotChunk(const std::vector<std::uint8_t>&, const std::array<std::uint8_t,32>&,
 std::size_t, std::uint32_t, const std::array<std::uint8_t,20>&, std::vector<std::uint8_t>*) { return ChunkError::kNone; }
}
int main() {
  char base[] = "/tmp/vapor-arch-XXXXXX"; assert(mkdtemp(base));
  DepotInfo x64; x64.id = 1; x64.os_arch = "64"; x64.manifest_gid = 11;
  DepotInfo x86; x86.id = 2; x86.os_arch = "32"; x86.manifest_gid = 22;
  InstallerHooks hooks;
  hooks.access = [](const StoredSession&, std::uint32_t, const std::vector<DepotRequest>& requests, const std::atomic<bool>&) {
    DepotAccessResult r; r.error = CmError::kNone;
    for (const auto& d : requests) r.grants.push_back({d.depot_id, d.manifest_gid, {}, 77}); return r;
  };
  hooks.servers = [](std::uint32_t, const std::atomic<bool>&, std::vector<ContentServer>* out) {
    *out = {{"test.invalid", "test.invalid", 1}}; return CdnError::kNone;
  };
  bool conflict = true;
  hooks.manifest = [&](const ContentServer&, const DepotGrant& grant, const std::atomic<bool>&, DepotManifest* out) {
    out->depot_id = grant.depot_id; out->gid = grant.manifest_gid;
    ManifestFile f; f.path = "bin/game.exe"; f.size = 0;
    if (grant.depot_id == 2 && conflict) f.sha_content[0] = 1;
    out->files.push_back(f); return InstallError::kNone;
  };
  hooks.free_bytes = [](const std::string&) { return std::uint64_t{1} << 40; };
  hooks.chunk = [](const ContentServer&, std::uint32_t, const std::string&, std::size_t, const std::atomic<bool>&, std::vector<std::uint8_t>*) { return CdnError::kNone; };
  InstallTarget target; target.app_id = 44; target.install_root = base; target.depots = {x64, x86};
  std::atomic<bool> cancel{false}; DepotInstaller installer(hooks);
  auto result = installer.Run({}, target, cancel);
  assert(result.error == InstallError::kProtocol);
  assert(result.detail.find("Conflicting 32-bit and 64-bit") != std::string::npos);
  assert(!std::filesystem::exists(std::string(base) + "/bin/game.exe"));
  conflict = false;
  result = installer.Run({}, target, cancel);
  assert(result.error == InstallError::kNone);
  assert(std::filesystem::exists(std::string(base) + "/bin/game.exe"));
  std::filesystem::remove_all(base);
}
