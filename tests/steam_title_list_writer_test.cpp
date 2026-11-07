// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/title_list_writer.hpp"
#include <cassert>
#include <cstdio>
using namespace ww::steam_integration;
struct List : CoordinatedTitleList {
  std::string value = "FAKE78003\n";
  bool available = true, concurrent = false; int writes = 0;
  std::optional<std::string> Read() override { return available ? std::optional(value) : std::nullopt; }
  TitleListReplace ReplaceIfUnchanged(const std::string& expected, const std::string& replacement) override {
    ++writes;
    if (concurrent) value += "FAKE78004\n";
    if (value != expected) return TitleListReplace::changed;
    value = replacement; return TitleListReplace::written;
  }
};
int main() {
  List list; auto write = MakeTitleListWriter(list); std::string error;
  assert(write("FAKE78099\n", &error) && list.value == "FAKE78003\nFAKE78099\n");
  assert(write("FAKE78099\n", &error) && list.writes == 2);
  list.concurrent = true;
  assert(!write("FAKE78099\n", &error) && list.writes == 3);
  list.value = "FAKE78003\nFAKE78099\n";
  list.concurrent = true;
  assert(!write("FAKE78098\n", &error) && list.value == "FAKE78003\nFAKE78099\nFAKE78004\n");
  assert(list.writes == 4 && error.find("changed") != std::string::npos);
  list.concurrent = false; list.value = "FAKE78003";
  assert(!write("FAKE78099\n", &error) && list.writes == 4);
  list.available = false;
  assert(!write("FAKE78099\n", &error) && list.writes == 4);
  std::puts("PASS: title-list updates preserve existing entries and refuse concurrent or invalid changes");
}
