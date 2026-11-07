// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_host_config.hpp"
#include "runtime/artifact_hash.h"
#include "steam/install_volumes.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace { int root_calls = 0; bool permit_root = true; }
namespace playden::ps5 {
bool WithRoot(const std::function<bool()>& work) { ++root_calls; return permit_root && work(); }
std::shared_ptr<void> HoldEffectiveUid() { return {}; }
std::shared_ptr<void> TryHoldEffectiveUid() { return {}; }
}
namespace playden::runtime {
const std::vector<std::filesystem::path>& PrivateProfilePaths() { static const std::vector<std::filesystem::path> value; return value; }
std::vector<VcRedist> MicrosoftVcRedists() { return {}; }
std::optional<std::string> HashArtifactFile(const std::filesystem::path&) { return "test"; }
std::optional<std::string> HashArtifactBytes(std::string_view) { return "test"; }
}
namespace playden::steam {
std::optional<VolumeSpace> QueryVolumeSpace(const std::string&, std::uint64_t) { return VolumeSpace{100, 50}; }
bool VolumeSpaceSupported() { return true; }
}
struct Titles : ww::steam_integration::CoordinatedTitleList {
  std::string value = "FAKE78003\n";
  std::optional<std::string> Read() override { return value; }
  ww::steam_integration::TitleListReplace ReplaceIfUnchanged(const std::string& old, const std::string& next) override {
    if (value != old) return ww::steam_integration::TitleListReplace::changed;
    value = next; return ww::steam_integration::TitleListReplace::written;
  }
};
int main() {
  using namespace playden::platform;
  char path[] = "/tmp/wine11-host-config-XXXXXX"; assert(mkdtemp(path));
  const auto state_path = std::filesystem::path(path) / "wine11-steam";
  assert(std::filesystem::create_directory(state_path));
  assert(chmod(state_path.c_str(), 0700) == 0);
  int fd = open((state_path / "wine11-state.lock").c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600); assert(fd >= 0); close(fd);
  auto state = ManagedDirectory::Open(state_path, DirectoryPolicy::kPrivate); assert(state);
  auto lease = ManagedFileLock::Acquire(*state, "wine11-state.lock", LockMode::kPublisher); assert(lease);
  Titles titles;
  ww::steam_integration::NativeHostInputs inputs{*state, *lease, "/app0/runtime", titles, {}, {}, {}};
  assert(!ww::steam_integration::BuildNativeHostConfig(inputs));
  inputs.registry = [](const std::string&, const std::string&) -> std::optional<std::string> { return "ok"; };
  inputs.admit_kit = [](std::string*) -> std::optional<playden::homebrew::RuntimeKit> { return std::nullopt; };
  inputs.launch_title = [](const std::string&, const std::vector<std::string>&) { return playden::ps5::TitleResult{}; };
  inputs.fetch_redist = [](const std::string&, std::uint64_t, std::uint64_t, const std::atomic<bool>&, std::string*) -> std::optional<std::string> { return "data"; };
  auto config = ww::steam_integration::BuildNativeHostConfig(inputs); assert(config);
  assert(!config->unregister_title);
  assert(config->boundary == state_path && config->state_root == state_path / "runtime-state");
  assert(config->runtime_dir == "/app0/runtime" && config->launch_title && config->registry_transport);
  std::atomic<bool> cancel{false}; std::string fetch_error;
  assert(config->fetch_range("https://example.test", 0, 4, cancel, &fetch_error) == "data");
  std::string error; assert(config->write_title_allow_list("FAKE78099\n", &error));
  assert(root_calls == 1);
  assert(titles.value == "FAKE78003\nFAKE78099\n");
  permit_root = false;
  assert(!config->write_title_allow_list("FAKE78098\n", &error));
  assert(titles.value == "FAKE78003\nFAKE78099\n");
  permit_root = true;
  int removals = 0;
  inputs.unregister_title = [&removals](const std::string& title, const std::string&) {
    assert(title == "FAKE78008"); ++removals;
    return playden::homebrew::TitleRemoverResult{true, 0, "accepted"};
  };
  config = ww::steam_integration::BuildNativeHostConfig(inputs); assert(config && config->unregister_title);
  assert(config->unregister_title("FAKE78008", "nonce").sent && removals == 1);
  inputs.packaged_runtime_dir = "/app0/../runtime";
  assert(!ww::steam_integration::BuildNativeHostConfig(inputs));
  assert(rename((state_path / "wine11-state.lock").c_str(), (state_path / "old.lock").c_str()) == 0);
  fd = open((state_path / "wine11-state.lock").c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600); assert(fd >= 0); close(fd);
  assert(!config->write_title_allow_list("FAKE78098\n", &error));
  assert(!config->registry_transport("/test", ""));
  assert(!config->fetch_range("https://example.test", 0, 4, cancel, &fetch_error));
  assert(!config->unregister_title("FAKE78008", "nonce").sent && removals == 1);
  assert(!config->launch_title("FAKE78099", {}).ok);
  assert(titles.value == "FAKE78003\nFAKE78099\n");
  std::filesystem::remove_all(path);
  std::puts("PASS: game host configuration uses the leased Wine 11 state and preserves existing titles");
}
