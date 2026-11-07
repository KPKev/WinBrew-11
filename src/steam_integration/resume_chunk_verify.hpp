// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "steam/backend/depot_manifest.h"
#include <mbedtls/sha1.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
enum class ResumeChunk { valid, repair, unavailable, canceled };
// Component checks reject observed links but are not atomic with open or repair.
// This experiment must remain disabled without an anchored write boundary.
inline ResumeChunk VerifyResumeChunk(const std::string& root, const std::string& relative,
    std::uint64_t file_size, const playden::steam::ManifestChunk& chunk, const std::atomic<bool>& cancel) {
  if (cancel.load()) return ResumeChunk::canceled;
  if (!chunk.original_size || chunk.offset > file_size || chunk.original_size > file_size - chunk.offset ||
      chunk.offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) return ResumeChunk::unavailable;
  std::string path = root;
  std::size_t start = 0;
  while (true) {
    if (cancel.load()) return ResumeChunk::canceled;
    const auto slash = relative.find('/', start);
    if (slash == std::string::npos) break;
    const auto component = relative.substr(start, slash - start);
    if (component.empty() || component == "." || component == "..") return ResumeChunk::unavailable;
    path += "/" + component;
    struct stat parent{};
    if (lstat(path.c_str(), &parent) != 0) return errno == ENOENT ? ResumeChunk::repair : ResumeChunk::unavailable;
    if (!S_ISDIR(parent.st_mode)) return ResumeChunk::unavailable;
    start = slash + 1;
  }
  const auto last = relative.substr(start);
  if (last.empty() || last == "." || last == "..") return ResumeChunk::unavailable;
  path += "/" + last;
  int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return errno == ENOENT ? ResumeChunk::repair : ResumeChunk::unavailable;
  struct stat st{};
  if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0) { close(fd); return ResumeChunk::unavailable; }
  if (static_cast<std::uint64_t>(st.st_size) < chunk.offset + chunk.original_size)
    return close(fd) == 0 ? ResumeChunk::repair : ResumeChunk::unavailable;
  if (lseek(fd, static_cast<off_t>(chunk.offset), SEEK_SET) < 0) { close(fd); return ResumeChunk::unavailable; }
  mbedtls_sha1_context sha; mbedtls_sha1_init(&sha);
  bool ok = chunk.crc || mbedtls_sha1_starts(&sha) == 0;
  std::uint32_t a = 0, b = 0; std::uint64_t left = chunk.original_size;
  std::array<unsigned char, 16384> buffer{};
  while (ok && left) {
    if (cancel.load()) { mbedtls_sha1_free(&sha); close(fd); return ResumeChunk::canceled; }
    auto n = read(fd, buffer.data(), static_cast<std::size_t>(std::min<std::uint64_t>(left, buffer.size())));
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    if (chunk.crc) for (ssize_t i = 0; i < n; ++i) { a = (a + buffer[i]) % 65521U; b = (b + a) % 65521U; }
    else if (mbedtls_sha1_update(&sha, buffer.data(), static_cast<std::size_t>(n)) != 0) ok = false;
    left -= static_cast<std::uint64_t>(n);
  }
  std::array<unsigned char, 20> digest{};
  if (ok && !chunk.crc && mbedtls_sha1_finish(&sha, digest.data()) != 0) ok = false;
  mbedtls_sha1_free(&sha);
  if (close(fd) != 0) ok = false;
  if (cancel.load()) return ResumeChunk::canceled;
  if (!ok) return ResumeChunk::unavailable;
  return (chunk.crc ? ((b << 16) | a) == chunk.crc : digest == chunk.sha) ? ResumeChunk::valid : ResumeChunk::repair;
}
}
