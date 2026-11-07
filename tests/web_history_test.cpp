// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/web_history.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
int main() {
  char origin[ww::WebHistory::OriginBytes]{};
  assert(ww::WebHistory::origin("https://Example.COM:443/private/token?code=secret#access_token=secret", origin, sizeof(origin)));
  assert(!std::strcmp(origin, "https://example.com"));
  assert(ww::WebHistory::origin("http://[::1]:8080/private", origin, sizeof(origin)));
  assert(!std::strcmp(origin, "http://[::1]:8080"));
  for (const char *bad : {"https://user:password@example.com", "file:///private", "https://good.com\\@evil.com", "https://example.com:65536", "https://example.com:", "https://bad..host/", "https://-bad.com/", "https://example.com/\nsecret"})
    assert(!ww::WebHistory::origin(bad, origin, sizeof(origin)));
  std::filesystem::create_directories("build/web-history-tests");
  char path[256];
  std::snprintf(path, sizeof(path), "build/web-history-tests/%d.txt", int(getpid()));
  std::remove(path);
  ww::WebHistory history;
  assert(history.load(path) && !history.count && !history.error);
  assert(history.remember("https://www.google.com/search?q=sensitive#fragment"));
  assert(history.count == 1 && !history.dirty && !history.error);
  ww::WebHistory reopened;
  assert(reopened.load(path) && reopened.count == 1);
  assert(!std::strcmp(reopened.sites[0], "https://www.google.com"));
  for (int i = 0; i < 25; ++i) {
    char site[80]; std::snprintf(site, sizeof(site), "https://site%d.example/page?token=secret", i);
    assert(history.remember(site));
  }
  assert(history.count == 20 && !std::strcmp(history.sites[0], "https://site24.example"));
  assert(history.remember("https://site10.example/other"));
  assert(history.count == 20 && !std::strcmp(history.sites[0], "https://site10.example"));
  const int count = history.count;
  assert(!history.remember("https://user:password@example.com") && history.count == count);
  // Missing parent: session changes stay visible, disk data survives, retry is honest.
  char original_path[1024]; std::snprintf(original_path, sizeof(original_path), "%s", history.path);
  std::snprintf(history.path, sizeof(history.path), "%s/missing.txt", path);
  assert(!history.remember("https://new.example/secret") && history.error && history.dirty);
  assert(!history.remember("https://new.example/secret") && history.error && history.dirty);
  assert(reopened.load(original_path) && reopened.count == 20 && !std::strcmp(reopened.sites[0], "https://site10.example"));
  std::snprintf(history.path, sizeof(history.path), "%s", original_path);
  assert(history.save() && !history.dirty && !history.error);
  assert(history.clear() && !history.count && !history.dirty);
  assert(reopened.load(path) && !reopened.count);
  // A hand-edited file cannot smuggle userinfo/query/path data into stored sites.
  FILE *f = std::fopen(path, "wb"); assert(f);
  std::fputs("WINE11_RECENT_SITES 1\nhttps://user:password@example.com\n", f); std::fclose(f);
  assert(!reopened.load(path) && reopened.error == EINVAL && !reopened.count);
  std::remove(path);
  std::puts("PASS: recent-site privacy, bounded recency, restart, clear and save failure recovery");
}
