#include "../src/notepad_search.hpp"
#include <cassert>
int main() {
  using namespace ww::notepad_search;
  assert(find_next("one two one", "one", 1) == 8);
  assert(find_next("one two one", "one", 9) == 0);
  assert(find_next("one", "", 0) == std::string::npos);
  std::string result = "unchanged"; size_t count = 42;
  assert(replace_all("a aa", "a", "bb", 7, result, count));
  assert(result == "bb bbbb" && count == 3);
  assert(!replace_all("a aa", "a", "bbb", 7, result, count));
  assert(result == "bb bbbb" && count == 3);
  assert(!replace_all("text", "", "x", 10, result, count));
}
