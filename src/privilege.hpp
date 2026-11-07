// SPDX-License-Identifier: GPL-3.0-or-later
// Runs a single filesystem step with root when the process has lowered itself
// to the app user (the Vapor build, which follows Playden's credential model).
// Without an installed raiser (the plain desktop, the PC preview and tests)
// steps run directly, exactly as before.
//
// Only the call that needs the privilege is wrapped (open, rename, unlink,
// fchmod); a descriptor opened this way keeps working afterwards. A raiser
// must never block the UI: when the backend holds the credential lock, it
// fails the step with EBUSY ("busy, try again") instead of waiting.
#pragma once
#include <cerrno>
#include <type_traits>

namespace ww::privilege {
// Returns step's result. On a refusal returns -1 and sets errno.
using Raise = int (*)(int (*step)(void *), void *context);
void install(Raise raise);  // nullptr restores direct execution
bool installed();
int run_step(int (*step)(void *), void *context);

// f returns int (a descriptor or a status). errno set inside f is preserved.
template <class F> int run(F &&f) {
  using Fn = std::remove_reference_t<F>;
  return run_step([](void *c) -> int { return (*static_cast<Fn *>(c))(); },
                  static_cast<void *>(&f));
}
}  // namespace ww::privilege
