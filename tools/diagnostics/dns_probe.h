/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef WINE11_DNS_PROBE_H
#define WINE11_DNS_PROBE_H
#include <stddef.h>
#include <stdint.h>
/* Fixed public diagnostic names only. No user URL, account, or setting input. */
struct wine11_dns_ops {
  int (*init)(void);
  int (*pool_create)(const char *, int, int);
  int (*pool_destroy)(int);
  int (*resolver_create)(const char *, int, int);
  int (*resolver_destroy)(int);
  int (*lookup4)(int, const char *, void *, int, int, int);
  int (*lookup6)(int, const char *, void *, int, int, int);
  int (*resolver_error)(int, int *);
  const char *(*format)(int, const void *, char *, unsigned);
  uint64_t (*microseconds)(void);
  void (*progress)(const char *, const char *, int);
};
struct wine11_dns_result {
  const char *stage, *host;
  int family, code, resolver_error, resolver_error_code;
  uint64_t elapsed_us;
  char address[48];
};
typedef void (*wine11_dns_sink)(const struct wine11_dns_result *);
static void wine11_dns_run(const struct wine11_dns_ops *ops, wine11_dns_sink sink) {
  static const char *const hosts[] = {"accounts.google.com", "example.com"};
  struct wine11_dns_result result = {0};
  if (ops->progress) ops->progress("net_init_begin", 0, 0);
  int code = ops->init();
  if (code < 0) { result.stage = "net_init"; result.code = code; sink(&result); return; }
  if (ops->progress) ops->progress("pool_create_begin", 0, 0);
  const int pool = ops->pool_create("wine11_dns_probe", 16 * 1024, 0);
  if (pool < 0) { result.stage = "pool_create"; result.code = pool; sink(&result); return; }
  int stop = 0;
  for (int host = 0; host < 2 && !stop; ++host) {
    for (int pass = 0; pass < 2; ++pass) {
      result = (struct wine11_dns_result){0};
      result.host = hosts[host]; result.family = pass ? 28 : 2;
      if (ops->progress) ops->progress("resolver_create_begin", hosts[host], result.family);
      const int resolver = ops->resolver_create("wine11_dns_probe", pool, 0);
      if (resolver < 0) {
        result.stage = "resolver_create"; result.code = resolver; sink(&result); continue;
      }
      union { uint32_t ipv4; unsigned char ipv6[16]; } address = {0};
      if (ops->progress) ops->progress("lookup_begin", hosts[host], result.family);
      const uint64_t start = ops->microseconds();
      /* Native resolver timeout is microseconds. One attempt, no retry. */
      result.code = pass ? ops->lookup6(resolver, hosts[host], &address, 1000000, 0, 0)
                         : ops->lookup4(resolver, hosts[host], &address, 1000000, 0, 0);
      result.elapsed_us = ops->microseconds() - start;
      result.stage = "lookup";
      if (result.code < 0) {
        if (ops->progress) ops->progress("resolver_error_begin", hosts[host], result.family);
        result.resolver_error_code = ops->resolver_error(resolver, &result.resolver_error);
      }
      else if (!ops->format(result.family, &address, result.address, sizeof(result.address))) {
        result.stage = "format"; result.code = -1;
      }
      sink(&result);
      if (ops->progress) ops->progress("resolver_destroy_begin", hosts[host], result.family);
      code = ops->resolver_destroy(resolver);
      if (code < 0) {
        result.stage = "resolver_destroy"; result.code = code; result.address[0] = 0;
        sink(&result); stop = 1; break;
      }
    }
  }
  if (ops->progress) ops->progress("pool_destroy_begin", 0, 0);
  code = ops->pool_destroy(pool);
  if (code < 0) {
    result = (struct wine11_dns_result){0}; result.stage = "pool_destroy"; result.code = code; sink(&result);
  }
}
#endif
