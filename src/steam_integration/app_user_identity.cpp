// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_user_identity.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
constexpr const char *kMarker = ".wine11-app-user-v1";

bool path_join(char *out, size_t size, const char *a, const char *b) {
  const int n = std::snprintf(out, size, "%s/%s", a, b);
  return n > 0 && static_cast<size_t>(n) < size;
}

bool marker_present(const char *private_root) {
  char path[1024];
  if (!path_join(path, sizeof path, private_root, kMarker)) return false;
  struct stat st{};
  return lstat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool write_marker(const char *private_root) {
  char path[1024];
  if (!path_join(path, sizeof path, private_root, kMarker)) return false;
  const int fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  static const char text[] = "WinBrew 11 runs as the app user; root-created files were handed over.\n";
  const bool ok = write(fd, text, sizeof text - 1) == static_cast<ssize_t>(sizeof text - 1) && fsync(fd) == 0;
  return close(fd) == 0 && ok;
}

// Everything earlier WinBrew 11 builds created as root for Vapor.
bool adopt_all(const AppUserPaths &paths, uid_t app_uid, ww::ownership::Stats &stats, bool dry_run) {
  using namespace ww::ownership;
  bool ok = adopt_tree(paths.private_root, app_uid, stats, true, dry_run);
  char path[1024];
  // Vapor's games folder: back to the private mode Playden's checks require.
  if (path_join(path, sizeof path, paths.data_root, "wine11/wine11-games")) {
    if (dry_run) {
      struct stat st{};
      if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & 07777) != 0700 && (st.st_mode & 07777) != 0711)
        ok = false;  // an unexpected mode would make the apply step refuse
    } else ok = restore_private_mode(path) && ok;
    ok = adopt_tree(path, app_uid, stats, false, dry_run) && ok;
  } else ok = false;
  // Per-game data roots Vapor published under Playden's games folder.
  char games[1024];
  if (path_join(games, sizeof games, paths.data_root, "playden/games")) {
    if (DIR *dir = opendir(games)) {
      while (dirent *entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "wine11-steam-", 13) != 0) continue;
        if (path_join(path, sizeof path, games, entry->d_name)) ok = adopt_tree(path, app_uid, stats, false, dry_run) && ok;
        else ok = false;
      }
      closedir(dir);
    } else if (errno != ENOENT) ok = false;
  } else ok = false;
  // Title folders Vapor published: exactly the titles in Vapor's own allow-list.
  char list[1024];
  char ids[256][10];
  int count = 0;
  if (path_join(list, sizeof list, paths.data_root, "wine11/title-rule/allow-list"))
    count = read_title_list(list, ids, 256);
  if (count < 0) return false;
  for (int i = 0; i < count; ++i) {
    char homebrew[1024];
    if (!path_join(homebrew, sizeof homebrew, paths.data_root, "homebrew") ||
        !path_join(path, sizeof path, homebrew, ids[i])) { ok = false; continue; }
    ok = adopt_tree(path, app_uid, stats, false, dry_run) && ok;
  }
  return ok;
}
}  // namespace

const char *AppUserStageName(AppUserStage stage) {
  switch (stage) {
    case AppUserStage::lowered: return "app user";
    case AppUserStage::not_elevated: return "not elevated";
    case AppUserStage::original_was_root: return "started as root";
    case AppUserStage::no_real_root: return "no real root";
    case AppUserStage::adopt_incomplete: return "adoption incomplete";
    case AppUserStage::lower_failed: return "lower failed";
  }
  return "unknown";
}

AppUserOutcome AdoptAndLower(uid_t original_euid, bool elevated, const AppUserPaths &paths) {
  AppUserOutcome out;
  out.app_uid = original_euid;
  if (!elevated) { out.stage = AppUserStage::not_elevated; return out; }
  if (original_euid == 0) { out.stage = AppUserStage::original_was_root; return out; }
  // Raising root again later needs a real (or saved) UID of 0.
  if (getuid() != 0 || geteuid() != 0) { out.stage = AppUserStage::no_real_root; return out; }
  if (!paths.private_root || !*paths.private_root) { out.stage = AppUserStage::adopt_incomplete; out.error = EINVAL; return out; }
  bool apply_clean = true;
  if (!marker_present(paths.private_root)) {
    // 1. Plan: a read-only pass over everything. Anything unexpected (or no
    //    working way to change ownership) means no file is changed and the
    //    process keeps its current (root) behaviour.
    ww::ownership::Stats plan;
    if (!ww::ownership::can_change_owner()) {
      out.stage = AppUserStage::adopt_incomplete;
      out.error = ENOSYS;
      return out;
    }
    if (!adopt_all(paths, original_euid, plan, true) || !plan.clean()) {
      out.stats = plan;
      out.stage = AppUserStage::adopt_incomplete;
      out.error = EIO;
      return out;
    }
    // 2. Apply. Lower only when every entry was handed over. After a rare
    //    failure the process stays root; the next start's plan treats the
    //    already converted private folders as done and finishes the rest.
    out.adopted_now = true;
    apply_clean = adopt_all(paths, original_euid, out.stats, false) && out.stats.clean();
    if (!apply_clean) {
      out.stage = AppUserStage::adopt_incomplete;
      out.error = EIO;
      return out;
    }
  }
  errno = 0;
  if (seteuid(original_euid) != 0 || geteuid() != original_euid) {
    out.error = errno ? errno : EPERM;
    if (geteuid() != 0 && seteuid(0) != 0) out.error = errno ? errno : EPERM;
    out.stage = AppUserStage::lower_failed;
    return out;
  }
  if (out.adopted_now && !write_marker(paths.private_root)) out.error = errno ? errno : EIO;  // retried next start
  out.stage = AppUserStage::lowered;
  return out;
}
}  // namespace ww::steam_integration
