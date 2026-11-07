/* Read-only title-rule ownership observation. No content, credentials or paths
 * are emitted; no elevation or process interaction occurs. */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef WINE11_RULE_DIR
#define WINE11_RULE_DIR "/data/wine11/title-rule"
#endif
#ifndef WINE11_RULE_LIST
#define WINE11_RULE_LIST "/data/wine11/title-rule/allow-list"
#endif
int main(void) {
  struct stat dir_before, dir_after, named, opened, named_after;
  int flags = O_RDONLY | O_NOFOLLOW | O_NONBLOCK;
  int dir_ok = lstat(WINE11_RULE_DIR, &dir_before) == 0 && S_ISDIR(dir_before.st_mode);
  int list_ok = lstat(WINE11_RULE_LIST, &named) == 0 && S_ISREG(named.st_mode);
  int fd = -1, stable = 0;
  if (dir_ok && list_ok) fd = open(WINE11_RULE_LIST, flags);
  if (fd >= 0) {
    stable = fstat(fd, &opened) == 0 && lstat(WINE11_RULE_DIR, &dir_after) == 0 &&
      lstat(WINE11_RULE_LIST, &named_after) == 0 &&
      dir_before.st_dev == dir_after.st_dev && dir_before.st_ino == dir_after.st_ino &&
      dir_before.st_uid == dir_after.st_uid && dir_before.st_mode == dir_after.st_mode &&
      named.st_dev == opened.st_dev && named.st_ino == opened.st_ino &&
      named.st_uid == opened.st_uid && named.st_mode == opened.st_mode && named.st_nlink == opened.st_nlink &&
      opened.st_dev == named_after.st_dev && opened.st_ino == named_after.st_ino &&
      opened.st_uid == named_after.st_uid && opened.st_mode == named_after.st_mode && opened.st_nlink == named_after.st_nlink &&
      S_ISREG(opened.st_mode) && S_ISREG(named_after.st_mode);
    close(fd);
  }
  printf("WINE11_RULE_OWNER dir_present=%d dir_root=%d dir_private=%d list_present=%d list_root=%d list_private=%d list_single_link=%d stable=%d\n",
    dir_ok, dir_ok && dir_before.st_uid == 0, dir_ok && !(dir_before.st_mode & 0022),
    list_ok, stable && opened.st_uid == 0, stable && !(opened.st_mode & 0022),
    stable && opened.st_nlink == 1, stable);
  return 0;
}
