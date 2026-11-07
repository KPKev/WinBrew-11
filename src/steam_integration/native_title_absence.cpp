// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_title_absence.hpp"
#include "platform/ps5/console_services.h"
#include "platform/ps5/title_database.h"
#include "platform/ps5/title_registry.h"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <sys/stat.h>
namespace ww::steam_integration {
namespace {
TitleObservation Path(const std::string& parent, const std::string& title) {
  TitleObservation result = TitleObservation::unknown;
  const bool ok = playden::ps5::WithRoot([&] {
    struct stat st{};
    if (lstat(parent.c_str(), &st) || !S_ISDIR(st.st_mode)) return false;
    errno = 0;
    if (lstat((parent + "/" + title).c_str(), &st) == 0) result = TitleObservation::present;
    else if (errno == ENOENT) result = TitleObservation::absent;
    return result != TitleObservation::unknown;
  });
  return ok ? result : TitleObservation::unknown;
}
}
TitleObservation VerifyNativeTitleAbsent(playden::homebrew::RuntimeHost& host, const std::string& title) {
  TitleAbsenceInputs inputs;
  inputs.runtime_record = [&host](const std::string& id) {
    const auto state = host.Shared();
    if (!state || state->status != playden::homebrew::RuntimeHostStatus::kReady || !state->installs_known)
      return TitleObservation::unknown;
    for (const auto& item : state->installs) if (item.title_id == id) return TitleObservation::present;
    return TitleObservation::absent;
  };
  inputs.database = [](const std::string& id) {
    const auto state = playden::ps5::ReadNativeTitleDatabase(id).inventory;
    return state == playden::runtime::TitleAvailability::kFree ? TitleObservation::absent :
           state == playden::runtime::TitleAvailability::kOccupied ? TitleObservation::present : TitleObservation::unknown;
  };
  inputs.shadowmount = [](const std::string& id) {
    playden::ps5::TitleRegistry registry(playden::ps5::ShadowMountPost);
    const auto state = registry.Query(id).state;
    return state == playden::ps5::RegistrationState::kAbsent ? TitleObservation::absent :
           state == playden::ps5::RegistrationState::kUnavailable ? TitleObservation::unknown : TitleObservation::present;
  };
  inputs.homebrew_source = [](const std::string& id) { return Path("/data/homebrew", id); };
  inputs.native_install = [](const std::string& id) { return Path("/user/app", id); };
  return VerifyTitleAbsent(title, inputs);
}
}
