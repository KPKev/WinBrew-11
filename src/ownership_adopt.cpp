// SPDX-License-Identifier: GPL-3.0-or-later
#include "ownership_adopt.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __PROSPERO__
#include <atomic>
#include <cstdint>
#include <sys/syscall.h>
extern "C" int sceKernelDlsym(int module, const char *name, void **symbol);
#endif

namespace ww::ownership {
namespace {
constexpr int kMaxDepth = 64;

#ifdef __PROSPERO__
// lchown is exported only by libkernel_sys, which titles do not link: the
// imported symbol resolves to null on the console. Use the same libkernel
// syscall gateway that the preserved prospero-win title launcher qualified for
// umask/flock (pw_title_publication_ps5_lock.c, LGPL-2.1-or-later): enter
// libkernel's getpid stub just past its syscall number and capture carry.
struct SyscallResult { long value; long failed; };
__attribute__((naked, noinline)) SyscallResult gateway_syscall(void *entry, long number, long first,
                                                              long second, long third) {
  __asm__ volatile(
      "movq %rdi, %r11\n"
      "movq %rsi, %rax\n"
      "movq %rdx, %rdi\n"
      "movq %rcx, %rsi\n"
      "movq %r8, %rdx\n"
      "movq %rdx, %r10\n"
      "subq $8, %rsp\n"
      "call *%r11\n"
      "setc %dl\n"
      "movzbl %dl, %edx\n"
      "addq $8, %rsp\n"
      "ret\n");
}
std::atomic<uintptr_t> gateway{0};
bool ensure_gateway() {
  if (gateway.load(std::memory_order_acquire)) return true;
  void *entry = nullptr;
  if (sceKernelDlsym(0x2001, "getpid", &entry) != 0 || !entry) return false;
  const uintptr_t candidate = reinterpret_cast<uintptr_t>(entry) + 0xa;
  const SyscallResult probe = gateway_syscall(reinterpret_cast<void *>(candidate), SYS_getpid, 0, 0, 0);
  if (probe.failed || probe.value != getpid()) return false;
  gateway.store(candidate, std::memory_order_release);
  return true;
}
int change_owner(const char *path, uid_t uid) {
  if (!ensure_gateway()) { errno = ENOSYS; return -1; }
  const SyscallResult r = gateway_syscall(reinterpret_cast<void *>(gateway.load(std::memory_order_acquire)),
                                          SYS_lchown, reinterpret_cast<long>(path), static_cast<long>(uid), -1L);
  if (r.failed) { errno = static_cast<int>(r.value); return -1; }
  return 0;
}
#else
int change_owner(const char *path, uid_t uid) { return lchown(path, uid, static_cast<gid_t>(-1)); }
#endif

struct Walk {
  uid_t app_uid;
  dev_t device;
  uint64_t max_entries;
  bool dry_run;
  Stats &stats;
};

bool hand_over(const char *path, const struct stat &before, Walk &w) {
  if (w.dry_run) { w.stats.changed++; return true; }
  if (change_owner(path, w.app_uid) != 0) { w.stats.errors++; return false; }
  struct stat after{};
  if (lstat(path, &after) != 0 || after.st_uid != w.app_uid || after.st_gid != before.st_gid ||
      (after.st_mode & 07777) != (before.st_mode & 07777) || after.st_ino != before.st_ino) {
    w.stats.errors++;
    return false;
  }
  w.stats.changed++;
  return true;
}

void visit(const char *path, int depth, Walk &w) {
  struct stat st{};
  if (w.stats.seen >= w.max_entries) { w.stats.errors++; return; }
  if (lstat(path, &st) != 0) { w.stats.errors++; return; }
  w.stats.seen++;
  if (st.st_dev != w.device) { w.stats.refused++; return; }
  if (S_ISLNK(st.st_mode)) { w.stats.symlinks++; return; }
  if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) { w.stats.refused++; return; }
  if (st.st_uid != 0 && st.st_uid != w.app_uid) { w.stats.refused++; return; }
  if (!S_ISDIR(st.st_mode)) {
    if (st.st_uid == w.app_uid) w.stats.already++;
    else hand_over(path, st, w);
    return;
  }
  if (depth >= kMaxDepth) { w.stats.errors++; return; }
  bool handed_back = false;
  DIR *dir = opendir(path);
  if (!dir && errno == EACCES && st.st_uid == w.app_uid && w.dry_run) {
    w.stats.unverified++;
    w.stats.already++;
    return;
  }
  if (!dir && errno == EACCES && st.st_uid == w.app_uid) {
    if (change_owner(path, 0) == 0) {
      handed_back = true;
      dir = opendir(path);
    }
  }
  if (!dir) {
    w.stats.errors++;
    if (handed_back && change_owner(path, w.app_uid) != 0) w.stats.errors++;
    return;
  }
  char child[1024];
  while (dirent *entry = readdir(dir)) {
    if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
    const int n = std::snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
    if (n < 0 || static_cast<size_t>(n) >= sizeof child) { w.stats.errors++; continue; }
    visit(child, depth + 1, w);
  }
  closedir(dir);
  if (st.st_uid == w.app_uid && !handed_back) { w.stats.already++; return; }
  hand_over(path, st, w);
}
}  // namespace

bool adopt_tree(const char *root, uid_t app_uid, Stats &stats, bool required, bool dry_run,
                uint64_t max_entries) {
  Stats local;
  struct stat st{};
  if (!root || !*root || app_uid == 0) { stats.errors++; return false; }
  if (lstat(root, &st) != 0) {
    if (errno == ENOENT && !required) return true;
    stats.errors++;
    return false;
  }
  if (!S_ISDIR(st.st_mode)) { stats.refused++; return false; }
  Walk w{app_uid, st.st_dev, max_entries, dry_run, local};
  visit(root, 0, w);
  stats.add(local);
  return local.clean();
}

bool can_change_owner() {
#ifdef __PROSPERO__
  return ensure_gateway();
#else
  return true;
#endif
}

bool restore_private_mode(const char *directory) {
  struct stat st{};
  if (lstat(directory, &st) != 0) return errno == ENOENT;
  if (!S_ISDIR(st.st_mode)) return false;
  const unsigned mode = st.st_mode & 07777;
  if (mode == 0700) return true;
  if (mode != 0711) return false;
  return chmod(directory, 0700) == 0 && lstat(directory, &st) == 0 && (st.st_mode & 07777) == 0700;
}

int read_title_list(const char *path, char (*ids)[10], int max) {
  const int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return errno == ENOENT ? 0 : -1;
  char bytes[4096];
  size_t used = 0;
  for (;;) {
    const ssize_t got = read(fd, bytes + used, sizeof bytes - used);
    if (got < 0) { close(fd); return -1; }
    if (got == 0) break;
    used += static_cast<size_t>(got);
    if (used == sizeof bytes) { close(fd); return -1; }
  }
  close(fd);
  int count = 0;
  size_t start = 0;
  for (size_t i = 0; i < used; ++i) {
    if (bytes[i] != '\n') continue;
    const size_t length = i - start;
    bool valid = length == 9;
    for (size_t k = 0; valid && k < 9; ++k) {
      const char c = bytes[start + k];
      valid = k < 4 ? (c >= 'A' && c <= 'Z') : (c >= '0' && c <= '9');
    }
    if (!valid) return -1;
    if (count >= max) return -1;
    std::memcpy(ids[count], bytes + start, 9);
    ids[count][9] = 0;
    ++count;
    start = i + 1;
  }
  return start == used ? count : -1;
}
}  // namespace ww::ownership
