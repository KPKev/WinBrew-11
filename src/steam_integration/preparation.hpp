// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include <memory>
#include <string>
#include <thread>
namespace ww::steam_integration {
class PreparationHost {
 public:
  virtual ~PreparationHost() = default;
  virtual std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const = 0;
  virtual void Prepare(playden::homebrew::RuntimePrepareRequest request) = 0;
};
enum class PreparationState { idle, waiting, awaiting_registration, ready, failed };
struct PreparationView { PreparationState state = PreparationState::idle; std::string message; };
// Application-thread coordinator for a player-requested preparation. The
// caller supplies a currently owned, installed game's identity and metadata.
class Preparation {
 public:
  explicit Preparation(PreparationHost& host);
  bool Request(playden::homebrew::RuntimePrepareRequest request, std::uint64_t account, std::uint64_t generation);
  void Tick(std::uint64_t account, std::uint64_t generation);
  bool Busy() const { return view_.state == PreparationState::waiting; }
  const PreparationView& view() const { return view_; }
  std::uint32_t app() const { return app_; }
  bool NeedsRegistration(std::uint32_t app) const;
 private:
  PreparationHost& host_;
  std::thread::id thread_;
  PreparationView view_;
  std::uint32_t app_ = 0;
  std::uint64_t account_ = 0, generation_ = 0, before_ = 0;
  std::string root_;
  bool account_changed_ = false;
};
class PlaydenPreparationHost final : public PreparationHost {
 public:
  explicit PlaydenPreparationHost(playden::homebrew::RuntimeHost& host) : host_(host) {}
  std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const override;
  void Prepare(playden::homebrew::RuntimePrepareRequest request) override;
 private:
  playden::homebrew::RuntimeHost& host_;
};
}
