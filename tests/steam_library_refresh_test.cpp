// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/controller.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
using namespace playden;
using namespace ww::steam_integration;
using namespace std::chrono_literals;
namespace {
struct Sessions : SessionRepository {
  mutable std::mutex mutex;
  mutable std::optional<StoredSession> value = StoredSession{42,"Player","refresh","access",{}};
  Result<std::optional<StoredSession>> Load() const override {
    std::lock_guard<std::mutex> lock(mutex); return Result<std::optional<StoredSession>>::Success(value);
  }
  Result<void> Save(const StoredSession& s) const override {
    std::lock_guard<std::mutex> lock(mutex); value=s; return Result<void>::Success();
  }
  Result<void> Clear() const override {
    std::lock_guard<std::mutex> lock(mutex); value.reset(); return Result<void>::Success();
  }
};
struct Auth : steam::AuthBackend {
  std::atomic<bool> offline{false};
  std::atomic<int> restores{0};
  steam::AuthResult AuthenticateQr(const std::atomic<bool>&, const steam::ChallengeCallback&) override {
    assert(false); return {};
  }
  steam::AuthResult Restore(const StoredSession& s, const std::atomic<bool>&) override {
    ++restores; return {offline ? steam::AuthError::kNetwork : steam::AuthError::kNone,
                       offline ? std::nullopt : std::optional<StoredSession>{s}};
  }
};
struct Cache : LibraryRepository {
  Result<LibrarySnapshot> Load(std::uint64_t) const override {
    LibrarySnapshot s; s.games={{GameId::Steam(10),"Cached game",{},{},{}}};
    return Result<LibrarySnapshot>::Success(s);
  }
  Result<void> Save(std::uint64_t, const LibrarySnapshot&) const override { return Result<void>::Success(); }
  Result<void> Clear(std::uint64_t) const override { return Result<void>::Success(); }
};
struct Backend : steam::LibraryBackend {
  std::atomic<int> calls{0};
  std::atomic<bool> block{false}, canceled{false};
  steam::LibraryResult Fetch(const StoredSession&, const std::atomic<bool>& cancel) override {
    ++calls;
    while (block && !cancel) std::this_thread::sleep_for(1ms);
    if (cancel) { canceled=true; return {steam::LibraryError::kCanceled,{},{}}; }
    return {steam::LibraryError::kNone,{{GameId::Steam(20),"New game",{},{},{}}},false};
  }
};
template<class F> void Wait(Controller& c,F done) {
  const auto deadline=std::chrono::steady_clock::now()+3s;
  while (!done() && std::chrono::steady_clock::now()<deadline) { c.Tick(); std::this_thread::sleep_for(1ms); }
  c.Tick(); assert(done());
}
void OfflineRefreshReconnects() {
  Sessions sessions; Auth auth; auth.offline=true; Cache cache; Backend backend;
  steam::SteamService service(auth,sessions); LibraryService library(backend,sessions,&cache);
  Controller c(service,library); c.Restore();
  Wait(c,[&]{return c.view().library==LibraryState::ready;});
  assert(c.view().account==AccountState::offline && backend.calls==0 && auth.restores==1);
  // Still offline: retry once, fall back to cached rows without a reconnect loop.
  c.Refresh(); Wait(c,[&]{return c.view().library==LibraryState::ready;});
  assert(auth.restores==2 && backend.calls==0 && c.view().games.size()==1);
  auth.offline=false; c.Refresh();
  assert(c.view().games.size()==1); // Preserve the library during renewal.
  Wait(c,[&]{return c.view().account==AccountState::online && c.view().library==LibraryState::ready;});
  assert(auth.restores==3 && backend.calls==1 && c.view().games.front().id==GameId::Steam(20));
}
void CachedRowsDoNotFinishOrCancelRefresh() {
  Sessions sessions; Auth auth; Cache cache; Backend backend; backend.block=true;
  steam::SteamService service(auth,sessions); LibraryService library(backend,sessions,&cache);
  Controller c(service,library); c.Restore();
  Wait(c,[&]{return backend.calls==1 && !c.view().games.empty();});
  assert(c.view().library==LibraryState::loading && c.view().games.front().id==GameId::Steam(10));
  for(int i=0;i<5;++i) { c.Refresh(); c.Tick(); }
  assert(backend.calls==1 && !backend.canceled);
  backend.block=false; Wait(c,[&]{return c.view().library==LibraryState::ready;});
  assert(c.view().games.front().id==GameId::Steam(20));
  c.Refresh(); Wait(c,[&]{return c.view().library==LibraryState::ready;});
  assert(backend.calls==2);
}
}
int main() {
  OfflineRefreshReconnects(); CachedRowsDoNotFinishOrCancelRefresh();
  std::puts("PASS: offline reconnect, cached loading, duplicate refresh, and fresh games");
}
