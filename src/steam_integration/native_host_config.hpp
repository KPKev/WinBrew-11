// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "title_list_writer.hpp"
#include "redist_fetch.hpp"
#include <optional>
namespace ww::steam_integration {
using NativeTitleLauncher = std::function<playden::ps5::TitleResult(const std::string&, const std::vector<std::string>&)>;
// Platform-owned dependencies must remain alive until RuntimeHost stops.
struct NativeHostInputs {
  const playden::platform::ManagedDirectory& state;
  const playden::platform::ManagedFileLock& state_lease;
  std::string packaged_runtime_dir;
  CoordinatedTitleList& titles;
  playden::ps5::TitleRegistry::Transport registry;
  std::function<std::optional<playden::homebrew::RuntimeKit>(std::string*)> admit_kit;
  NativeTitleLauncher launch_title;
  // Optional until the host is built with Wine 11's private dependency cache.
  RedistFetch fetch_redist;
  // Optional until a pinned remover and its handoff have been admitted.
  std::function<playden::homebrew::TitleRemoverResult(const std::string&, const std::string&)> unregister_title;
};
// Does not start the host or perform publication. The caller must supply an
// immutable, verified package path and real console coordination callbacks.
std::optional<playden::homebrew::RuntimeHostConfig> BuildNativeHostConfig(const NativeHostInputs& inputs);
}
