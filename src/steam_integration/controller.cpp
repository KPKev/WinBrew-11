// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller.hpp"
#include <utility>
namespace ww::steam_integration {
using playden::steam::SteamEventKind;
Controller::Controller(playden::steam::SteamService& auth, playden::LibraryService& library)
    : auth_(auth), library_(library) {}
void Controller::ResetLibrary() {
  library_.Invalidate(++generation_);
  account_id_ = library_request_ = 0;
  view_ = View{};
  view_.account_generation = generation_;
}
void Controller::SignInQr() {
  ResetLibrary(); view_.account = AccountState::signing_in;
  auth_request_ = auth_.BeginQrSignIn();
}
void Controller::SignIn(std::string account, std::string password) {
  ResetLibrary(); view_.account = AccountState::signing_in;
  auth_request_ = auth_.BeginCredentialSignIn(std::move(account), std::move(password));
  playden::steam::ZeroizeSecret(password);
}
void Controller::SubmitGuardCode(std::string code) {
  view_.code_rejected = false;
  auth_.SubmitGuardCode(std::move(code));
  playden::steam::ZeroizeSecret(code);
}
void Controller::Restore() {
  ResetLibrary(); view_.account = AccountState::signing_in;
  auth_request_ = auth_.RestoreSession();
}
void Controller::Cancel() { if (auth_request_) auth_.Cancel(auth_request_); }
void Controller::SignOut() {
  ResetLibrary(); auth_request_ = 0;
  view_.account = AccountState::signing_out; auth_.SignOut();
}
void Controller::Refresh() {
  if (!account_id_) return;
  if (view_.account != AccountState::online && view_.account != AccountState::offline) return;
  view_.library = LibraryState::loading;
  library_request_ = view_.account == AccountState::offline
      ? library_.LoadCached(account_id_, generation_)
      : library_.Refresh(account_id_, generation_);
  if (!library_request_) view_.library = LibraryState::failed;
}
void Controller::Tick() {
  for (const auto& event : auth_.Pump(64)) Process(event);
  for (auto& event : library_.Pump(64)) Process(event);
}
void Controller::Process(const playden::steam::SteamEvent& e) {
  if (view_.account == AccountState::signing_out) {
    if (e.kind == SteamEventKind::kSignedOut) { view_ = View{}; return; }
    if (e.kind == SteamEventKind::kFailure) {
      view_.account = AccountState::failed; view_.auth_error = e.error;
    }
    return;
  }
  if (!auth_request_ || e.request_id != auth_request_) return;
  switch (e.kind) {
    case SteamEventKind::kChallenge:
      view_.challenge_url = e.challenge_url; break;
    case SteamEventKind::kAwaitingApproval:
      view_.account = AccountState::approval; view_.code_allowed = e.code_allowed;
      view_.email_domain = e.email_domain; break;
    case SteamEventKind::kNeedsEmailCode:
      view_.account = AccountState::guard_email; view_.email_domain = e.email_domain; break;
    case SteamEventKind::kNeedsAuthenticatorCode:
      view_.account = AccountState::guard_app; break;
    case SteamEventKind::kCodeRejected:
      view_.account = e.code_is_email ? AccountState::guard_email : AccountState::guard_app;
      view_.code_rejected = true; break;
    case SteamEventKind::kAuthenticated:
    case SteamEventKind::kOfflineSession:
      view_.challenge_url.clear(); view_.email_domain.clear();
      view_.account = e.kind == SteamEventKind::kAuthenticated ? AccountState::online : AccountState::offline;
      view_.account_name = e.account_name; account_id_ = e.steam_id; view_.account_id = e.steam_id;
      auth_request_ = 0; Refresh(); break;
    case SteamEventKind::kCanceled:
      auth_request_ = 0; view_ = View{}; break;
    case SteamEventKind::kFailure:
      auth_request_ = 0; view_.challenge_url.clear(); view_.email_domain.clear();
      view_.account = AccountState::failed; view_.auth_error = e.error; break;
    case SteamEventKind::kSignedOut: break;
  }
}
void Controller::Process(playden::LibraryEvent& e) {
  if (!library_request_ || e.request_id != library_request_ ||
      e.account_generation != generation_ || e.account_id != account_id_) return;
  switch (e.kind) {
    case playden::LibraryEventKind::kLoading: view_.library = LibraryState::loading; break;
    case playden::LibraryEventKind::kCached:
    case playden::LibraryEventKind::kComplete:
      view_.games = std::move(e.games); view_.metadata_partial = e.metadata_partial;
      view_.library = LibraryState::ready;
      if (e.kind == playden::LibraryEventKind::kComplete || view_.account == AccountState::offline)
        library_request_ = 0;
      break;
    case playden::LibraryEventKind::kFailure:
      view_.library_error = e.error; view_.library = LibraryState::failed; library_request_ = 0; break;
    case playden::LibraryEventKind::kCleared: library_request_ = 0; break;
  }
}
}
