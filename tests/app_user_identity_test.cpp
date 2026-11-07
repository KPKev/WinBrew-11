// SPDX-License-Identifier: GPL-3.0-or-later
// Run as root: builds a fake /data and private directory, then checks that
// Vapor's startup hands earlier root-created files to the app user and lowers
// its effective UID, or stays root when that cannot be done completely.
#include "../src/steam_integration/app_user_identity.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace ww::steam_integration;
static std::string base;
static std::string p(const std::string &rel) { return base + "/" + rel; }
static void mk(const std::string &rel, mode_t mode) { assert(mkdir(p(rel).c_str(), mode) == 0 && chmod(p(rel).c_str(), mode) == 0); }
static void file(const std::string &rel, mode_t mode, const char *text = "x") {
  int fd = open(p(rel).c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  assert(fd >= 0 && write(fd, text, strlen(text)) == (ssize_t)strlen(text) && close(fd) == 0 && chmod(p(rel).c_str(), mode) == 0);
}
static uid_t owner(const std::string &rel) { struct stat s{}; assert(lstat(p(rel).c_str(), &s) == 0); return s.st_uid; }
static unsigned mode(const std::string &rel) { struct stat s{}; assert(lstat(p(rel).c_str(), &s) == 0); return s.st_mode & 07777; }
static void back_to_root() { assert(seteuid(0) == 0 && geteuid() == 0); }

static void build_layout() {
  mk("data", 0755); mk("data/wine11", 0755); mk("data/wine11/kits", 0755);
  mk("data/wine11/title-rule", 0700); file("data/wine11/title-rule/allow-list", 0600, "FAKE78004\nFAKE78005\n");
  mk("data/wine11/wine11-games", 0711);  // the earlier stopgap
  file("data/wine11/wine11-games/wine11-install.lock", 0600);
  mk("data/wine11/wine11-games/489830", 0777); file("data/wine11/wine11-games/489830/Game.exe", 0666);
  mk("data/playden", 0755); mk("data/playden/games", 0777);
  mk("data/playden/games/wine11-steam-489830", 0700); mk("data/playden/games/wine11-steam-489830/.playden-control", 0700);
  file("data/playden/games/wine11-steam-489830/.playden-control/publication.lock", 0600);
  mk("data/playden/games/steam-1089130", 0700);  // Playden's own game: not Vapor's, must stay as is
  assert(lchown(p("data/playden/games/steam-1089130").c_str(), 1, (gid_t)-1) == 0);
  mk("data/homebrew", 0777);
  for (auto id : {"FAKE78004", "FAKE78005"}) {
    mk(std::string("data/homebrew/") + id, 0755);
    file(std::string("data/homebrew/") + id + "/game-launch.json", 0600);
  }
  mk("data/homebrew/FAKE78003", 0755);  // Quake, Playden-published, uid 1 already
  assert(lchown(p("data/homebrew/FAKE78003").c_str(), 1, (gid_t)-1) == 0);
  mk("data/homebrew/PPSA77381", 0777);  // the Wine 11 app itself: not in the list, untouched
  mk("private", 0755); mk("private/wine11-steam", 0700); file("private/wine11-steam/session.v1", 0600);
  file("private/desktop-settings.ini", 0644);
}

int main() {
  if (getuid() != 0 || geteuid() != 0) { std::puts("SKIP: needs real root"); return 0; }
  char tmpl[] = "/tmp/wine11-appuser-XXXXXX";
  assert(mkdtemp(tmpl));
  base = tmpl;
  assert(chmod(base.c_str(), 0755) == 0);
  build_layout();
  const std::string priv = p("private"), data = p("data");
  AppUserPaths paths{priv.c_str(), data.c_str()};

  // No grant, or a title that started as root: nothing changes.
  assert(AdoptAndLower(1, false, paths).stage == AppUserStage::not_elevated);
  assert(AdoptAndLower(0, true, paths).stage == AppUserStage::original_was_root);
  assert(owner("data/homebrew/FAKE78005/game-launch.json") == 0 && geteuid() == 0);

  // A file owned by someone else blocks completion: stay root, no marker, nothing lost.
  file("private/foreign", 0600);
  assert(lchown(p("private/foreign").c_str(), 4321, (gid_t)-1) == 0);
  auto blocked = AdoptAndLower(1, true, paths);
  assert(blocked.stage == AppUserStage::adopt_incomplete && geteuid() == 0 && blocked.stats.refused == 1);
  // The plan found the foreign file first, so nothing at all was changed: no mixed owners.
  assert(owner("private/desktop-settings.ini") == 0 && owner("data/homebrew/FAKE78005/game-launch.json") == 0);
  assert(owner("data/wine11/wine11-games") == 0 && mode("data/wine11/wine11-games") == 0711);
  assert(access(p("private/.wine11-app-user-v1").c_str(), F_OK) != 0);
  assert(unlink(p("private/foreign").c_str()) == 0);

  // The real conversion.
  auto done = AdoptAndLower(1, true, paths);
  assert(done.stage == AppUserStage::lowered && done.adopted_now && done.stats.clean());
  assert(geteuid() == 1 && getuid() == 0);
  back_to_root();
  for (auto rel : {"private", "private/wine11-steam", "private/wine11-steam/session.v1", "private/desktop-settings.ini",
                   "data/wine11/wine11-games", "data/wine11/wine11-games/wine11-install.lock",
                   "data/wine11/wine11-games/489830/Game.exe",
                   "data/playden/games/wine11-steam-489830/.playden-control/publication.lock",
                   "data/homebrew/FAKE78004/game-launch.json", "data/homebrew/FAKE78005"})
    assert(owner(rel) == 1);
  assert(mode("data/wine11/wine11-games") == 0700);                       // stopgap undone
  assert(mode("data/homebrew/FAKE78005/game-launch.json") == 0600);
  assert(owner("data/wine11") == 0 && owner("data/wine11/kits") == 0);     // stays root, like Playden's kits
  assert(owner("data/wine11/title-rule") == 0 && owner("data/wine11/title-rule/allow-list") == 0);
  assert(owner("data/homebrew/PPSA77381") == 0);                          // not a Vapor game title
  assert(owner("data/playden/games") == 0);                               // shared parent untouched
  assert(owner("private/.wine11-app-user-v1") == 1);

  // Next start: marker present, so no walk; it just lowers.
  file("data/homebrew/FAKE78005/late-root-file", 0600);
  auto next = AdoptAndLower(1, true, paths);
  assert(next.stage == AppUserStage::lowered && !next.adopted_now && next.stats.seen == 0 && geteuid() == 1);
  back_to_root();
  assert(owner("data/homebrew/FAKE78005/late-root-file") == 0);

  std::string cleanup = "rm -rf '" + base + "'";
  assert(std::system(cleanup.c_str()) == 0);
  std::puts("app user identity tests passed");
}
