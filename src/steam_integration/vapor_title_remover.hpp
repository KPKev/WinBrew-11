// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include <functional>
#include <string>
namespace ww::steam_integration {
inline constexpr char kVaporTitleRemoverPath[] = "/data/homebrew/PPSA77382/vapor-title-remover.elf";
// Hash the helper through the mounted application directory. The final file
// is opened without following links; the exact sent bytes are checked again.
bool VerifyVaporTitleRemover(const std::string& payload_path, std::string* reason = nullptr);
// Uses Vapor's own request directory and accepts only the reviewed payload
// bytes. A request already present is never overwritten or retried blindly.
std::function<playden::homebrew::TitleRemoverResult(const std::string&, const std::string&)>
MakeVaporTitleRemover(const std::string& payload_path);
}
