// SPDX-License-Identifier: GPL-3.0-or-later
// Run as root (it creates root-owned fixtures and hands them to uid 1).
#include "../src/ownership_adopt.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using ww::ownership::Stats;
static std::string base;
static std::string p(const char *rel) { return base + "/" + rel; }
static void mk(const char *rel, mode_t mode) { assert(mkdir(p(rel).c_str(), mode) == 0); assert(chmod(p(rel).c_str(), mode) == 0); }
static void file(const char *rel, mode_t mode, const char *text = "x") {
  int fd = open(p(rel).c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
  assert(fd >= 0 && write(fd, text, std::strlen(text)) == (ssize_t)std::strlen(text) && close(fd) == 0);
  assert(chmod(p(rel).c_str(), mode) == 0);
}
static struct stat st(const char *rel) { struct stat s{}; assert(lstat(p(rel).c_str(), &s) == 0); return s; }

int main() {
  if (geteuid() != 0) { std::puts("SKIP: needs root"); return 0; }
  char tmpl[] = "/tmp/wine11-adopt-XXXXXX";
  assert(mkdtemp(tmpl));
  base = tmpl;

  // A Vapor-published title as earlier builds left it: everything root-owned.
  mk("title", 0755); file("title/game-launch.json", 0600); mk("title/win", 0755);
  file("title/win/a.dll", 0644); mk("title/private", 0700); file("title/private/lock", 0600);
  assert(symlink("/etc/passwd", p("title/link").c_str()) == 0);
  Stats s;
  assert(ww::ownership::adopt_tree(p("title").c_str(), 1, s));
  assert(s.changed == 6 && s.symlinks == 1 && s.clean());
  for (auto rel : {"title", "title/game-launch.json", "title/win", "title/win/a.dll", "title/private", "title/private/lock"})
    assert(st(rel).st_uid == 1);
  assert((st("title/game-launch.json").st_mode & 07777) == 0600);   // modes kept
  assert((st("title/private").st_mode & 07777) == 0700);
  assert(st("title/link").st_uid == 0);                              // link untouched
  struct stat target{}; assert(stat("/etc/passwd", &target) == 0 && target.st_uid == 0);

  // Idempotent.
  Stats again;
  assert(ww::ownership::adopt_tree(p("title").c_str(), 1, again) && again.changed == 0 && again.already == 6);

  // Interrupted earlier run: top already handed over, contents still root's.
  mk("data", 0700); mk("data/.playden-control", 0700); file("data/.playden-control/publication.lock", 0600);
  assert(lchown(p("data").c_str(), 1, (gid_t)-1) == 0);
  Stats partial;
  assert(ww::ownership::adopt_tree(p("data").c_str(), 1, partial));
  assert(partial.changed == 2 && partial.already == 1);
  assert(st("data/.playden-control/publication.lock").st_uid == 1 && st("data").st_uid == 1);

  // Another user's entry is refused and left alone; the rest still converts.
  mk("mixed", 0755); file("mixed/theirs", 0644); file("mixed/ours", 0644);
  assert(lchown(p("mixed/theirs").c_str(), 1234, (gid_t)-1) == 0);
  Stats mixed;
  assert(!ww::ownership::adopt_tree(p("mixed").c_str(), 1, mixed));
  assert(mixed.refused == 1 && st("mixed/theirs").st_uid == 1234 && st("mixed/ours").st_uid == 1);

  // Missing roots: fine unless required. uid 0 target is rejected.
  Stats missing;
  assert(ww::ownership::adopt_tree(p("absent").c_str(), 1, missing) && missing.seen == 0);
  assert(!ww::ownership::adopt_tree(p("absent").c_str(), 1, missing, true));
  Stats zero;
  assert(!ww::ownership::adopt_tree(p("title").c_str(), 0, zero));

  // Entry limit is an error, not a silent partial success.
  mk("big", 0755); for (int i = 0; i < 5; ++i) { char n[32]; std::snprintf(n, sizeof n, "big/f%d", i); file(n, 0644); }
  Stats limited;
  assert(!ww::ownership::adopt_tree(p("big").c_str(), 1, limited, false, false, 3) && limited.errors > 0);

  // Dry run: counts what would change and changes nothing.
  mk("preview", 0755); file("preview/a", 0644); mk("preview/sub", 0700); file("preview/sub/b", 0600);
  Stats plan;
  assert(ww::ownership::adopt_tree(p("preview").c_str(), 1, plan, false, true));
  assert(plan.changed == 4 && plan.clean());
  for (auto rel : {"preview", "preview/a", "preview/sub", "preview/sub/b"}) assert(st(rel).st_uid == 0);
  // A foreign owner makes the plan unclean, still without changing anything.
  file("preview/theirs", 0644); assert(lchown(p("preview/theirs").c_str(), 999, (gid_t)-1) == 0);
  Stats plan2;
  assert(!ww::ownership::adopt_tree(p("preview").c_str(), 1, plan2, false, true) && plan2.refused == 1);
  assert(st("preview/a").st_uid == 0);

  // Private mode restoration.
  mk("games", 0711);
  assert(ww::ownership::restore_private_mode(p("games").c_str()) && (st("games").st_mode & 07777) == 0700);
  assert(ww::ownership::restore_private_mode(p("games").c_str()));
  mk("public", 0755);
  assert(!ww::ownership::restore_private_mode(p("public").c_str()) && (st("public").st_mode & 07777) == 0755);
  assert(ww::ownership::restore_private_mode(p("nope").c_str()));

  // Title allow-list parsing.
  char ids[8][10];
  file("list-ok", 0600, "FAKE78004\nFAKE78005\n");
  assert(ww::ownership::read_title_list(p("list-ok").c_str(), ids, 8) == 2);
  assert(!std::strcmp(ids[0], "FAKE78004") && !std::strcmp(ids[1], "FAKE78005"));
  file("list-empty", 0600, "");
  assert(ww::ownership::read_title_list(p("list-empty").c_str(), ids, 8) == 0);
  file("list-bad", 0600, "FAKE78004\n../etc\n");
  assert(ww::ownership::read_title_list(p("list-bad").c_str(), ids, 8) == -1);
  file("list-noeol", 0600, "FAKE78004");
  assert(ww::ownership::read_title_list(p("list-noeol").c_str(), ids, 8) == -1);
  assert(ww::ownership::read_title_list(p("list-missing").c_str(), ids, 8) == 0);
  file("list-many", 0600, "AAAA00001\nAAAA00002\nAAAA00003\n");
  assert(ww::ownership::read_title_list(p("list-many").c_str(), ids, 2) == -1);

  std::string cleanup = "rm -rf '" + base + "'";
  assert(std::system(cleanup.c_str()) == 0);
  std::puts("ownership adopt tests passed");
}
