// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_trust.hpp"
#ifndef MBEDTLS_CONFIG_FILE
#define MBEDTLS_CONFIG_FILE "pw_publication_config.h"
#endif
#include "../owner_admission/src/pw_publication_sha256.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
bool VerifyPackagedSteamCa(const char* path) {
  static constexpr unsigned char expected[32] = {
    0xa4,0x1b,0x5d,0x35,0x6a,0xea,0x97,0xa5,0x29,0xfe,0x27,0xe0,0xf7,0x31,0x6d,0x2f,
    0x9d,0x94,0x6d,0x75,0x92,0x74,0x76,0xcf,0x9c,0xf1,0xb9,0x06,0x37,0xd0,0x05,0x05
  };
  if (!path || !*path) return false;
  struct stat before{}, opened{}, after{};
  if (lstat(path, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size != 188900) return false;
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) return false;
  bool ok = fstat(fd, &opened) == 0 && S_ISREG(opened.st_mode) && opened.st_dev == before.st_dev &&
      opened.st_ino == before.st_ino && opened.st_size == before.st_size;
  PwPublicationSha256 hash{};
  bool initialized = ok && pw_publication_sha256_init(&hash) == 0;
  ok = initialized;
  unsigned char bytes[8192], digest[32]{};
  std::size_t total = 0;
  while (ok) {
    ssize_t n = read(fd, bytes, sizeof(bytes));
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) { ok = false; break; }
    if (n == 0) break;
    total += static_cast<std::size_t>(n);
    if (total > 188900 || pw_publication_sha256_update(&hash, bytes, static_cast<std::size_t>(n)) != 0) ok = false;
  }
  if (ok) ok = total == 188900 && pw_publication_sha256_finish(&hash, digest) == 0 &&
      std::memcmp(digest, expected, sizeof(expected)) == 0;
  if (initialized) pw_publication_sha256_clear(&hash);
  if (fstat(fd, &after) != 0 || after.st_dev != opened.st_dev || after.st_ino != opened.st_ino ||
      after.st_size != opened.st_size) ok = false;
  if (close(fd) != 0) ok = false;
  return ok;
}
}
