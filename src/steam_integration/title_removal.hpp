// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include <memory>
#include <optional>
#include <string>
#include <thread>
namespace ww::steam_integration {
class TitleRemovalHost {
 public:
  virtual ~TitleRemovalHost() = default;
  virtual std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const = 0;
  virtual void Remove(std::uint32_t app) = 0; // must preserve the bottle
};
enum class TitleRemovalState { idle, waiting, removed, failed };
class TitleRemoval {
 public:
  explicit TitleRemoval(TitleRemovalHost& host) : host_(host), thread_(std::this_thread::get_id()) {}
  bool CanBegin(std::uint32_t app, const std::string& expected_title) const;
  bool HostReady() const;
  std::optional<std::string> TitleFor(std::uint32_t app) const;
  bool Begin(std::uint32_t app, const std::string& expected_title);
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  bool CanRetryFailedRetire(std::uint32_t app, const std::string& expected_title) const;
  bool BeginRetryFailedRetire(std::uint32_t app, const std::string& expected_title);
#endif
  void Tick();
  TitleRemovalState state() const { return state_; }
  std::uint32_t app() const { return app_; }
  const std::string& title() const { return title_; }
  const std::string& message() const { return message_; }
 private:
  TitleRemovalHost& host_; std::thread::id thread_;
  TitleRemovalState state_ = TitleRemovalState::idle;
  std::uint32_t app_ = 0; std::uint64_t before_ = 0;
  std::string title_, message_;
};
class PlaydenTitleRemovalHost final : public TitleRemovalHost {
 public:
  explicit PlaydenTitleRemovalHost(playden::homebrew::RuntimeHost& host) : host_(host) {}
  std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const override;
  void Remove(std::uint32_t app) override;
 private:
  playden::homebrew::RuntimeHost& host_;
};
}
