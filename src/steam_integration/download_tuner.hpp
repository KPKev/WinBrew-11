// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace ww::steam_integration {
// Increase one connection at a time, comparing real, completed download bytes.
// A short or idle window is not evidence that a connection count is slower.
class DownloadTuner {
 public:
  static constexpr int Minimum = 5, Maximum = 20;
  static constexpr std::uint64_t WindowMs = 30000, MinimumBytes = 8ull * 1024 * 1024;
  int slots() const { return slots_; }
  int best_slots() const { return best_slots_; }
  bool finished() const { return finished_; }
  int sampled_slots() const { return sampled_slots_; }
  std::uint64_t sampled_bytes() const { return sampled_bytes_; }
  std::uint64_t sampled_ms() const { return sampled_ms_; }
  // now_ms and completed_bytes must be monotonically increasing.
  bool observe(std::uint64_t now_ms, std::uint64_t completed_bytes) {
    if (finished_) return false;
    if (!started_ || now_ms < last_ms_ || completed_bytes < last_bytes_) {
      started_ = true; start_ms_ = last_ms_ = progress_ms_ = now_ms;
      start_bytes_ = last_bytes_ = completed_bytes; return false;
    }
    const bool progressed = completed_bytes > last_bytes_;
    const bool long_gap = now_ms - last_ms_ >= WindowMs;
    const bool stalled = now_ms - progress_ms_ >= WindowMs;
    if (long_gap) {
      // We cannot tell when bytes arrived during a missed observation window.
      // Exclude that ambiguous interval from calibration, not from download progress.
      start_ms_ = progress_ms_ = last_ms_ = now_ms;
      start_bytes_ = last_bytes_ = completed_bytes;
      return false;
    }
    if (stalled) {
      // With a recent observation, bytes since that observation are known to
      // have arrived after the stall and remain part of the next sample.
      start_ms_ = progressed ? last_ms_ : now_ms;
      start_bytes_ = progressed ? last_bytes_ : completed_bytes;
      progress_ms_ = now_ms;
    } else if (progressed) progress_ms_ = now_ms;
    last_ms_ = now_ms; last_bytes_ = completed_bytes;
    const auto elapsed = now_ms - start_ms_, bytes = completed_bytes - start_bytes_;
    if (elapsed < WindowMs || bytes < MinimumBytes) return false;
    const double rate = double(bytes) * 1000.0 / double(elapsed);
    sampled_slots_ = slots_; sampled_bytes_ = bytes; sampled_ms_ = elapsed;
    bool changed = false;
    if (best_rate_ == 0 || rate > best_rate_ * 1.03) {
      best_rate_ = rate; best_slots_ = slots_;
      if (slots_ == Maximum) finished_ = true;
      else { ++slots_; changed = true; }
    } else {
      slots_ = best_slots_; finished_ = true; changed = true;
    }
    start_ms_ = now_ms; start_bytes_ = completed_bytes;
    return changed;
  }
 private:
  int slots_ = Minimum, best_slots_ = Minimum;
  double best_rate_ = 0;
  bool started_ = false, finished_ = false;
  std::uint64_t start_ms_ = 0, start_bytes_ = 0, last_ms_ = 0, last_bytes_ = 0, progress_ms_ = 0;
  int sampled_slots_ = 0;
  std::uint64_t sampled_bytes_ = 0, sampled_ms_ = 0;
};
}
