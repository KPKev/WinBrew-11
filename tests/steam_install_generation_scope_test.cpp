#include "../src/steam_integration/install_generation.hpp"
#include <cassert>
#include <thread>
using namespace ww::steam_integration::install_generation;
int main() {
  const std::string first = "11111111-1111-4111-8111-111111111111";
  const std::string second = "22222222-2222-4222-8222-222222222222";
  assert(Valid(first) && !Valid("not-an-id") && Current().empty());
  {
    Scope outer(first); assert(Current() == first);
    try { Scope inner(second); assert(Current() == second); throw 1; } catch (...) {}
    assert(Current() == first);
    std::thread other([&] { assert(Current().empty()); Scope scoped(second); assert(Current() == second); });
    other.join(); assert(Current() == first);
  }
  assert(Current().empty());
}
