// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "steam/backend/http_client.h"
#include <functional>
#include <optional>
namespace ww::steam_integration {
using RedistTransport = std::function<playden::steam::HttpResponse(const playden::steam::HttpRequest&, const std::atomic<bool>&)>;
using RedistFetch = std::function<std::optional<std::string>(const std::string&, std::uint64_t, std::uint64_t,
                                                              const std::atomic<bool>&, std::string*)>;
// The caller still verifies the complete redistributable against Playden's
// pinned SHA-256 before it is installed.
std::optional<std::string> FetchRedistRange(const RedistTransport&, const std::string&, std::uint64_t,
                                             std::uint64_t, const std::atomic<bool>&, std::string*);
RedistFetch MakeRedistFetch(std::string verified_ca_bundle);
}
