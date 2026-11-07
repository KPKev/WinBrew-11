// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/redist_fetch.hpp"
#include <cassert>
#include <limits>
int main() {
  using namespace ww::steam_integration;
  using namespace playden::steam;
  std::atomic<bool> cancel{false}; std::string error; int calls = 0;
  RedistTransport transport = [&](const HttpRequest& request, const std::atomic<bool>&) {
    ++calls; assert(!request.allow_plain_http && request.headers[1].second == "bytes=10-12");
    HttpResponse response;
    if (calls == 1) { response.status_code = 302; response.headers = {{"Location", "https://example.test/file"}}; }
    else { response.status_code = 206; response.body = {'a', 'b', 'c'}; }
    return response;
  };
  auto result = FetchRedistRange(transport, "https://example.test/start", 10, 3, cancel, &error);
  assert(result && *result == "abc" && calls == 2 && error.empty());
  assert(!FetchRedistRange(transport, "http://example.test", 0, 3, cancel, &error));
  assert(!FetchRedistRange(transport, "https://example.test", std::numeric_limits<std::uint64_t>::max(), 2, cancel, &error));
  RedistTransport unsafe = [](const HttpRequest&, const std::atomic<bool>&) {
    HttpResponse response; response.status_code = 302; response.headers = {{"location", "http://example.test"}}; return response;
  };
  assert(!FetchRedistRange(unsafe, "https://example.test", 0, 3, cancel, &error));
  RedistTransport short_response = [](const HttpRequest&, const std::atomic<bool>&) {
    HttpResponse response; response.status_code = 206; response.body = {'x'}; return response;
  };
  assert(!FetchRedistRange(short_response, "https://example.test", 0, 3, cancel, &error));
  cancel = true; calls = 0;
  assert(!FetchRedistRange(transport, "https://example.test", 10, 3, cancel, &error) && calls == 0);
}
