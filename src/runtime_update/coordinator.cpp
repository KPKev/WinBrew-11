// SPDX-License-Identifier: GPL-3.0-or-later
#include "coordinator.hpp"
#include <utility>

namespace ww::runtime_update {
Coordinator::Coordinator(Backend *backend, std::uint64_t interval_seconds)
    : backend_(backend), interval_(interval_seconds ? interval_seconds : 86400),
      state_(backend ? State::idle : State::unsupported) {}

void Coordinator::tick(std::uint64_t now) {
  if (!backend_ || state_ == State::restart_required ||
      state_ == State::recovery_required) return;
  // A reset clock causes a fresh check instead of unsigned underflow.
  if (!checked_ || now < last_check_ || now - last_check_ >= interval_)
    check_now(now);
}
void Coordinator::check_now(std::uint64_t now) {
  if (!backend_ || state_ == State::restart_required ||
      state_ == State::recovery_required) return;
  checked_ = true;
  last_check_ = now;
  offer_ = {};
  Offer candidate;
  switch (backend_->check(candidate)) {
    case CheckResult::current: state_ = State::current; break;
    case CheckResult::available:
      if (candidate.release_id.empty() || candidate.version.empty()) {
        state_ = State::check_failed;
      } else {
        offer_ = std::move(candidate);
        state_ = State::available;
      }
      break;
    default: state_ = State::check_failed; break;
  }
}
bool Coordinator::install_approved(const std::string &release_id) {
  if (!backend_ || state_ != State::available || release_id.empty() ||
      release_id != offer_.release_id) return false;
  Offer approved = std::move(offer_);
  offer_ = {};
  // Clear the pending offer before entering the backend to consume consent.
  state_ = State::install_failed;
  switch (backend_->install(approved)) {
    case InstallResult::installed: state_ = State::restart_required; break;
    case InstallResult::recovery_required: state_ = State::recovery_required; break;
    default: state_ = State::install_failed; break;
  }
  return true;
}
} // namespace ww::runtime_update
