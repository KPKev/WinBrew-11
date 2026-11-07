// Host test of the opt-in generated downloader; uses isolated temporary files.
#include "steam/backend/depot_installer.h"
#include "steam/backend/depot_chunk.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
using namespace playden;
using namespace playden::steam;
static int injection = 0;
static std::atomic<bool>* injected_cancel = nullptr;
extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = static_cast<mode_t>(va_arg(args, int)); va_end(args); }
  if (injection == 8 && std::strstr(path, "/game.bin")) { errno = EACCES; return -1; }
  return __real_open(path, flags, mode);
}
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t count) {
  char link[64], path[512]{}; std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
  const auto len = readlink(link, path, sizeof(path) - 1);
  if (len > 0 && std::strstr(path, "/game.bin")) {
    if (injection == 7) { errno = EIO; return -1; }
    if (injection == 9 && injected_cancel) *injected_cancel = true;
  }
  return __real_read(fd, buffer, count);
}
namespace playden::steam {
ChunkError ProcessDepotChunk(const std::vector<std::uint8_t>&, const std::array<std::uint8_t,32>&,
    std::size_t size, std::uint32_t, const std::array<std::uint8_t,20>&, std::vector<std::uint8_t>* out) {
  out->assign(size, 'A'); return ChunkError::kNone;
}
}
int main() {
  char base[] = "/tmp/wine11-resume-verify-XXXXXX"; assert(mkdtemp(base));
  for (int mode = 0; mode < 11; ++mode) {
    auto root = std::filesystem::path(base) / std::to_string(mode);
    std::filesystem::create_directories(root / DepotInstaller::kJournalDir);
    const std::string rel = mode == 6 ? "link/game.bin" : "game.bin";
    { std::ofstream j(root / DepotInstaller::kJournalDir / "journal"); j << "#v2 441:11\n441\t0\t" << rel << "\n"; }
    if (mode == 5) std::filesystem::create_symlink(root / "missing", root / "game.bin");
    else if (mode == 6) { auto external = std::filesystem::path(base) / "external"; std::filesystem::create_directory(external); std::ofstream(external / "game.bin") << 'A'; std::filesystem::create_directory_symlink(external, root / "link"); }
    else if (mode != 3) { std::ofstream file(root / "game.bin", std::ios::binary); if (mode != 2) file << (mode == 0 || (mode >= 4 && mode != 10) ? 'A' : 'B'); }
    DepotManifest manifest; manifest.depot_id = 441; manifest.gid = 11;
    ManifestFile file; file.path = rel; file.size = 1;
    ManifestChunk chunk; chunk.offset = 0; chunk.original_size = 1; chunk.compressed_size = 1; chunk.crc = (65u << 16) | 65u;
    if (mode == 4) { chunk.crc = 0; chunk.sha = {0x6d,0xcd,0x4c,0xe2,0x3d,0x88,0xe2,0xee,0x95,0x68,0xba,0x54,0x6c,0x00,0x7c,0x63,0xd9,0x13,0x1c,0x1b}; }
    file.chunks.push_back(chunk); manifest.files.push_back(file);
    std::atomic<int> fetched{0}; InstallerHooks hooks;
    hooks.access=[](const StoredSession&, std::uint32_t, const std::vector<DepotRequest>& r, const std::atomic<bool>&) {
      DepotAccessResult out; out.error=CmError::kNone; out.grants.push_back({r[0].depot_id,r[0].manifest_gid,{},77}); return out; };
    hooks.servers=[](std::uint32_t,const std::atomic<bool>&,std::vector<ContentServer>* out) { *out={{"test.invalid","test.invalid",1}}; return CdnError::kNone; };
    hooks.manifest=[&](const ContentServer&,const DepotGrant&,const std::atomic<bool>&,DepotManifest* out) { *out=manifest; return InstallError::kNone; };
    hooks.free_bytes=[](const std::string&) { return std::uint64_t{1}<<40; };
    std::atomic<bool>* cancel_ptr = nullptr;
    hooks.chunk=[&](const ContentServer&,std::uint32_t,const std::string&,std::size_t,const std::atomic<bool>&,std::vector<std::uint8_t>* out) {
      ++fetched; if (mode == 10 && fetched == 1) { *cancel_ptr = true; return CdnError::kCanceled; }
      out->assign(1,0); return CdnError::kNone; };
    DepotInfo depot; depot.id=441; depot.manifest_gid=11;
    InstallTarget target; target.app_id=440; target.install_root=root.string(); target.depots.push_back(depot);
    DepotInstaller installer(hooks); std::atomic<bool> cancel{false}; cancel_ptr = &cancel;
    injection = mode; injected_cancel = &cancel;
    auto outcome = installer.Run({}, target, cancel);
    injection = 0; injected_cancel = nullptr;
    if (mode >= 5 && mode <= 9) { assert(outcome.error == (mode == 9 ? InstallError::kCanceled : InstallError::kStorage) && fetched == 0); continue; }
    if (mode == 10) {
      assert(outcome.error == InstallError::kCanceled && fetched == 1);
      { std::ifstream before(root / "game.bin"); char byte = 0; before.get(byte); assert(byte == 'B'); }
      cancel = false; outcome = installer.Run({}, target, cancel);
      assert(outcome.error == InstallError::kNone && fetched == 2);
    }
    assert(outcome.error == InstallError::kNone);
    assert(fetched == (mode == 10 ? 2 : mode == 0 || mode == 4 ? 0 : 1));
    std::ifstream data(root / "game.bin", std::ios::binary); char actual = 0; data.get(actual); assert(actual == 'A');
  }
  std::filesystem::remove_all(base);
}
