// Link/import probe only. Never run on the console without a separate review.
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  int parent = open(argv[1], O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
  if (parent < 0) return 3;
  struct stat named, opened;
#if !defined(PROBE_OPENAT_ONLY)
  if (fstatat(parent, argv[2], &named, AT_SYMLINK_NOFOLLOW) != 0) return 4;
#endif
  int fd = openat(parent, argv[2], O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
  if (fd < 0 || fstat(fd, &opened) != 0) return 5;
#if !defined(PROBE_OPENAT_ONLY)
  if (named.st_dev != opened.st_dev || named.st_ino != opened.st_ino) return 6;
#else
  if (fstat(parent, &named) != 0) return 6;
#endif
  DIR* dir = fdopendir(fd);
  if (!dir) return 7;
  while (readdir(dir)) {}
  return closedir(dir) || close(parent);
}
