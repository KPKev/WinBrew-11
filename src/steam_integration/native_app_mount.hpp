// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
namespace ww::steam_integration {
// The anchor must be opened on the immutable /app0 mount before root changes.
// Finds the same mount afterward and builds the packaged CA path.
bool ResolvePackagedCa(int anchor, const char* const* roots, std::size_t count,
                       char* output, std::size_t capacity);
}
