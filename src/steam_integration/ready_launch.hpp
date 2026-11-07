// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include <functional>
#include <memory>
#include <string>
#include <thread>
namespace ww::steam_integration {
// RuntimeHost is the sole owner of publication, preflight and title handoff.
// This interface keeps its host-test double separate from the production adapter.
class ReadyLaunchHost {
 public:
  virtual ~ReadyLaunchHost() = default;
  virtual std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const = 0;
  virtual void Play(std::uint32_t app) = 0;
  // True means the host attempted a handoff, not that the native launch succeeded.
  virtual bool Handoff() = 0;
};
enum class CloudGateState { ready, checking, blocked };
struct CloudGateResult { CloudGateState state = CloudGateState::blocked; std::string message; };
using CloudGate = std::function<CloudGateResult(std::uint32_t, std::uint64_t, std::uint64_t)>;
using CloudBegin = std::function<bool(std::uint32_t, const std::string&, std::uint64_t, std::uint64_t, std::string*)>;
enum class ReadyLaunchState { idle, waiting, handoff, running, exited, failed };
struct ReadyLaunchView {
  ReadyLaunchState state = ReadyLaunchState::idle;
  std::uint32_t app = 0;
  std::string message;
};
// UI thread only and the sole Play requester for this RuntimeHost. The gate
// must be a nonblocking view of the current account's completed cloud-save
// preflight. Missing or uncertain evidence refuses Play.
class ReadyLaunch {
 public:
  ReadyLaunch(ReadyLaunchHost&, CloudGate, CloudBegin = {});
  bool Request(std::uint32_t app, const std::string& installed_root, std::uint64_t account_id, std::uint64_t account_generation);
  // Used after the player explicitly accepts an unsupported Cloud check.
  // The gate is still required to return ready for the same game and account.
  bool RequestUsingCompletedCloud(std::uint32_t app, const std::string& installed_root,
                                  std::uint64_t account_id, std::uint64_t account_generation);
  void Tick(std::uint64_t account_id, std::uint64_t account_generation);
  bool Busy() const;
  const ReadyLaunchView& view() const { return view_; }
 private:
  bool ApplicationThread() const;
  bool RequestImpl(std::uint32_t, const std::string&, std::uint64_t, std::uint64_t, bool begin_cloud);
  ReadyLaunchHost& host_;
  CloudGate cloud_;
  CloudBegin begin_cloud_;
  std::thread::id thread_;
  ReadyLaunchView view_;
  std::uint64_t plays_before_ = 0, account_id_ = 0, account_generation_ = 0;
  std::string installed_root_;
  bool submitted_ = false, handoff_attempted_ = false, cloud_pending_ = false;
};
// Thin production adapter: it never constructs or configures RuntimeHost.
class PlaydenReadyLaunchHost final : public ReadyLaunchHost {
 public:
  explicit PlaydenReadyLaunchHost(playden::homebrew::RuntimeHost& host) : host_(host) {}
  std::shared_ptr<const playden::homebrew::RuntimeSnapshot> Snapshot() const override;
  void Play(std::uint32_t app) override;
  bool Handoff() override;
 private:
  playden::homebrew::RuntimeHost& host_;
};
}
