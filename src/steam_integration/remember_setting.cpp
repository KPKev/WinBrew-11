// SPDX-License-Identifier: GPL-3.0-or-later
#include "remember_setting.hpp"
#include <utility>
namespace ww::steam_integration {
RememberSetting::~RememberSetting() { if (worker_.joinable()) worker_.join(); }
bool RememberSetting::Set(bool enabled) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (view_.busy || !change_) return false;
    if (view_.enabled == enabled && !view_.failed) return true;
    view_.busy = true; view_.failed = false;
  }
  if (worker_.joinable()) worker_.join();
  try {
    worker_ = std::thread([this, enabled] {
      bool ok = false;
      try { ok = change_(enabled); } catch (...) { }
      std::lock_guard<std::mutex> lock(mutex_);
      if (ok) view_.enabled = enabled;
      view_.failed = !ok; view_.busy = false;
    });
  } catch (...) {
    std::lock_guard<std::mutex> lock(mutex_); view_.busy = false; view_.failed = true; return false;
  }
  return true;
}
RememberView RememberSetting::View() const { std::lock_guard<std::mutex> lock(mutex_); return view_; }
}
