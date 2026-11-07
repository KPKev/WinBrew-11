// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../src/shell.hpp"
#include <cassert>
inline void finish_paint_save(ww::Paint &paint, ww::Shell &shell) {
  int frames = 0;
  while (paint.saving() && frames++ < 100000)
    paint.tick(shell);
  assert(!paint.saving());
}
