/* Read-only owner comparison; no account names or credentials are read. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
  struct stat info;
  int ok = lstat("/mnt/sandbox/PPSA77382_000/download0/wine11-steam/wine11-games", &info) == 0;
  printf("STEAM_STORAGE_OWNER ok=%d owner=%u group=%u root=%d\n", ok,
         ok ? (unsigned)info.st_uid : 0, ok ? (unsigned)info.st_gid : 0, ok && info.st_uid == 0);
  puts("STEAM_STORAGE_OWNER done"); fflush(stdout); return 0;
}
