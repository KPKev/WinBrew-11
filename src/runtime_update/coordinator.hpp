// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>

namespace ww::runtime_update {
// A platform adapter must authenticate the release metadata and package against
// a configured publisher key. An absent adapter is deliberately unsupported.
struct Offer {
  std::string release_id;
  std::string version;
};
enum class CheckResult { unavailable, current, available };
enum class InstallResult { installed, failed_unchanged, failed_restored, recovery_required };
struct Backend {
  virtual ~Backend() = default;
  // Return available only for a fully authenticated Wine 11 runtime release.
  virtual CheckResult check(Offer &offer) = 0;
  // Reauthenticate the exact offered release, stage and verify all files, then
  // use a crash-recoverable transaction. Never alter game or user data.
  virtual InstallResult install(const Offer &offer) = 0;
};
enum class State { unsupported, idle, current, available, check_failed,
                   restart_required, install_failed, recovery_required };
class Coordinator {
 public:
  explicit Coordinator(Backend *backend, std::uint64_t interval_seconds = 86400);
  // Call from the host's normal event loop. This does not install an update.
  void tick(std::uint64_t monotonic_seconds);
  void check_now(std::uint64_t monotonic_seconds);
  // Consent is bound to the release ID shown to the user, and is single-use.
  bool install_approved(const std::string &release_id);
  State state() const { return state_; }
  const Offer &offer() const { return offer_; }
 private:
  Backend *backend_;
  std::uint64_t interval_;
  std::uint64_t last_check_ = 0;
  bool checked_ = false;
  State state_;
  Offer offer_;
};
} // namespace ww::runtime_update
