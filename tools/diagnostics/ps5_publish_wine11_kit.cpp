// One-time publication of the already verified, isolated Wine 11 test kit.
// Refuses existing kits or unsafe directory ownership; no overwrites or deletes.
#include "runtime/artifact_hash.h"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  constexpr const char* parent = "/data/wine11";
  constexpr const char* kits = "/data/wine11/kits";
  constexpr const char* source = "/data/wine11-kit-stage-0f803ada2eee/playden-kit";
  constexpr const char* target = "/data/wine11/kits/playden-kit";
  constexpr const char* digest = "1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc";
  std::puts("WINE11_KIT_PUBLISH start overwrite=none"); std::fflush(stdout);
  auto fail = [](const char* stage) { std::printf("WINE11_KIT_PUBLISH failed=%s errno=%d\n", stage, errno); std::fflush(stdout); return 1; };
  if (geteuid() != 0) return fail("not-root");
  struct stat before{}, after{}, info{};
  if (lstat(source, &before) || !S_ISDIR(before.st_mode) || before.st_uid != 0 || (before.st_mode & 07777) != 0755)
    return fail("source-directory");
  auto manifest = playden::runtime::HashArtifactFile(std::string(source) + "/manifest.json");
  if (!manifest || *manifest != digest) return fail("source-manifest");
  // This isolated payload has its own umask. Avoid chmod on a path that
  // might be replaced between creation and validation.
  umask(0);
  if (lstat(parent, &info) != 0) {
    if (errno != ENOENT || mkdir(parent, 0755) != 0) return fail("create-parent");
    if (lstat(parent, &info) != 0) return fail("new-parent-mode");
  }
  if (!S_ISDIR(info.st_mode) || info.st_uid != 0 || (info.st_mode & 07777) != 0755) return fail("parent-identity");
  // mkdir is exclusive: an existing kits folder is never taken over.
  if (mkdir(kits, 0755) != 0) return fail("create-kits");
  if (lstat(kits, &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != 0 ||
      (info.st_mode & 07777) != 0755) return fail("kits-identity");
  if (lstat(target, &info) == 0 || errno != ENOENT) return fail("target-exists");
  if (lstat(source, &after) != 0 || before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
      before.st_uid != after.st_uid || (after.st_mode & 07777) != 0755) return fail("source-changed");
  if (rename(source, target) != 0) return fail("rename");
  if (lstat(target, &after) != 0 || before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
      after.st_uid != 0 || (after.st_mode & 07777) != 0755) return fail("published-identity");
  std::puts("WINE11_KIT_PUBLISH done installed=1"); std::fflush(stdout);
  return 0;
}
