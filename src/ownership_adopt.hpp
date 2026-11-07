// SPDX-License-Identifier: GPL-3.0-or-later
// Hands root-owned files that Wine 11 created back to the app user.
//
// Playden's backend runs with the app user's effective UID and raises root
// only around specific steps. Its title launcher, publication reader and
// managed private folders all require the files they use to belong to that
// effective UID. Earlier Wine 11 builds ran their whole process as root, so
// everything they installed belonged to root and every published game failed
// at startup ("invalid-descriptor"). This module converts such trees once.
#pragma once
#include <cstdint>
#include <sys/types.h>

namespace ww::ownership {
struct Stats {
  uint64_t seen = 0, already = 0, changed = 0, symlinks = 0, refused = 0, errors = 0;
  uint64_t unverified = 0;  // dry run: app-owned private folders root may not enter
  void add(const Stats &o) {
    seen += o.seen; already += o.already; changed += o.changed;
    symlinks += o.symlinks; refused += o.refused; errors += o.errors; unverified += o.unverified;
  }
  bool clean() const { return refused == 0 && errors == 0; }
};

// Every entry under root owned by uid 0 becomes app_uid; gid and mode are kept.
// Post-order (contents before their directory: on the PS5 root cannot enter a
// 0700 directory owned by another user), symlinks are never followed or
// changed, the walk never leaves root's device, and entries owned by any other
// user are refused and left alone. A directory an interrupted earlier run
// already handed over is temporarily handed back for the walk. Needs euid 0.
// A missing root is not an error (stats stay zero) unless required is set.
// dry_run: change nothing; `changed` counts what would change. An app-owned
// directory root cannot enter is counted in `unverified` (an earlier hand-over
// already made it the app user's) and is not an error.
bool adopt_tree(const char *root, uid_t app_uid, Stats &stats, bool required = false,
                bool dry_run = false, uint64_t max_entries = 2000000);

// True when ownership can be changed in this process (on the console: the
// libkernel syscall gateway resolved and passed its getpid self-test).
bool can_change_owner();

// For a private directory that a stopgap made traversable (0711), restores
// 0700, the mode Playden's private-folder checks require. Other modes refuse.
bool restore_private_mode(const char *directory);

// Reads a title allow-list ("ABCD12345\n" lines). Writes up to max IDs
// (10 characters each incl. terminator) and returns how many; -1 on error.
int read_title_list(const char *path, char (*ids)[10], int max);
}  // namespace ww::ownership
