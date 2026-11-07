// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "shell.hpp"
#include "owner_admission/admission_observer.h"
namespace ww {
using OwnerObserver = Wine11AdmissionReport (*)(int);
inline void dispatch_owner_check(Shell &shell, bool granted, OwnerObserver observer) {
  if (!shell.owner_check_requested) return;
  shell.owner_check_requested = false;
  if (shell.pending != Action::none) { shell.owner_check_status = -1; return; }
  auto result = observer ? observer(granted ? 1 : 0) : Wine11AdmissionReport{};
  int status = int(result.status);
  shell.owner_check_status = status >= 0 && status <= 8 ? status : 4;
}
inline const char *owner_check_line(int status) {
  if (status == -1) return "Check Quake II RTX automatic return compatibility.";
  if (status == -2) return "Waiting to run the read-only check.";
  if (status == WINE11_ADMISSION_READ_ONLY_PASSED) return "Read-only checks passed. Return is not installed yet.";
  if (status == WINE11_ADMISSION_BUSY) return "The game is busy. Close it and try again.";
  if (status == WINE11_ADMISSION_NO_ACCESS) return "The required game details are not accessible.";
  if (status == WINE11_ADMISSION_WRONG_GAME) return "This installation does not match Quake II RTX.";
  return "The game details could not be verified.";
}
} // namespace ww
