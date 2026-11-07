// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
#include <utility>
namespace ww::steam_integration::install_generation {
inline bool Valid(std::string_view id) {
  if (id.size() != 36) return false;
  if (id[14] != '4' || (id[19] != '8' && id[19] != '9' && id[19] != 'a' && id[19] != 'b')) return false;
  for (std::size_t i = 0; i < id.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (id[i] != '-') return false; }
    else if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
  }
  return true;
}
class Scope;
// Trivially destructible TLS; the stack-owned scope restores any outer scope.
inline thread_local Scope* current_scope = nullptr;
class Scope {
 public:
  explicit Scope(std::string id) : previous_(current_scope), id_(std::move(id)) { current_scope = this; }
  ~Scope() { current_scope = previous_; }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  const std::string& id() const { return id_; }
 private:
  Scope* previous_; std::string id_;
};
inline std::string Current() { return current_scope ? current_scope->id() : std::string(); }
}
