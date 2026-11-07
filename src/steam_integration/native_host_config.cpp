// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_host_config.hpp"
#include "platform/ps5/title_allow_list.h"
#include "runtime/artifact_hash.h"
#include "steam/install_volumes.h"
#include <filesystem>
#include <utility>
namespace ww::steam_integration {
std::optional<playden::homebrew::RuntimeHostConfig> BuildNativeHostConfig(const NativeHostInputs& inputs) {
  namespace fs = std::filesystem;
  namespace rt = playden::runtime;
  using playden::platform::DirectoryPolicy;
  const auto& root = inputs.state.path();
  const fs::path runtime_dir(inputs.packaged_runtime_dir);
  if (inputs.state.policy() != DirectoryPolicy::kPrivate || !root.is_absolute() ||
      root.filename() != "wine11-steam" || !inputs.state.Validate() ||
      !inputs.state_lease.Validate(inputs.state, "wine11-state.lock") ||
      !runtime_dir.is_absolute() || runtime_dir != runtime_dir.lexically_normal() ||
      !inputs.registry || !inputs.admit_kit || !inputs.launch_title) return std::nullopt;
  playden::homebrew::RuntimeHostConfig config;
  const auto* state = &inputs.state;
  const auto* lease = &inputs.state_lease;
  auto valid_state = [state, lease] {
    return state->Validate() && lease->Validate(*state, "wine11-state.lock");
  };
  config.boundary = root;
  config.state_root = root / "runtime-state";
  config.runtime_dir = runtime_dir.string();
  config.registry_transport = [valid_state, registry = inputs.registry](const std::string& route, const std::string& body) {
    return valid_state() ? registry(route, body) : std::optional<std::string>{};
  };
  config.admit_kit = [valid_state, admit = inputs.admit_kit](std::string* reason) {
    if (valid_state()) return admit(reason);
    if (reason) *reason = "WinBrew 11's game state is unavailable.";
    return std::optional<playden::homebrew::RuntimeKit>{};
  };
  config.launch_title = [valid_state, launch = inputs.launch_title](const std::string& title, const std::vector<std::string>& args) {
    return valid_state() ? launch(title, args) : playden::ps5::TitleResult{false, 0, "WinBrew 11 game state unavailable"};
  };
  if (inputs.unregister_title) {
    config.unregister_title = [valid_state, remove = inputs.unregister_title](const std::string& title, const std::string& nonce) {
      if (!valid_state()) return playden::homebrew::TitleRemoverResult{false, std::nullopt, "Game state is unavailable."};
      return remove(title, nonce);
    };
  }
  if (inputs.fetch_redist) {
    config.fetch_range = [valid_state, fetch = inputs.fetch_redist](const std::string& url, std::uint64_t offset,
        std::uint64_t length, const std::atomic<bool>& cancel, std::string* error) -> std::optional<std::string> {
      if (valid_state()) return fetch(url, offset, length, cancel, error);
      if (error) *error = "WinBrew 11's game state is unavailable.";
      return {};
    };
  }
  config.write_title_allow_list = [valid_state, write = MakeTitleListWriter(inputs.titles)](const std::string& value, std::string* error) {
    // The WinBrew 11 state belongs to the app user, while the dedicated title
    // list must be root-owned for its helper. Check the user lease first,
    // then access the title list under Playden's coordinated UID guard.
    if (valid_state()) return playden::ps5::WithRoot([&] { return write(value, error); });
    if (error) *error = "WinBrew 11's game state is unavailable.";
    return false;
  };
  config.package_operations.hash_file = rt::HashArtifactFile;
  config.package_operations.hash_bytes = rt::HashArtifactBytes;
  config.package_operations.free_bytes = [](const fs::path& path) -> std::optional<std::uint64_t> {
    const auto space = playden::steam::QueryVolumeSpace(path.string(), playden::steam::kConsoleSystemReserve);
    return space ? std::optional<std::uint64_t>(space->free_bytes) : std::nullopt;
  };
  config.package_operations.free_space_unverifiable = !playden::steam::VolumeSpaceSupported();
  config.package_operations.validate_bottle = [](const fs::path& bottle) {
    std::error_code error;
    for (const char* name : {"system.reg", "user.reg", "userdef.reg"})
      if (!fs::is_regular_file(bottle / name, error)) return false;
    return true;
  };
  config.as_root = [valid_state](const std::function<bool()>& work) { return valid_state() && playden::ps5::WithRoot(work); };
  config.hold_effective_uid = playden::ps5::HoldEffectiveUid;
  config.try_hold_effective_uid = [](bool* busy) {
    auto held = playden::ps5::TryHoldEffectiveUid(); *busy = !held; return held;
  };
  return config;
}
}
