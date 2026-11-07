// SPDX-License-Identifier: GPL-3.0-or-later
// Link anchor only: never run this function on a console. It makes the linker
// include the actual Playden game host and Wine 11's preparation adapters.
#include "homebrew/runtime_host.hpp"
#include "native_game_runtime.hpp"
#include "runtime.hpp"
#include "native_bridge.hpp"
#include "shell.hpp"
#include <utility>
namespace {
struct Titles : ww::steam_integration::CoordinatedTitleList {
  std::optional<std::string> Read() override { return std::nullopt; }
  ww::steam_integration::TitleListReplace ReplaceIfUnchanged(const std::string&, const std::string&) override {
    return ww::steam_integration::TitleListReplace::unavailable;
  }
};
}
extern "C" void wine11_game_host_link_probe() {
  playden::homebrew::RuntimeHostConfig config;
  playden::homebrew::RuntimeHost host(std::move(config));
  ww::steam_integration::NativeGameRuntime game(host, [](std::uint32_t, std::uint64_t, std::uint64_t) {
    return ww::steam_integration::CloudGateResult{ww::steam_integration::CloudGateState::blocked, "diagnostic"};
  });
  ww::steam_integration::Runtime steam("/not-a-runtime", "/not-a-certificate", false);
  (void)game.Attach(steam);
  (void)host.Start();
  host.Prepare({});
  host.Play(0);
  (void)host.Handoff();
  ww::Shell shell; Titles titles; ww::steam_integration::NativeBridge bridge(shell);
  bridge.StartGameHost("/not-a-package", titles,
    [](const std::string&, const std::string&) -> std::optional<std::string> { return std::nullopt; },
    [](const std::string&, const std::vector<std::string>&) { return playden::ps5::TitleResult{}; },
    [](std::uint32_t) -> std::optional<playden::steam::CloudLocation> { return std::nullopt; });
}
