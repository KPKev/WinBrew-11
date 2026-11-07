// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>
namespace ww::steam_integration {
inline bool ValidUninstallOperation(std::string_view operation) {
  if (operation.size() != 36) return false;
  if (operation[14] != '4' || (operation[19] != '8' && operation[19] != '9' && operation[19] != 'a' && operation[19] != 'b')) return false;
  for (std::size_t i = 0; i < operation.size(); ++i) {
    const char c = operation[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; }
    else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}
}
