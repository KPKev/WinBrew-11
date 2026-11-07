// SPDX-License-Identifier: GPL-3.0-or-later
// PS5 launch ABI and provider resolution follow Playden's GPL-3.0-or-later
// src/platform/ps5/title_runtime.{h,cpp}. See THIRD_PARTY_NOTICES.md.
#pragma once
#include <cstddef>
#include <cstdint>
namespace ww {
struct LaunchParameters {
  uint32_t size, user, options;
  uint64_t crash_report;
  uint32_t check_flag;
};
static_assert(sizeof(LaunchParameters) == 32 &&
              offsetof(LaunchParameters, check_flag) == 24);
struct TitleApi {
  int (*initialize)() = nullptr;
  int (*user)(uint32_t *) = nullptr;
  uint32_t (*launch)(const char *, const char **, LaunchParameters *) = nullptr;
  uint32_t (*public_launch)(const char *, const char **, void *) = nullptr;
};
struct LaunchResult {
  bool ok;
  uint32_t code;
  const char *stage;
};
inline LaunchResult launch_title(TitleApi a, const char *title, bool elevated) {
  // Never request a privileged big-app handoff before a successful root proof.
  if (!elevated)
    return {false, 1, "root access required"};
  if (!a.user || (!a.launch && !a.public_launch))
    return {false, 38, "launch provider"};
  if (a.launch) {
    if (!a.initialize)
      return {false, 38, "initialize provider"};
    uint32_t r = a.initialize();
    if (r && r != 0x80940018u)
      return {false, r, "initialize"};
  }
  uint32_t user = 0;
  uint32_t r = a.user(&user);
  if (r)
    return {false, r, "foreground user"};
  LaunchParameters p{sizeof(p), user, 0, 0, a.launch ? 0u : 1u};
  r = a.launch ? a.launch(title, nullptr, &p)
               : a.public_launch(title, nullptr, &p);
  return {!(r & 0x80000000u), r, a.launch ? "LncUtil" : "system service"};
}
} // namespace ww
