// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
namespace ww::steam_integration {
struct RememberView { bool enabled = false, busy = false, failed = false; };
// The callback updates the real session repository on a worker. The owner
// keeps its repository and vault alive until this object is destroyed.
class RememberSetting {
 public:
  using Change = std::function<bool(bool)>;
  RememberSetting(bool initial, Change change) : change_(std::move(change)) { view_.enabled = initial; }
  ~RememberSetting();
  bool Set(bool enabled);
  RememberView View() const;
 private:
  Change change_;
  mutable std::mutex mutex_;
  RememberView view_;
  std::thread worker_;
};
}
