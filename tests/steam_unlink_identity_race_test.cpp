// Host-only characterization of POSIX unlink semantics. This is not a remover.
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char root[] = "/tmp/wine11-unlink-race-XXXXXX"; assert(mkdtemp(root));
  const int parent = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW); assert(parent >= 0);
  assert(mkdirat(parent, "game", 0700) == 0 && mkdirat(parent, "outside", 0700) == 0);
  const int game = openat(parent, "game", O_RDONLY | O_DIRECTORY | O_NOFOLLOW); assert(game >= 0);
  const int outside = openat(parent, "outside", O_RDONLY | O_DIRECTORY | O_NOFOLLOW); assert(outside >= 0);
  int fd = openat(outside, "sentinel", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0);
  assert(write(fd, "safe", 4) == 4 && close(fd) == 0);
  assert(symlinkat("../outside", game, "link") == 0);
  // A traversal that opens each component with O_NOFOLLOW refuses this link.
  assert(openat(game, "link", O_RDONLY | O_DIRECTORY | O_NOFOLLOW) < 0);
  struct stat external{}; assert(fstatat(outside, "sentinel", &external, AT_SYMLINK_NOFOLLOW) == 0);

  fd = openat(game, "victim", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0 && close(fd) == 0);
  fd = openat(game, "replacement", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0 && close(fd) == 0);
  struct stat inspected{}, substituted{};
  assert(fstatat(game, "victim", &inspected, AT_SYMLINK_NOFOLLOW) == 0);
  // Deterministically model an uncooperative rename between inspection and unlink.
  assert(renameat(game, "replacement", game, "victim") == 0);
  assert(fstatat(game, "victim", &substituted, AT_SYMLINK_NOFOLLOW) == 0);
  assert(inspected.st_dev != substituted.st_dev || inspected.st_ino != substituted.st_ino);
  // unlinkat has no inode argument: it deletes the replacement, not the inspected file.
  assert(unlinkat(game, "victim", 0) == 0);
  assert(fstatat(outside, "sentinel", &external, AT_SYMLINK_NOFOLLOW) == 0);
  fd = openat(game, "partial", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0 && close(fd) == 0);
  struct stat record{};
  assert(fstatat(game, ".playden-install.json", &record, AT_SYMLINK_NOFOLLOW) < 0 && errno == ENOENT);
  // A writer can publish an install record after the absence check, before unlink.
  fd = openat(game, ".playden-install.json", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0 && close(fd) == 0);
  assert(unlinkat(game, "partial", 0) == 0);
  assert(fstatat(game, ".playden-install.json", &record, AT_SYMLINK_NOFOLLOW) == 0);
  assert(close(game) == 0 && close(outside) == 0 && close(parent) == 0);
  std::filesystem::remove_all(root);
}
