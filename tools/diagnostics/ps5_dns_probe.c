/* SPDX-License-Identifier: GPL-3.0-or-later
 * Fixed-host read-only native DNS diagnostic. API signatures/time units follow
 * SharpProspero Interop/Net/Socket.cs and Platform/HostResolver.cs (GPL-3.0).
 * Uses only current resolver settings. It does not modify DNS or clear caches.
 */
#include "dns_probe.h"
#include <stdio.h>
#include <time.h>
extern int sceNetInit(void);
extern int sceNetPoolCreate(const char *, int, int);
extern int sceNetPoolDestroy(int);
extern int sceNetResolverCreate(const char *, int, int);
extern int sceNetResolverDestroy(int);
extern int sceNetResolverStartNtoa(int, const char *, void *, int, int, int);
extern int sceNetResolverStartNtoa6(int, const char *, void *, int, int, int);
extern int sceNetResolverGetError(int, int *);
extern const char *sceNetInetNtop(int, const void *, char *, unsigned);
static uint64_t microseconds(void) {
  struct timespec ts = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
static void progress(const char *stage, const char *host, int family) {
  printf("DNSPROBE phase=%s host=%s family=%s\n", stage, host ? host : "-",
         family == 2 ? "IPv4" : family == 28 ? "IPv6" : "-");
  fflush(stdout);
}
static void report(const struct wine11_dns_result *result) {
  printf("DNSPROBE stage=%s host=%s family=%s rc=0x%08x error_query=0x%08x resolver_error=0x%08x elapsed_ms=%llu address=%s\n",
         result->stage, result->host ? result->host : "-",
         result->family == 2 ? "IPv4" : result->family == 28 ? "IPv6" : "-",
         (unsigned)result->code, (unsigned)result->resolver_error_code,
         (unsigned)result->resolver_error,
         (unsigned long long)(result->elapsed_us / 1000), result->address);
  fflush(stdout);
}
int main(void) {
  const struct wine11_dns_ops ops = {
    sceNetInit, sceNetPoolCreate, sceNetPoolDestroy, sceNetResolverCreate,
    sceNetResolverDestroy, sceNetResolverStartNtoa, sceNetResolverStartNtoa6,
    sceNetResolverGetError, sceNetInetNtop, microseconds, progress
  };
  puts("DNSPROBE context=isolated_payload fixed_hosts=2 lookups=4 timeout_us=1000000 retries=0 writes=none");
  fflush(stdout);
  progress("probe_run_begin", 0, 0);
  wine11_dns_run(&ops, report);
  puts("DNSPROBE done");
  fflush(stdout);
  return 0;
}
