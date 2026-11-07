// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/controller.hpp"
#include "../src/steam_integration/ui_adapter.hpp"
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
#include <cstdio>
using namespace playden;
struct Sessions : SessionRepository {
  mutable std::mutex mutex;
  mutable std::optional<StoredSession> session;
  bool fail_clear = false;
  Result<std::optional<StoredSession>> Load() const override {
    std::lock_guard<std::mutex> l(mutex); return Result<std::optional<StoredSession>>::Success(session);
  }
  Result<void> Save(const StoredSession& s) const override {
    std::lock_guard<std::mutex> l(mutex); session = s; return Result<void>::Success();
  }
  Result<void> Clear() const override {
    std::lock_guard<std::mutex> l(mutex);
    if (fail_clear) return Result<void>::Failure(StorageError::kRemoveFailed, "failed");
    session.reset(); return Result<void>::Success();
  }
};
struct Auth : steam::AuthBackend {
  std::atomic<bool> hold{false}, started{false}, hold_after_qr{false};
  steam::AuthResult AuthenticateQr(const std::atomic<bool>& cancel, const steam::ChallengeCallback& challenge) override {
    started = true;
    while (hold && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (cancel) return {steam::AuthError::kCanceled, std::nullopt};
    challenge("steam://test-challenge");
    while (hold_after_qr && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (cancel) return {steam::AuthError::kCanceled, std::nullopt};
    return {steam::AuthError::kNone, StoredSession{42, "Player", "refresh", "access", {}}};
  }
  steam::AuthResult AuthenticateCredentials(steam::Credentials& credentials, const std::atomic<bool>& cancel,
      const steam::GuardPromptCallback& prompt, const steam::GuardCodeSource& code) override {
    assert(credentials.account_name == "Player" && credentials.password == "test password");
    steam::ZeroizeSecret(credentials.password);
    steam::GuardPrompt request; request.kind = steam::GuardPromptKind::kEmailCode;
    request.email_domain = "example.test"; prompt(request);
    while (!cancel) {
      auto supplied = code();
      if (supplied) {
        const bool correct = *supplied == "12345";
        steam::ZeroizeSecret(*supplied);
        if (correct) return {steam::AuthError::kNone, StoredSession{42, "Player", "refresh", "access", {}}};
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return {steam::AuthError::kCanceled, std::nullopt};
  }
  steam::AuthResult Restore(const StoredSession& session, const std::atomic<bool>&) override {
    return {steam::AuthError::kNone, session};
  }
};
struct Library : steam::LibraryBackend {
  steam::LibraryResult Fetch(const StoredSession& session, const std::atomic<bool>&) override {
    assert(session.steam_id == 42);
    GameSummary game; game.id = GameId::Steam(440); game.title = "Example game";
    return {steam::LibraryError::kNone, {game}, false};
  }
};
template<class F> void wait(ww::steam_integration::Controller& controller, F done) {
  for (int i = 0; i < 500 && !done(); ++i) {
    controller.Tick(); std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  controller.Tick(); assert(done());
}
int main() {
  Sessions sessions; Auth backend; Library library_backend;
  steam::SteamService auth(backend, sessions); LibraryService library(library_backend, sessions);
  ww::steam_integration::Controller controller(auth, library);
  controller.SignInQr();
  wait(controller, [&]{ return controller.view().library == ww::steam_integration::LibraryState::ready; });
  assert(controller.view().account == ww::steam_integration::AccountState::online);
  assert(controller.view().account_name == "Player");
  assert(controller.view().games.size() == 1 && controller.view().games[0].id.steam_app() == 440);
  controller.SignOut();
  assert(controller.view().games.empty());
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::signed_out; });
  assert(!sessions.Load().value());
  controller.SignInQr();
  wait(controller, [&]{ return controller.view().library == ww::steam_integration::LibraryState::ready; });
  sessions.fail_clear = true;
  controller.SignOut();
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::failed; });
  assert(controller.view().auth_error == steam::AuthError::kPersistence);
  assert(controller.view().games.empty() && sessions.Load().value());
  sessions.fail_clear = false;
  controller.SignOut();
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::signed_out; });
  assert(!sessions.Load().value());
  backend.started = false; backend.hold = true;
  controller.SignInQr();
  for (int i = 0; i < 500 && !backend.started; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(backend.started);
  controller.Cancel();
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::signed_out; });
  backend.hold = false;
  assert(controller.view().games.empty() && !sessions.Load().value());
  controller.SignIn("Player", "test password");
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::guard_email; });
  assert(controller.view().email_domain == "example.test");
  controller.SubmitGuardCode("12345");
  wait(controller, [&]{ return controller.view().library == ww::steam_integration::LibraryState::ready; });
  assert(controller.view().email_domain.empty());
  controller.SignOut();
  wait(controller, [&]{ return controller.view().account == ww::steam_integration::AccountState::signed_out; });
  ww::steam_integration::UiAdapter ui(controller);
  backend.hold_after_qr = true;
  ui.sign_in();
  for (int i = 0; i < 500 && ui.status().qr_size == 0; ++i) {
    ui.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  assert(ui.status().account == ww::SteamAccount::approval && ui.status().qr_size >= 21);
  assert(ui.qr_module(0, 0) && !ui.qr_module(1, 1));
  assert(!ui.qr_module(-1, 0));
  backend.hold_after_qr = false;
  for (int i = 0; i < 500 && ui.status().library != ww::SteamLibrary::ready; ++i) {
    ui.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  ww::SteamGame visible;
  assert(ui.status().library == ww::SteamLibrary::ready && ui.status().qr_size == 0);
  assert(ui.game(0, visible) && visible.app_id == 440 && std::string(visible.title) == "Example game");
  ui.sign_out();
  for (int i = 0; i < 500 && ui.status().account != ww::SteamAccount::signed_out; ++i) {
    ui.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  assert(ui.status().account == ww::SteamAccount::signed_out && ui.status().game_count == 0);
  std::puts("PASS: Playden worker QR, Steam Guard, library, cancellation, sign-out failure and UI adapter");
}
