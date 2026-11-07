#include "../src/steam_integration/download_tuner.hpp"
#include <cassert>
using ww::steam_integration::DownloadTuner;
int main() {
  const auto mb = 1024ull * 1024;
  DownloadTuner t; assert(t.slots() == 5 && !t.observe(0, 0));
  assert(!t.observe(15000, 100));
  assert(!t.observe(30000, 100)); // Too little data is not evidence.
  assert(!t.observe(45000, 30 * mb));
  assert(t.observe(60000, 60 * mb) && t.slots() == 6);
  assert(!t.observe(75000, 95 * mb));
  assert(t.observe(90000, 132 * mb) && t.slots() == 7);
  assert(!t.observe(105000, 165 * mb));
  assert(t.observe(120000, 197 * mb) && t.finished() && t.slots() == 6);
  DownloadTuner rising; rising.observe(0, 0);
  std::uint64_t bytes = 0;
  for (int n = 5; n <= 20; ++n) {
    const auto delta = std::uint64_t(n) * 10 * mb;
    assert(!rising.observe(std::uint64_t(n - 5) * 30000 + 15000, bytes + delta / 2));
    bytes += delta;
    rising.observe(std::uint64_t(n - 4) * 30000, bytes);
  }
  assert(rising.finished() && rising.slots() == 20 && rising.best_slots() == 20);
  DownloadTuner stalled; stalled.observe(0, 0); stalled.observe(15000, 30 * mb);
  assert(stalled.observe(30000, 60 * mb) && stalled.slots() == 6);
  assert(!stalled.observe(45000, 60 * mb));
  assert(!stalled.observe(60000, 60 * mb));
  assert(!stalled.observe(75000, 60 * mb));
  assert(!stalled.observe(90000, 60 * mb));
  assert(!stalled.observe(91000, 68 * mb) && !stalled.finished());
  assert(!stalled.observe(105000, 98 * mb));
  assert(stalled.observe(120000, 130 * mb) && stalled.slots() == 7);
  assert(!stalled.observe(10, 130 * mb)); // backward clock
  assert(!stalled.observe(1000, 131 * mb));
  // A long missed observation ends with bytes: that interval is ambiguous.
  DownloadTuner missed; missed.observe(0, 0); missed.observe(15000, 30 * mb);
  assert(missed.observe(30000, 60 * mb) && missed.slots() == 6);
  assert(!missed.observe(90000, 68 * mb) && !missed.finished() && missed.slots() == 6);
  assert(!missed.observe(105000, 100 * mb));
  assert(missed.observe(120000, 132 * mb) && missed.slots() == 7);
  // A recent observation bounds the new bytes to after the stall; retain them.
  DownloadTuner bounded; bounded.observe(0, 0); bounded.observe(15000, 30 * mb);
  assert(bounded.observe(30000, 60 * mb));
  assert(!bounded.observe(59900, 60 * mb));
  assert(!bounded.observe(60000, 68 * mb));
  assert(!bounded.observe(75000, 98 * mb));
  assert(bounded.observe(89900, 130 * mb));
  assert(bounded.sampled_bytes() == 70 * mb && bounded.sampled_ms() == 30000);
}
