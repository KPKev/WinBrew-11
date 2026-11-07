// SPDX-License-Identifier: GPL-3.0-or-later
#include "vapor_privilege.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>

namespace ww::steam_integration {
int VaporRaise(int (*step)(void *), void *context) {
  auto token = playden::ps5::TryHoldEffectiveUid();
  if (!token) { errno = EBUSY; return -1; }
  int result = -1, saved = 0;
  bool ran = false;
  const bool restored = playden::ps5::WithRoot([&] {
    errno = 0;
    result = step(context);
    saved = errno;
    ran = true;
    return true;
  });
  if (!ran) { errno = EPERM; return -1; }
  (void)restored;  // the step itself completed; WithRoot reports any restore failure to its own callers
  errno = saved;
  return result;
}
}  // namespace ww::steam_integration
