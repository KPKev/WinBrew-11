// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_absence.hpp"
namespace ww::steam_integration {
TitleObservation VerifyTitleAbsent(const std::string& title, const TitleAbsenceInputs& inputs) {
  if (title.size() != 9 || title.compare(0, 6, "FAKE78") != 0) return TitleObservation::unknown;
  for (std::size_t i = 6; i < title.size(); ++i) if (title[i] < '0' || title[i] > '9') return TitleObservation::unknown;
  if (!inputs.runtime_record || !inputs.database || !inputs.shadowmount || !inputs.homebrew_source || !inputs.native_install)
    return TitleObservation::unknown;
  bool unknown = false;
  for (const auto* callback : {&inputs.runtime_record, &inputs.database, &inputs.shadowmount, &inputs.homebrew_source, &inputs.native_install}) {
    const auto observed = (*callback)(title);
    if (observed == TitleObservation::present) return TitleObservation::present;
    if (observed != TitleObservation::absent) unknown = true;
  }
  return unknown ? TitleObservation::unknown : TitleObservation::absent;
}
}
