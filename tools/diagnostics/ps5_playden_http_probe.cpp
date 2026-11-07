// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed-host diagnostic for Playden's unmodified HTTP client. No credentials,
// account APIs, persistent state, user data, writes, or application changes.
#include "steam/backend/http_client.h"
#include <mbedtls/sha256.h>
#include <atomic>
#include <cstdio>
#include <cstring>
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
bool perform(playden::steam::HttpClient& client,const char* label,const char* url) {
  using namespace playden::steam;
  std::printf("PLAYDEN_HTTP phase=request host=%s\n",label); std::fflush(stdout);
  HttpRequest request; request.url=url; request.timeout_ms=12000; request.max_response_bytes=65536;
  std::atomic<bool> cancel{false}; auto response=client.Perform(request,cancel);
  std::printf("PLAYDEN_HTTP host=%s error=%s transport=%d http=%ld bytes=%zu\n",label,
              HttpErrorName(response.error),response.transport_code,response.status_code,response.body.size());
  std::fflush(stdout);
  return response.error==HttpError::kNone && response.status_code==200;
}
}
int main() {
  std::puts("PLAYDEN_HTTP start client=playden fixed_hosts=2 credentials=none writes=none"); std::fflush(stdout);
  if (!certificate_valid()) { std::puts("PLAYDEN_HTTP certificate=invalid\nPLAYDEN_HTTP done success=0"); std::fflush(stdout); return 1; }
  std::puts("PLAYDEN_HTTP certificate=verified\nPLAYDEN_HTTP phase=client_init"); std::fflush(stdout);
  bool example=false,steam=false;
  {
    playden::steam::HttpClient client(ca_path);
    std::puts("PLAYDEN_HTTP phase=client_ready"); std::fflush(stdout);
    example=perform(client,"example.com","https://example.com/");
    steam=perform(client,"api.steampowered.com","https://api.steampowered.com/ISteamWebAPIUtil/GetServerInfo/v1/");
  }
  std::printf("PLAYDEN_HTTP done success=%d\n",example && steam); std::fflush(stdout);
  return example && steam ? 0 : 1;
}
