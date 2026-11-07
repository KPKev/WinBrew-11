// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/dedicated_title_list.hpp"
extern "C" {
#include "title_rule.h"
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {
  using namespace playden::platform;
  using namespace ww::steam_integration;
  char path[] = "/tmp/wine11-rule-XXXXXX"; assert(mkdtemp(path));
  const auto root = std::filesystem::path(path) / "wine11";
  const auto dir = root / "title-rule";
  assert(std::filesystem::create_directory(root) && chmod(root.c_str(), 0700) == 0);
  assert(std::filesystem::create_directory(dir) && chmod(dir.c_str(), 0700) == 0);
  const auto lock_path = dir / "wine11-title.lock";
  int fd = open(lock_path.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0600); assert(fd >= 0); close(fd);
  auto directory = ManagedDirectory::Open(dir, DirectoryPolicy::kPrivate); assert(directory);
  auto titles = DedicatedTitleList::Open(std::move(*directory)); assert(titles);
  auto other = ManagedDirectory::Open(dir, DirectoryPolicy::kPrivate); assert(other);
  assert(!DedicatedTitleList::Open(std::move(*other)));
  auto write = MakeTitleListWriter(*titles); std::string error;
  assert(write("FAKE78099\n", &error));
  assert(lapy_title_rule_check("FAKE78099", "FAKE78", (dir/"allow-list").c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  assert(write("FAKE78003\n", &error));
  assert(lapy_title_rule_check("FAKE78003", "FAKE78", (dir/"allow-list").c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  assert(lapy_title_rule_check("FAKE78099", "FAKE78", (dir/"allow-list").c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  assert(titles->ReplaceIfUnchanged("stale", "FAKE78001\n") == TitleListReplace::changed);
  assert(rename(lock_path.c_str(), (dir/"old.lock").c_str()) == 0);
  fd = open(lock_path.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0600); assert(fd >= 0); close(fd);
  assert(!write("FAKE78001\n", &error));
  titles.reset();
  assert(unlink((dir/"allow-list").c_str()) == 0);
  assert(symlink("/etc/passwd", (dir/"allow-list").c_str()) == 0);
  auto reopened = ManagedDirectory::Open(dir, DirectoryPolicy::kPrivate); assert(reopened);
  auto guarded = DedicatedTitleList::Open(std::move(*reopened)); assert(guarded);
  assert(!guarded->Read());
  guarded.reset();
  const auto data_path = std::filesystem::path(path) / "data";
  assert(std::filesystem::create_directory(data_path));
  if (geteuid() == 0) assert(chmod(path, 0755) == 0 && chmod(data_path.c_str(), 0755) == 0);
  auto data = ManagedDirectory::Open(data_path, DirectoryPolicy::kApprovedTree); assert(data);
  int data_anchor = open(data_path.c_str(), O_RDONLY|O_DIRECTORY); assert(data_anchor >= 0);
  int wrong_anchor = open(path, O_RDONLY|O_DIRECTORY); assert(wrong_anchor >= 0);
  assert(!ProvisionDedicatedTitleList(*data, wrong_anchor)); close(wrong_anchor);
  auto provisioned = ProvisionDedicatedTitleList(*data, data_anchor); assert(provisioned);
  struct stat parent_stat{}, rule_stat{};
  assert(stat((data_path/"wine11").c_str(), &parent_stat) == 0 && (parent_stat.st_mode & 07777) == 0755);
  assert(stat((data_path/"wine11/title-rule").c_str(), &rule_stat) == 0 && (rule_stat.st_mode & 07777) == 0700);
  // Exercise a separate unprivileged identity when run as root: the app must
  // open every kit ancestor, but must not be able to read the title rule.
  if (geteuid() == 0) {
    const auto kit = data_path / "wine11/kits/playden-kit";
    assert(std::filesystem::create_directories(kit));
    assert(chmod(kit.parent_path().c_str(), 0755) == 0 && chmod(kit.c_str(), 0755) == 0);
    pid_t child = fork(); assert(child >= 0);
    if (!child) {
      if (setgid(65534) != 0 || setuid(65534) != 0) _exit(2);
      auto readable = ManagedDirectory::Open(kit, DirectoryPolicy::kApprovedTree);
      auto private_rule = ManagedDirectory::Open(data_path/"wine11/title-rule", DirectoryPolicy::kApprovedTree);
      _exit(readable && !private_rule ? 0 : 1);
    }
    int status = 0; assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
  auto write_owned = MakeTitleListWriter(*provisioned); assert(write_owned("FAKE78020\n", &error));
  const auto owned_list = data_path/"wine11/title-rule/allow-list";
  assert(lapy_title_rule_check("FAKE78020", "FAKE78", owned_list.c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  provisioned.reset();
  assert(chmod((data_path/"wine11").c_str(), 0700) == 0);
  assert(!ProvisionDedicatedTitleList(*data, data_anchor));
  assert(stat((data_path/"wine11").c_str(), &parent_stat) == 0 && (parent_stat.st_mode & 07777) == 0700);
  assert(chmod((data_path/"wine11").c_str(), 0711) == 0);
  assert(!ProvisionDedicatedTitleList(*data, data_anchor));
  assert(stat((data_path/"wine11").c_str(), &parent_stat) == 0 && (parent_stat.st_mode & 07777) == 0711);
  assert(chmod((data_path/"wine11").c_str(), 0777) == 0);
  assert(!ProvisionDedicatedTitleList(*data, data_anchor));
  assert(stat((data_path/"wine11").c_str(), &parent_stat) == 0 && (parent_stat.st_mode & 07777) == 0777);
  close(data_anchor);
  std::filesystem::remove_all(path);
  std::puts("PASS: dedicated title list preserves entries, excludes other writers and detects a replaced lease");
}
