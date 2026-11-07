// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed, ephemeral Playden QR-auth challenge test. No credentials, no QR URL
// output, no account approval, no credential persistence and no file writes.
#include "steam/backend/auth_client.h"
#include "steam/service.h"
#include <mbedtls/sha256.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
namespace {
constexpr const char* ca_path = "/data/homebrew/FAKE77031/trust/cacert.pem";
const unsigned char expected[32] = {
  0xa4,0x1b,0x5d,0x35,0x6a,0xea,0x97,0xa5,0x29,0xfe,0x27,0xe0,0xf7,0x31,0x6d,0x2f,
  0x9d,0x94,0x6d,0x75,0x92,0x74,0x76,0xcf,0x9c,0xf1,0xb9,0x06,0x37,0xd0,0x05,0x05};
bool certificate_valid() {
  FILE* file=std::fopen(ca_path,"rb"); if (!file) return false;
  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx);
  int rc=mbedtls_sha256_starts(&ctx,0); unsigned char buffer[4096]; size_t total=0;
  while (rc==0) {
    const size_t n=std::fread(buffer,1,sizeof(buffer),file); total+=n;
    if (total>256*1024) { rc=-1; break; }
    if (n && mbedtls_sha256_update(&ctx,buffer,n)!=0) { rc=-1; break; }
    if (n<sizeof(buffer)) { if (std::ferror(file)) rc=-1; break; }
  }
  unsigned char digest[32]{};
  if (rc==0) rc=mbedtls_sha256_finish(&ctx,digest);
  mbedtls_sha256_free(&ctx); if (std::fclose(file)!=0) rc=-1;
  return rc==0 && total==188900 && std::memcmp(digest,expected,sizeof(expected))==0;
}

struct NoDiskSessions final : playden::SessionRepository {
  mutable std::atomic<unsigned> saves{0};
  playden::Result<std::optional<playden::StoredSession>> Load() const override {
    return playden::Result<std::optional<playden::StoredSession>>::Success(std::nullopt);
  }
  playden::Result<void> Save(const playden::StoredSession&) const override {
    ++saves; return playden::Result<void>::Failure(playden::StorageError::kWriteFailed,"disabled");
  }
  playden::Result<void> Clear() const override { return playden::Result<void>::Success(); }
};
}
int main() {
  std::puts("PLAYDEN_AUTH start mode=ephemeral_qr credentials=none writes=none"); std::fflush(stdout);
  if (!certificate_valid()) { std::puts("PLAYDEN_AUTH certificate=invalid\nPLAYDEN_AUTH done success=0"); std::fflush(stdout); return 1; }
  std::puts("PLAYDEN_AUTH certificate=verified"); std::fflush(stdout);
  NoDiskSessions sessions; bool challenge=false,canceled=false,failed=false;
  {
    playden::steam::AuthClient backend(ca_path);
    playden::steam::SteamService service(backend,sessions);
    const auto request=service.BeginQrSignIn();
    std::puts("PLAYDEN_AUTH phase=request_queued"); std::fflush(stdout);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
    while (std::chrono::steady_clock::now()<deadline && !canceled && !failed) {
      for (auto& event : service.Pump(16)) {
        if (event.request_id != request) continue;
        if (event.kind == playden::steam::SteamEventKind::kChallenge && !challenge) {
          challenge=!event.challenge_url.empty();
          std::printf("PLAYDEN_AUTH challenge_received=%d\n",challenge); std::fflush(stdout);
          service.Cancel(request);
        } else if (event.kind == playden::steam::SteamEventKind::kCanceled) {
          canceled=true;
        } else if (event.kind == playden::steam::SteamEventKind::kFailure) {
          std::printf("PLAYDEN_AUTH auth_error=%d\n",int(event.error)); std::fflush(stdout); failed=true;
        } else if (event.kind == playden::steam::SteamEventKind::kAuthenticated) {
          // No code is displayed or approved; an unexpected session is an error.
          std::puts("PLAYDEN_AUTH unexpected_authentication"); failed=true;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!canceled) service.Cancel(request);
    std::puts("PLAYDEN_AUTH phase=worker_shutdown"); std::fflush(stdout);
  }
  const bool ok=challenge && canceled && !failed && sessions.saves.load()==0;
  std::printf("PLAYDEN_AUTH canceled=%d save_attempts=%u\n",canceled,sessions.saves.load());
  std::printf("PLAYDEN_AUTH done success=%d\n",ok); std::fflush(stdout);
  return ok ? 0 : 1;
}
