// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/title_list_merge.hpp"
extern "C" {
#include "title_rule.h"
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
int main() {
  using ww::steam_integration::MergeTitleLists;
  auto merged = MergeTitleLists("FAKE78003\nFAKE78001\n", "FAKE78099\nFAKE78003\n");
  assert(merged && *merged == "FAKE78001\nFAKE78003\nFAKE78099\n");
  assert(!MergeTitleLists("FAKE78003", "FAKE78099\n"));
  assert(!MergeTitleLists("FAKE77031\n", "FAKE78099\n"));
  assert(!MergeTitleLists("FAKE78003\n", "FAKE78099\r\n"));
  assert(!MergeTitleLists(std::string(65536, 'x'), ""));
  char directory[] = "/tmp/wine11-title-list-XXXXXX"; assert(mkdtemp(directory));
  const auto file = std::filesystem::path(directory) / "allow-list";
  { std::ofstream out(file); out << *merged; assert(out.good()); }
  assert(chmod(file.c_str(), 0644) == 0);
  assert(lapy_title_rule_check("FAKE78003", "FAKE78", file.c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  assert(lapy_title_rule_check("FAKE78099", "FAKE78", file.c_str(), getuid()) == LAPY_TITLE_RULE_ALLOWED);
  assert(lapy_title_rule_check("FAKE78098", "FAKE78", file.c_str(), getuid()) == LAPY_TITLE_RULE_NOT_LISTED);
  std::filesystem::remove_all(directory);
  std::puts("PASS: merged titles preserve Playden and Wine 11 entries accepted by the actual helper rule");
}
