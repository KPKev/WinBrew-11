/* Read-only filesystem measurements for the test title's game storage. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
  const char *paths[] = {"/mnt/sandbox/PPSA77382_000/download0/wine11-steam/wine11-games", "/data"};
  puts("RAW_VOLUME_PROBE start writes=none");
  for (int i = 0; i < 2; ++i) {
    struct statfs info = {0};
    int fd = open(paths[i], O_RDONLY | O_DIRECTORY);
    int result = fd < 0 ? -1 : (int)syscall(SYS_fstatfs, fd, &info);
    int error = result ? errno : 0;
    if (fd >= 0) close(fd);
    printf("RAW_VOLUME_PROBE volume=%s ok=%d errno=%d block=%llu available=%lld\n", i ? "data" : "game-storage", result == 0, error,
           (unsigned long long)info.f_bsize, (long long)info.f_bavail);
    fflush(stdout);
  }
  puts("RAW_VOLUME_PROBE done"); fflush(stdout);
  return 0;
}
