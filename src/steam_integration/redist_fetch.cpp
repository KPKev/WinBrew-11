// SPDX-License-Identifier: GPL-3.0-or-later
#include "redist_fetch.hpp"
#include <cctype>
#include <limits>
#include <memory>
#include <utility>
namespace ww::steam_integration {
std::optional<std::string> FetchRedistRange(const RedistTransport& transport, const std::string& url,
    std::uint64_t offset, std::uint64_t length, const std::atomic<bool>& cancel, std::string* error) {
  using namespace playden::steam;
  if (error) error->clear();
  auto fail = [error](const char* message) -> std::optional<std::string> { if (error) *error = message; return {}; };
  if (!transport || url.rfind("https://", 0) != 0 || !length || length >= kMaxHttpResponseBytes ||
      offset > std::numeric_limits<std::uint64_t>::max() - (length - 1)) return fail("Invalid dependency download request.");
  std::string target = url;
  for (int hop = 0; hop <= 5; ++hop) {
    if (cancel.load()) return fail("Dependency download canceled.");
    HttpRequest request; request.url = target; request.timeout_ms = kMaxHttpTimeoutMs;
    request.max_response_bytes = static_cast<std::size_t>(length) + 1;
    request.headers = {{"User-Agent", "Playden"}, {"Range", "bytes=" + std::to_string(offset) + "-" + std::to_string(offset + length - 1)}};
    const auto response = transport(request, cancel);
    if (response.status_code == 301 || response.status_code == 302 || response.status_code == 303 ||
        response.status_code == 307 || response.status_code == 308) {
      std::string location;
      for (const auto& [name, value] : response.headers) {
        std::string lower = name;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == "location") { if (!location.empty()) return fail("Ambiguous dependency download redirect."); location = value; }
      }
      if (!response.ok() || location.rfind("https://", 0) != 0) return fail("Unsafe dependency download redirect.");
      target = std::move(location); continue;
    }
    if (!response.ok() || (response.status_code != 206 && !(response.status_code == 200 && offset == 0)) ||
        response.body.size() != length) return fail("Dependency download did not return the requested data.");
    return std::string(response.body.begin(), response.body.end());
  }
  return fail("Too many dependency download redirects.");
}
RedistFetch MakeRedistFetch(std::string ca) {
  if (ca.empty() || ca.front() != '/') return {};
  auto client = std::make_shared<std::unique_ptr<playden::steam::HttpClient>>();
  return [client, ca = std::move(ca)](const std::string& url, std::uint64_t offset, std::uint64_t length,
                                     const std::atomic<bool>& cancel, std::string* error) {
    if (!*client) *client = std::make_unique<playden::steam::HttpClient>(ca);
    return FetchRedistRange([client](const auto& request, const auto& flag) { return (*client)->Perform(request, flag); },
                            url, offset, length, cancel, error);
  };
}
}
