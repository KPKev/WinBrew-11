// SPDX-License-Identifier: GPL-3.0-or-later
#include "vapor_title_remover.hpp"
#include "platform/ps5/title_remover.h"
#include "runtime/artifact_hash.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
namespace {
constexpr char kRequestDir[] = "/data/wine11/wine11-games/vapor-title-remover";
constexpr char kPayloadHash[] = "b9f421393039a3d2f3160d2b7e792883fb9aeff80d3540a9e589068500a1faf9";
bool ValidTitle(const std::string& title) {
  if (title.size() != 9 || title.compare(0, 6, "FAKE78")) return false;
  for (std::size_t i = 6; i < title.size(); ++i) if (title[i] < '0' || title[i] > '9') return false;
  return true;
}
bool ValidNonce(const std::string& nonce) {
  if (nonce.size() < 16 || nonce.size() > 64) return false;
  for (char c : nonce) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == '-')) return false;
  return true;
}
}
bool VerifyVaporTitleRemover(const std::string& path, std::string* reason) {
  const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) { if (reason) *reason = "Removal helper open failed (" + std::to_string(errno) + ")"; return false; }
  struct stat before{}, after{};
  bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_size > 4 && before.st_size <= 4 * 1024 * 1024;
  std::string bytes;
  if (ok) bytes.resize(static_cast<std::size_t>(before.st_size));
  std::size_t done = 0;
  while (ok && done < bytes.size()) {
    const auto n = read(fd, bytes.data() + done, bytes.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  ok = ok && fstat(fd, &after) == 0 && before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
       before.st_size == after.st_size && before.st_mtime == after.st_mtime && before.st_ctime == after.st_ctime;
  close(fd);
  if (!ok || bytes.compare(0, 4, "\x7f" "ELF") != 0) {
    if (reason) *reason = "Removal helper could not be read";
    return false;
  }
  const auto hash = playden::runtime::HashArtifactBytes(bytes);
  if (!hash || *hash != kPayloadHash) { if (reason) *reason = "Removal helper verification failed"; return false; }
  return true;
}
std::function<playden::homebrew::TitleRemoverResult(const std::string&, const std::string&)>
MakeVaporTitleRemover(const std::string& payload_path) {
  return [payload_path](const std::string& title, const std::string& nonce) {
    using playden::homebrew::TitleRemoverResult;
    if (!ValidTitle(title) || !ValidNonce(nonce) || payload_path != kVaporTitleRemoverPath)
      return TitleRemoverResult{false, std::nullopt, "Title removal request is invalid."};
    struct stat st{};
    for (const char* name : {"request", "request.tmp"}) {
      const std::string path = std::string(kRequestDir) + "/" + name;
      errno = 0;
      if (lstat(path.c_str(), &st) == 0 || errno != ENOENT)
        return TitleRemoverResult{false, std::nullopt, "A previous title removal needs inspection."};
    }
    playden::ps5::TitleRemoverConfig config;
    config.request_directory = kRequestDir; config.payload = payload_path;
    config.send = [](const std::string& elf, std::string* error) {
      const auto hash = playden::runtime::HashArtifactBytes(elf);
      if (!hash || *hash != kPayloadHash) { if (error) *error = "remover identity mismatch"; return false; }
      return playden::ps5::SendToPayloadLoader(elf, error);
    };
    const auto result = playden::ps5::RunTitleRemover(config, title, nonce);
    return TitleRemoverResult{result.sent, result.code, result.detail};
  };
}
}
