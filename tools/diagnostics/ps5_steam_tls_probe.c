/* SPDX-License-Identifier: GPL-3.0-or-later
 * Isolated, read-only PS5 test of the locally rebuilt curl/Mbed TLS libraries.
 * Fixed public URLs; no Steam account, user data, dynamic input, or file writes.
 */
#include <arpa/inet.h>
#include <curl/curl.h>
#include <mbedtls/threading.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "steam_probe_ca.h"
extern int sceNetInit(void);
extern int sceNetPoolCreate(const char *, int, int);
extern int sceNetPoolDestroy(int);
extern int sceNetResolverCreate(const char *, int, int);
extern int sceNetResolverDestroy(int);
extern int sceNetResolverStartNtoa(int, const char *, void *, int, int, int);
static size_t discard(char *data, size_t size, size_t count, void *context) {
  size_t *total = (size_t *)context;
  (void)data;
  if (size && count > SIZE_MAX / size) return 0;
  size_t bytes = size * count;
  if (bytes > 65536 || *total > 65536 - bytes) return 0;
  *total += bytes;
  return bytes;
}
static int request(const char *host, const char *url, int pool, int invalid_ca) {
  printf("TLS_PROBE phase=resolver_create host=%s\n",host); fflush(stdout);
  int resolver = sceNetResolverCreate("wine11-tls-probe", pool, 0);
  if (resolver < 0) { printf("TLS_PROBE host=%s resolver_create=%08x\n",host,(unsigned)resolver); return 0; }
  struct in_addr address = {0}; char numeric[INET_ADDRSTRLEN] = {0};
  printf("TLS_PROBE phase=resolve host=%s\n",host); fflush(stdout);
  int rc = sceNetResolverStartNtoa(resolver, host, &address.s_addr, 3000000, 0, 0);
  sceNetResolverDestroy(resolver);
  if (rc < 0 || !inet_ntop(AF_INET, &address, numeric, sizeof(numeric))) {
    printf("TLS_PROBE host=%s dns=%08x\n",host,(unsigned)rc); return 0;
  }
  char mapping[128]; snprintf(mapping, sizeof(mapping), "%s:443:%s", host, numeric);
  struct curl_slist *addresses = curl_slist_append(NULL, mapping);
  CURL *easy = curl_easy_init(); size_t total = 0; long http = 0; CURLcode result = CURLE_FAILED_INIT;
  struct curl_blob ca = {invalid_ca ? (void *)wine11_wrong_ca : (void *)wine11_probe_ca,
                         invalid_ca ? sizeof(wine11_wrong_ca) : sizeof(wine11_probe_ca), CURL_BLOB_NOCOPY};
  if (!addresses || !easy) goto done;
#define SETOPT(option, value) do { if (curl_easy_setopt(easy, (option), (value)) != CURLE_OK) goto done; } while (0)
  SETOPT(CURLOPT_URL, url);
  SETOPT(CURLOPT_RESOLVE, addresses);
  SETOPT(CURLOPT_CAINFO_BLOB, &ca);
  SETOPT(CURLOPT_SSL_VERIFYPEER, 1L);
  SETOPT(CURLOPT_SSL_VERIFYHOST, 2L);
  SETOPT(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
  SETOPT(CURLOPT_TIMEOUT_MS, 15000L);
  SETOPT(CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  SETOPT(CURLOPT_NOSIGNAL, 1L);
  SETOPT(CURLOPT_PROXY, "");
  SETOPT(CURLOPT_FOLLOWLOCATION, 0L);
  SETOPT(CURLOPT_WRITEFUNCTION, discard);
  SETOPT(CURLOPT_WRITEDATA, &total);
  printf("TLS_PROBE phase=transfer host=%s invalid_ca=%d\n",host,invalid_ca); fflush(stdout);
  result = curl_easy_perform(easy);
  curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http);
done:
  printf("TLS_PROBE host=%s invalid_ca=%d curl=%d http=%ld bytes=%zu\n", host, invalid_ca, (int)result, http, total);
  if (easy) curl_easy_cleanup(easy);
  curl_slist_free_all(addresses);
  if (invalid_ca) return result == CURLE_PEER_FAILED_VERIFICATION && http == 0;
  return result == CURLE_OK && http == 200;
}
int main(void) {
  puts("TLS_PROBE start endpoints=2 requests=3 credentials=none disk_writes=none"); fflush(stdout);
  mbedtls_threading_mutex_t mutex;
  mbedtls_mutex_init(&mutex);
  int lock = mbedtls_mutex_lock(&mutex);
  int unlock = lock == 0 ? mbedtls_mutex_unlock(&mutex) : -1;
  mbedtls_mutex_free(&mutex);
  printf("TLS_PROBE mutex_lock=%d mutex_unlock=%d\n", lock, unlock); fflush(stdout);
  if (lock || unlock) { puts("TLS_PROBE done success=0"); return 1; }
  puts("TLS_PROBE phase=net_init"); fflush(stdout);
  int init = sceNetInit();
  printf("TLS_PROBE net_init_result=%08x\n", (unsigned)init); fflush(stdout);
  if (init < 0) { printf("TLS_PROBE net_init=%08x\n",(unsigned)init); puts("TLS_PROBE done success=0"); return 1; }
  puts("TLS_PROBE phase=pool_create"); fflush(stdout);
  int pool = sceNetPoolCreate("wine11-tls-probe", 0x4000, 0);
  printf("TLS_PROBE pool_result=%08x\n", (unsigned)pool); fflush(stdout);
  if (pool < 0) { printf("TLS_PROBE pool=%08x\n",(unsigned)pool); puts("TLS_PROBE done success=0"); return 1; }
  puts("TLS_PROBE phase=curl_global_init"); fflush(stdout);
  CURLcode global = curl_global_init(CURL_GLOBAL_DEFAULT);
  printf("TLS_PROBE curl_global_result=%d\n", (int)global); fflush(stdout);
  if (global != CURLE_OK) { printf("TLS_PROBE curl_init=%d\n",(int)global); sceNetPoolDestroy(pool); puts("TLS_PROBE done success=0"); return 1; }
  int example = request("example.com", "https://example.com/", pool, 0); fflush(stdout);
  int invalid = request("example.com", "https://example.com/", pool, 1); fflush(stdout);
  int steam = request("api.steampowered.com", "https://api.steampowered.com/ISteamWebAPIUtil/GetServerInfo/v1/", pool, 0);
  curl_global_cleanup(); sceNetPoolDestroy(pool);
  printf("TLS_PROBE done success=%d\n", example && invalid && steam); fflush(stdout);
  return example && invalid && steam ? 0 : 1;
}
