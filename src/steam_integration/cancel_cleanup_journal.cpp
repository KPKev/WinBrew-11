// SPDX-License-Identifier: GPL-3.0-or-later
#include "cancel_cleanup_journal.hpp"
#include "platform/ps5/title_allow_list.h"
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace ww::steam_integration {
namespace {
constexpr const char* kJournal = "cancel-cleanup.v1";
std::string Encode(std::uint32_t app, std::uint64_t operation, std::uint64_t device,
                   std::uint64_t inode, const CancelTargetEvidence& target, bool bound) {
  return "3 " + std::to_string(app) + " " + std::to_string(operation) + " " +
      std::to_string(device) + " " + std::to_string(inode) + " " + std::to_string(target.nonce) + " " +
      std::to_string(target.device) + " " + std::to_string(target.inode) + " " + (bound ? "1\n" : "0\n");
}
playden::Bytes Bytes(const std::string& text) { return {text.begin(), text.end()}; }
}
CancelCleanupJournal::CancelCleanupJournal(const playden::platform::ManagedDirectory& state,
    const playden::platform::ManagedFileLock& state_lease,
    const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& games_lease)
    : state_(state), state_lease_(state_lease), games_(games), games_lease_(games_lease),
      storage_(state.path().string()) {}

bool CancelCleanupJournal::Identity(std::uint64_t& device, std::uint64_t& inode) const {
  struct stat info{};
  if (!state_.Validate() || !state_lease_.Validate(state_, "wine11-state.lock") ||
      !games_.Validate() || !games_lease_.Validate(games_, "wine11-install.lock") ||
      fstat(games_.fd(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;
  device = static_cast<std::uint64_t>(info.st_dev);
  inode = static_cast<std::uint64_t>(info.st_ino);
  return true;
}
CancelCleanupJournal::Entry CancelCleanupJournal::ReadLocked() {
  std::uint64_t device = 0, inode = 0;
  if (!Identity(device, inode)) return {};
  const auto bytes = storage_.ReadDurable(kJournal, 256);
  if (!bytes.ok()) {
    if (bytes.error() == playden::StorageError::kNotFound && Identity(device, inode)) return {State::absent, 0, 0};
    return {};
  }
  const std::string text(bytes.value().begin(), bytes.value().end());
  // Winner records are not accepted until a record can be bound to the
  // current install generation and its content is reverified after restart.
  if (text.rfind("4 ", 0) == 0) return {};
  std::uint32_t app = 0; std::uint64_t operation = 0, recorded_device = 0, recorded_inode = 0;
  CancelTargetEvidence target;
  unsigned version = 0, phase = 0;
  std::istringstream input(text);
  if (!(input >> version >> app >> operation >> recorded_device >> recorded_inode >> target.nonce >> target.device >> target.inode >> phase) ||
      version != 3 || !app || !operation || !target.nonce || !target.inode || phase > 1) return {};
  target.app = app;
  if (text != Encode(app, operation, recorded_device, recorded_inode, target, phase == 1) ||
      device != recorded_device || inode != recorded_inode || !Identity(device, inode) ||
      device != recorded_device || inode != recorded_inode) return {};
  if (phase == 1) {
    if (!VerifyCancelTarget(games_, games_lease_, target)) return {};
  } else {
    CancelTargetEvidence current;
    if (!InspectCancelTarget(games_, games_lease_, app, target.nonce, current) ||
        current.device != target.device || current.inode != target.inode) return {};
  }
  return {phase ? State::intent : State::prepared, app, operation, target};
}
CancelCleanupJournal::Entry CancelCleanupJournal::Read() {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::lock_guard<std::mutex> lock(mutex_);
  return ReadLocked();
}
bool CancelCleanupJournal::Prepare(std::uint32_t app, std::uint64_t operation, const CancelTargetEvidence& target) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::lock_guard<std::mutex> lock(mutex_);
  CancelTargetEvidence current;
  if (!app || !operation || target.app != app || ReadLocked().state != State::absent ||
      !InspectCancelTarget(games_, games_lease_, app, target.nonce, current) ||
      current.device != target.device || current.inode != target.inode) return false;
  std::uint64_t device = 0, inode = 0;
  if (!Identity(device, inode)) return false;
  const auto result = storage_.WriteDurableExclusive(kJournal, Bytes(Encode(app, operation, device, inode, target, false)));
  // A failed write may have published a record. Do not infer absence or delete.
  if (!result.ok()) return false;
  const auto observed = ReadLocked();
  return observed.state == State::prepared && observed.app == app && observed.operation == operation;
}
bool CancelCleanupJournal::BindMarker(std::uint32_t app, std::uint64_t operation) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::lock_guard<std::mutex> lock(mutex_);
  const auto current = ReadLocked();
  if (current.state != State::prepared || current.app != app || current.operation != operation ||
      !VerifyCancelTarget(games_, games_lease_, current.target)) return false;
  std::uint64_t device = 0, inode = 0;
  if (!Identity(device, inode)) return false;
  if (!storage_.WriteDurable(kJournal, Bytes(Encode(app, operation, device, inode, current.target, true))).ok()) return false;
  const auto observed = ReadLocked();
  return observed.state == State::intent && observed.app == app && observed.operation == operation;
}
bool CancelCleanupJournal::RecordWinner(std::uint32_t app, std::uint64_t operation, const CancelWinnerEvidence& winner) {
  (void)app; (void)operation; (void)winner;
  // A visible record has not yet been bound to the current install operation,
  // and syncing its metadata does not establish game-content durability.
  return false;
}
bool CancelCleanupJournal::CaptureMarker(std::uint32_t app, std::uint64_t operation) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::lock_guard<std::mutex> lock(mutex_);
  const auto current = ReadLocked();
  if (current.state != State::prepared || current.app != app || current.operation != operation) return false;
  return CapturePreparedCancelTarget(games_, games_lease_, current.target);
}
bool CancelCleanupJournal::Complete(std::uint32_t app, std::uint64_t operation) {
  (void)app; (void)operation;
  // No authoritative deletion or durable registry reconciliation exists yet.
  return false;
}
}
