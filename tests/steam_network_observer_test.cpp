// SPDX-License-Identifier: GPL-3.0-or-later
// This includes the generated transport in order to exercise the actual curl body callback.
#include "../build/test-observed-http.cpp"
#include <cassert>
#include <thread>
#include <vector>
using namespace ww::steam_integration::traffic;
int main() {
  Registry registry; auto counter = registry.begin(440);
  char data[16]{};
  // Failed/over-limit responses still delivered bytes; retries count again.
  auto attempt = [counter, &data](bool overflow) {
    Scope scope(counter);
    playden::steam::TransferBuffer transfer; transfer.body_limit = overflow ? 4 : 16;
    auto n = playden::steam::WriteBody(data, 1, 8, &transfer);
    assert(n == (overflow ? 0u : 8u));
  };
  std::vector<std::thread> workers;
  for (int i=0; i<8; ++i) workers.emplace_back(attempt, i % 2 == 0);
  for (auto& worker : workers) worker.join();
  assert(counter->bytes.load() == 64);
  { Scope scope(counter); playden::steam::TransferBuffer partial; partial.body_limit=16;
    assert(playden::steam::WriteBody(data,1,4,&partial)==4); } // canceled after partial body
  assert(counter->bytes.load() == 68);
  playden::steam::TransferBuffer unrelated; unrelated.body_limit=16;
  assert(playden::steam::WriteBody(data,1,4,&unrelated)==4);
  assert(counter->bytes.load() == 68);
  registry.end(counter);
}
