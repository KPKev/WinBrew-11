// Isolated host probe only; this does not establish PS5 syscall availability.
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char root[] = "/tmp/wine11-anchored-probe-XXXXXX";
  assert(mkdtemp(root));
  const int parent = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW); assert(parent >= 0);
  assert(mkdirat(parent, "440", 0700) == 0);
  int game = openat(parent, "440", O_RDONLY | O_DIRECTORY | O_NOFOLLOW); assert(game >= 0);
  struct stat saved{}, named{};
  assert(fstat(game, &saved) == 0 && fstatat(parent, "440", &named, AT_SYMLINK_NOFOLLOW) == 0);
  assert(saved.st_dev == named.st_dev && saved.st_ino == named.st_ino);
  assert(renameat(parent, "440", parent, "old") == 0);
  assert(mkdirat(parent, "440", 0700) == 0);
  assert(fstatat(parent, "440", &named, AT_SYMLINK_NOFOLLOW) == 0);
  assert(saved.st_dev != named.st_dev || saved.st_ino != named.st_ino);
  assert(unlinkat(parent, "440", AT_REMOVEDIR) == 0);
  assert(symlinkat("old", parent, "440") == 0);
  assert(openat(parent, "440", O_RDONLY | O_DIRECTORY | O_NOFOLLOW) < 0);
  assert(unlinkat(parent, "440", 0) == 0);
  assert(unlinkat(parent, "old", AT_REMOVEDIR) == 0);
  assert(close(game) == 0 && close(parent) == 0 && rmdir(root) == 0);
}
