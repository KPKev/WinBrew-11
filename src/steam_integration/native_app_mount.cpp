// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_app_mount.hpp"
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
bool ResolvePackagedCa(int anchor, const char* const* roots, std::size_t count,
                       char* output, std::size_t capacity) {
  if (!output || !capacity) return false;
  output[0] = '\0';
  struct stat original{};
  if (anchor < 0 || !roots || fstat(anchor, &original) != 0 || !S_ISDIR(original.st_mode)) return false;
  for (std::size_t i = 0; i < count; ++i) {
    if (!roots[i] || roots[i][0] != '/') continue;
    int fd = open(roots[i], O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) continue;
    struct stat candidate{};
    const bool same = fstat(fd, &candidate) == 0 && S_ISDIR(candidate.st_mode) &&
        candidate.st_dev == original.st_dev && candidate.st_ino == original.st_ino;
    close(fd);
    if (!same) continue;
    int n = std::snprintf(output, capacity, "%s/assets/steam/cacert.pem", roots[i]);
    if (n < 0 || static_cast<std::size_t>(n) >= capacity) { output[0] = '\0'; return false; }
    return true;
  }
  return false;
}
}
