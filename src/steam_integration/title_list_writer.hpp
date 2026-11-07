// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <optional>
#include <string>
namespace ww::steam_integration {
enum class TitleListReplace { written, changed, unavailable };
// The native implementation must serialize against every writer and atomically
// replace only if the observed bytes are still current. The updater does not
// claim that a plain read followed by rename provides this guarantee.
class CoordinatedTitleList {
 public:
  virtual ~CoordinatedTitleList() = default;
  virtual std::optional<std::string> Read() = 0;
  // When expected == replacement, still verify the observed bytes atomically;
  // an implementation may avoid rewriting the file in that case.
  virtual TitleListReplace ReplaceIfUnchanged(const std::string& expected, const std::string& replacement) = 0;
};
// Suitable for RuntimeHostConfig::write_title_allow_list. The list object must
// outlive the callback; errors stop preparation without retrying the write.
std::function<bool(const std::string&, std::string*)> MakeTitleListWriter(CoordinatedTitleList& list);
}
