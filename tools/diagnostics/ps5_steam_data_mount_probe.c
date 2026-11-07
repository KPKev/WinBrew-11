/* Read-only comparison of the test app's sandbox /data and global /data. */
#include <stdio.h>
#include <sys/stat.h>

int main(void) {
  struct stat sandbox, global;
  int a = stat("/mnt/sandbox/PPSA77382_000/data", &sandbox) == 0 && S_ISDIR(sandbox.st_mode);
  int b = stat("/data", &global) == 0 && S_ISDIR(global.st_mode);
  int same = a && b && sandbox.st_dev == global.st_dev && sandbox.st_ino == global.st_ino;
  printf("DATA_MOUNT_PROBE sandbox_exists=%d global_exists=%d same_directory=%d\n", a, b, same);
  puts("DATA_MOUNT_PROBE done");
  fflush(stdout);
  return 0;
}
