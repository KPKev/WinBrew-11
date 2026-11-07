// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/network_traffic.hpp"
#include <cassert>
#include <limits>
#include <thread>
#include <vector>
using namespace ww::steam_integration::traffic;
void receive_from_other_translation_unit(std::uint64_t);
int main() {
  // Capture retains ownership after the install scope exits, as a worker does.
  std::weak_ptr<Counter> weak;
  std::shared_ptr<Counter> worker_owner;
  {
    auto owner = std::make_shared<Counter>(); weak = owner;
    Scope outer(owner); worker_owner = capture();
    try { Scope nested(std::make_shared<Counter>()); throw 1; } catch (int) { }
    assert(capture() == owner);
  }
  assert(!current && !weak.expired());
  std::thread retained([worker_owner] { Scope scope(worker_owner); receive_from_other_translation_unit(5); });
  retained.join(); assert(worker_owner->bytes.load() == 5);
  worker_owner.reset(); assert(weak.expired());
  Registry registry; Sampler sampler;
  auto first = registry.begin(440); assert(!registry.get(441));
  assert(!sampler.sample(first, 0)); // no observed payload
  { Scope s(first); received(1); }
  assert(!sampler.sample(first, 100)); // initial observation is a baseline
  std::vector<std::thread> workers;
  for (int i=0; i<8; ++i) workers.emplace_back([first] {
    Scope scope(first);
    for (int j=0; j<10000; ++j) receive_from_other_translation_unit(1); // includes retries and partial failures
  });
  for (auto& worker : workers) worker.join();
  assert(sampler.sample(first,1100).value() == 80000);
  assert(sampler.sample(first,2100).value() == 0); // observed stall
  received(500); // no scope: unrelated auth, cloud, or artwork ignored
  assert(sampler.sample(first,3100).value() == 0);
  auto second = registry.begin(441);
  registry.end(first); assert(registry.get(441) == second); // late cleanup cannot clear new attempt
  assert(!sampler.sample(second, 3200));
  { Scope scope(second); received(200); }
  assert(!sampler.sample(second, 3300));
  { Scope scope(second); received(100); }
  assert(sampler.sample(second,4300).value() == 100);
  registry.end(second); assert(!registry.get(441));
  assert(!sampler.sample(nullptr,4400));
  // Old worker scope remains memory-safe but cannot affect the new attempt.
  { Scope scope(first); received(50); }
  assert(!registry.get(440));
  // A backward clock and a new attempt never reuse an old rate.
  auto third = registry.begin(441);
  { Scope scope(third); received(1); }
  assert(!sampler.sample(third,5000));
  { Scope scope(third); received(600); }
  assert(sampler.sample(third,6000).value() == 600);
  assert(!sampler.sample(third,50));
  { Scope scope(third); received(200); }
  assert(sampler.sample(third,1050).value() == 200);
  // Saturation is monotonic even when multiple workers add concurrently.
  auto saturated = std::make_shared<Counter>();
  saturated->bytes.store(std::numeric_limits<std::uint64_t>::max() - 1);
  std::thread a([saturated] { Scope scope(saturated); received(3); });
  std::thread b([saturated] { Scope scope(saturated); received(3); });
  a.join(); b.join();
  assert(saturated->bytes.load() == std::numeric_limits<std::uint64_t>::max());
}
