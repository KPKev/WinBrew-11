/* Read-only staging identity probe. It never elevates or changes permissions. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  struct stat kit, wine;
  int kit_ok = lstat("/data/wine11-kit-stage-0f803ada2eee/playden-kit", &kit) == 0 && S_ISDIR(kit.st_mode);
  int wine_ok = lstat("/data/wine11", &wine) == 0 && S_ISDIR(wine.st_mode);
  printf("STAGING_IDENTITY euid_is_root=%d kit_present=%d kit_root_owned=%d kit_mode=%03o wine_exists=%d\n",
         geteuid() == 0, kit_ok, kit_ok && kit.st_uid == 0,
         kit_ok ? (unsigned)(kit.st_mode & 0777) : 0, wine_ok);
  puts("STAGING_IDENTITY done");
  fflush(stdout);
  return 0;
}
