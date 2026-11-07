/* Read-only, isolated payload: test whether /data can be opened before elevation.
 * This is a payload-context observation, not proof of the app sandbox's access. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  struct stat info;
  puts("DATA_ANCHOR_PROBE start writes=none elevation=none");
  fflush(stdout);
  int fd = open("/data", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
  int open_error = fd < 0 ? errno : 0;
  int is_directory = fd >= 0 && fstat(fd, &info) == 0 && S_ISDIR(info.st_mode);
  if (fd >= 0) close(fd);
  printf("DATA_ANCHOR_PROBE open_ok=%d errno=%d is_directory=%d\n", fd >= 0, open_error, is_directory);
  puts("DATA_ANCHOR_PROBE done");
  fflush(stdout);
  return 0;
}
