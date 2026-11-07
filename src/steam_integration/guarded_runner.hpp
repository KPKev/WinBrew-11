// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "installs/install_service.h"
#include "network_traffic.hpp"
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "platform/ps5/title_allow_list.h"
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
#include "install_generation.hpp"
#include "platform/random_id.h"
#endif
namespace ww::steam_integration {
// This guard must exist before InstallService starts its worker: recovery can
// resume downloads and removals during service construction.
class GuardedInstallRunner final : public playden::installs::InstallRunner {
 public:
  GuardedInstallRunner(playden::platform::ManagedDirectory& directory,
                       playden::platform::ManagedFileLock& lease,
                       playden::installs::InstallRunner& delegate, traffic::Registry* traffic = nullptr)
      : directory_(directory), lease_(lease), delegate_(delegate), traffic_(traffic) {}
  Result Install(const playden::GameId& id, const std::string& root, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>& progress,
      const std::function<void(playden::installs::InstallStep)>& step) override {
    // The effective UID is process-wide. Keep it stable for the entire
    // transfer, including files created by the delegate.
    const auto uid = playden::ps5::HoldEffectiveUid();
    auto parsed = admitted(root);
    if (!parsed || *parsed != id) return {false, false, "WinBrew 11 game storage is unavailable or not exclusively held.", 0, false, {}};
#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
    const auto generation = playden::RandomUuid();
    if (!generation || !install_generation::Valid(*generation)) return {false, false, "Could not identify this download attempt.", 0, false, {}};
    install_generation::Scope generation_scope(*generation);
#endif
    if (!traffic_) return delegate_.Install(id, root, cancel, progress, step);
    auto counter = traffic_->begin(id.steam_app().value_or(0));
    struct End { traffic::Registry& registry; std::shared_ptr<traffic::Counter> counter; ~End() { registry.end(counter); } } end{*traffic_, counter};
    traffic::Scope scope(counter);
    return delegate_.Install(id, root, cancel, progress, step);
  }
  std::string Remove(const std::string& root) override {
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
    (void)root;
    return "Removing game files is unavailable.";
#else
    const auto uid = playden::ps5::HoldEffectiveUid();
    if (!admitted(root)) return "WinBrew 11 game storage is unavailable or not exclusively held.";
    return delegate_.Remove(root);
#endif
  }
  void Forget(const playden::GameId& id) override {
    const auto uid = playden::ps5::HoldEffectiveUid();
    if (id.store == playden::Store::kSteam && id.valid() && directory_.Validate() &&
        lease_.Validate(directory_, "wine11-install.lock")) delegate_.Forget(id);
  }
 private:
  std::optional<playden::GameId> admitted(const std::string& root) const {
    const auto prefix = directory_.path().string() + "/";
    if (root.size() <= prefix.size() || root.compare(0, prefix.size(), prefix) != 0 ||
        root.find('/', prefix.size()) != std::string::npos || !directory_.Validate() ||
        !lease_.Validate(directory_, "wine11-install.lock")) return std::nullopt;
    auto id = playden::ParseKey(std::string_view(root).substr(prefix.size()));
    if (!id || id->store != playden::Store::kSteam || playden::ToKey(*id) != root.substr(prefix.size()))
      return std::nullopt;
    return id;
  }
  playden::platform::ManagedDirectory& directory_;
  playden::platform::ManagedFileLock& lease_;
  playden::installs::InstallRunner& delegate_;
  traffic::Registry* traffic_;
};
}
