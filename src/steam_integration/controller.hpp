// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "steam/service.h"
#include "library/service.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ww::steam_integration {
// UI-thread coordinator. The Playden services and their dependencies outlive it.
// It never owns credentials or performs network or storage work on this thread.
enum class AccountState { signed_out, signing_in, approval, guard_email, guard_app,
                          online, offline, signing_out, failed };
enum class LibraryState { empty, loading, ready, failed };
struct View {
  AccountState account = AccountState::signed_out;
  LibraryState library = LibraryState::empty;
  std::string account_name, challenge_url, email_domain;
  std::uint64_t account_id = 0, account_generation = 0;
  playden::steam::AuthError auth_error = playden::steam::AuthError::kNone;
  playden::steam::LibraryError library_error = playden::steam::LibraryError::kNone;
  bool code_allowed = false, code_rejected = false, metadata_partial = false;
  std::vector<playden::GameSummary> games;
};
class Controller {
 public:
  Controller(playden::steam::SteamService&, playden::LibraryService&);
  void SignInQr();
  void SignIn(std::string account, std::string password);
  void SubmitGuardCode(std::string code);
  void Restore();
  void Cancel();
  void SignOut();
  void Refresh();
  void Tick();
  const View& view() const { return view_; }
 private:
  void ResetLibrary();
  void Process(const playden::steam::SteamEvent&);
  void Process(playden::LibraryEvent&);
  playden::steam::SteamService& auth_;
  playden::LibraryService& library_;
  View view_;
  std::uint64_t auth_request_ = 0, account_id_ = 0, generation_ = 0,
                library_request_ = 0;
};
}
