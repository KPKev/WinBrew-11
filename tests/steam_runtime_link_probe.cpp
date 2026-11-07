// SPDX-License-Identifier: GPL-3.0-or-later
// This file is linked for dependency diagnostics and is never executed.
#include "../src/steam_integration/runtime.hpp"
#include "../src/steam_integration/controller.hpp"
#include "../src/steam_ui.hpp"
#ifdef WINE11_PROBE_DOWNLOADS
#include "platform/managed_directory.h"
#include <utility>
#endif
#ifdef WINE11_PROBE_NATIVE_STARTUP
#include "../src/steam_integration/native_session.hpp"
#endif
int main() {
  ww::steam_integration::Runtime runtime("/unusable-probe-state", "/unusable-probe-ca", true);
  runtime.ui().tick();
#ifdef WINE11_PROBE_CLOUD
  if (!runtime.EnableCloud()) return 1;
#endif
#ifdef WINE11_PROBE_DOWNLOADS
  auto directory = playden::platform::ManagedDirectory::Open("/unusable-probe-state/wine11-games", playden::platform::DirectoryPolicy::kPrivate);
#if !defined(WINE11_CANCEL_CLEANUP_OVERLAY) && !defined(WINE11_EXPERIMENT_UNINSTALL)
  if (directory) runtime.EnableDownloads(std::move(*directory));
#endif
#endif
#ifdef WINE11_PROBE_NATIVE_STARTUP
  auto session = ww::steam_integration::NativeSession::Create("/unusable-probe-state", -1, "/unusable-probe-ca");
  if (session) session->ui().tick();
#endif
  return 0;
}
