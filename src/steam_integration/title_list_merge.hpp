// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace ww::steam_integration {
// Produces a union of two complete title lists. Refuses malformed or oversized
// input rather than silently discarding a title. This is a pure operation;
// callers still need coordinated, verified filesystem publication.
std::optional<std::string> MergeTitleLists(std::string_view existing, std::string_view requested);
}
