// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <utility>
#include "uninstall_identity.hpp"

namespace vapor_cleaner {
inline constexpr char kRequest[] = ".vapor-cleanup-request.v1";
inline constexpr char kResult[] = ".vapor-cleanup-result.v1";
struct Request {
  std::uint32_t app = 0;
  std::string operation;
  std::uint64_t device = 0, inode = 0;
  std::string title;
};
inline bool ValidTitle(const std::string& title) {
  if (title.size() != 9 || title.compare(0, 6, "FAKE78")) return false;
  for (std::size_t i = 6; i < title.size(); ++i)
    if (title[i] < '0' || title[i] > '9') return false;
  return true;
}
inline std::string Encode(const Request& r) {
  if (!r.app || !r.inode || !ww::steam_integration::ValidUninstallOperation(r.operation) || !ValidTitle(r.title)) return {};
  return "1 " + std::to_string(r.app) + " " + r.operation + " " + std::to_string(r.device) + " " +
      std::to_string(r.inode) + " " + r.title + "\n";
}
inline bool Decode(const std::string& text, Request& r) {
  if (text.empty() || text.back() != '\n') return false;
  std::string_view rest(text.data(), text.size() - 1);
  std::array<std::string_view, 6> fields{};
  for (std::size_t i = 0; i < fields.size(); ++i) {
    const auto pos = rest.find(' ');
    if ((i + 1 == fields.size()) != (pos == std::string_view::npos)) return false;
    fields[i] = rest.substr(0, pos);
    if (fields[i].empty()) return false;
    if (pos != std::string_view::npos) rest.remove_prefix(pos + 1);
  }
  const auto number = [](std::string_view field, auto& value) {
    const auto result = std::from_chars(field.data(), field.data() + field.size(), value);
    return result.ec == std::errc{} && result.ptr == field.data() + field.size();
  };
  Request parsed; parsed.operation = fields[2]; parsed.title = fields[5];
  if (fields[0] != "1" || !number(fields[1], parsed.app) || !number(fields[3], parsed.device) ||
      !number(fields[4], parsed.inode)) return false;
  r = std::move(parsed);
  return Encode(r) == text;
}
}  // namespace vapor_cleaner
