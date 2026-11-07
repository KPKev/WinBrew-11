// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_list_writer.hpp"
#include "title_list_merge.hpp"
namespace ww::steam_integration {
std::function<bool(const std::string&, std::string*)> MakeTitleListWriter(CoordinatedTitleList& list) {
  return [&list](const std::string& requested, std::string* error) {
    if (error) error->clear();
    auto fail = [error](const char* text) { if (error) *error = text; return false; };
    const auto existing = list.Read();
    if (!existing) return fail("The existing console title list could not be read safely.");
    const auto merged = MergeTitleLists(*existing, requested);
    if (!merged) return fail("The console title list is incomplete or invalid.");
    // Even an unchanged union must be checked atomically: another writer may
    // have removed a requested title after Read returned.
    switch (list.ReplaceIfUnchanged(*existing, *merged)) {
      case TitleListReplace::written: return true;
      case TitleListReplace::changed: return fail("The console title list changed during the update.");
      case TitleListReplace::unavailable: return fail("Coordinated console title-list writing is unavailable.");
    }
    return fail("The console title list could not be updated.");
  };
}
}
