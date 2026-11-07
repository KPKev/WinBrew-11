// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_title_provision.hpp"
#include "dedicated_title_list.hpp"
#include "platform/ps5/title_allow_list.h"
#include <unistd.h>
namespace ww::steam_integration {
NativeGameStartup::Provision MakeConsoleTitleProvision(int anchor) {
  if (anchor < 0) return {};
  int duplicate = dup(anchor);
  if (duplicate < 0) return {};
  auto held = std::shared_ptr<int>(new int(duplicate), [](int* fd) { close(*fd); delete fd; });
  return [held]() -> std::unique_ptr<CoordinatedTitleList> {
    std::unique_ptr<DedicatedTitleList> titles;
    const bool ok = playden::ps5::WithRoot([&] {
      auto data = playden::platform::ManagedDirectory::Open("/data", playden::platform::DirectoryPolicy::kApprovedTree);
      if (!data) return false;
      titles = ProvisionDedicatedTitleList(*data, *held);
      return titles != nullptr;
    });
    return ok ? std::move(titles) : nullptr;
  };
}
}
