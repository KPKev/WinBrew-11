// SPDX-License-Identifier: GPL-3.0-or-later
#include "vapor_game_cleanup.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
enum class ReadState { absent, valid, invalid };
ReadState ReadOwned(const playden::platform::ManagedDirectory& games, const std::string& name, std::string& contents) {
  struct stat named{}, before{}, after{}; errno = 0;
  if (!games.InspectChild(name, &named)) return errno == ENOENT && games.Validate() ? ReadState::absent : ReadState::invalid;
  const int fd = games.OpenRegularFile(name, true, 0600);
  if (fd < 0) return ReadState::invalid;
  bool ok = !fstat(fd, &before) && S_ISREG(before.st_mode) && before.st_uid == geteuid() && before.st_nlink == 1 &&
      (before.st_mode & 07777) == 0600 && before.st_size > 0 && before.st_size <= 256 &&
      before.st_dev == named.st_dev && before.st_ino == named.st_ino;
  if (ok) contents.resize(static_cast<std::size_t>(before.st_size));
  std::size_t done = 0;
  while (ok && done < contents.size()) {
    const auto n = read(fd, &contents[done], contents.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  ok = ok && !fsync(fd) && !fstat(fd, &after) && before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
      before.st_size == after.st_size && before.st_uid == after.st_uid && before.st_mode == after.st_mode &&
      before.st_nlink == after.st_nlink && before.st_mtim.tv_sec == after.st_mtim.tv_sec &&
      before.st_mtim.tv_nsec == after.st_mtim.tv_nsec && before.st_ctim.tv_sec == after.st_ctim.tv_sec &&
      before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
  const bool closed = close(fd) == 0;
  return ok && closed && games.InspectChild(name, &named) && named.st_dev == after.st_dev &&
      named.st_ino == after.st_ino && named.st_size == after.st_size && named.st_uid == after.st_uid &&
      named.st_mode == after.st_mode && named.st_nlink == after.st_nlink && games.Sync() ? ReadState::valid : ReadState::invalid;
}
bool CreateRequest(const playden::platform::ManagedDirectory& games, const std::string& text) {
  const int fd = games.CreateExclusiveFile(vapor_cleaner::kRequest, 0600);
  if (fd < 0) return false;
  std::size_t done = 0; bool ok = true;
  while (done < text.size()) {
    const auto n = write(fd, text.data() + done, text.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  ok = ok && !fsync(fd);
  if (close(fd) != 0 || !ok || !games.Sync()) return false;
  std::string checked;
  return ReadOwned(games, vapor_cleaner::kRequest, checked) == ReadState::valid && checked == text;
}
}
CleanerState RunGameCleanupHelper(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const vapor_cleaner::Request& request,
    const std::function<bool()>& journal_valid, const std::function<ProtectedState()>& observe,
    const std::function<bool()>& send_once, const std::function<void()>& wait, unsigned polls) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto valid = [&] { return games.policy() == playden::platform::DirectoryPolicy::kPrivate && games.Validate() &&
      lease.Validate(games, "wine11-install.lock") && journal_valid && journal_valid(); };
  const auto text = vapor_cleaner::Encode(request);
  if (text.empty() || !valid() || !observe || !send_once || !wait || !polls) return CleanerState::blocked;
  std::string request_text, result_text;
  auto req = ReadOwned(games, vapor_cleaner::kRequest, request_text);
  auto result = ReadOwned(games, vapor_cleaner::kResult, result_text);
  if (req == ReadState::invalid || (req == ReadState::valid && request_text != text) ||
      (req == ReadState::absent && result != ReadState::absent &&
       (result != ReadState::valid || result_text != text + "ok\n"))) return CleanerState::blocked;
  if (req == ReadState::absent && result == ReadState::absent) {
    const auto state = observe();
    if (state == ProtectedState::removed) return CleanerState::complete;
    if (state != ProtectedState::retired || !valid() || !CreateRequest(games, text) || !valid()) return CleanerState::blocked;
    // Failure is ambiguous: the loader may already have accepted the bytes.
    // Leave the request in place so recovery can only observe the receipt.
    if (!send_once()) return CleanerState::pending;
  }
  for (unsigned attempt = 0; attempt < polls; ++attempt) {
    if (!valid()) return CleanerState::blocked;
    request_text.clear(); result_text.clear();
    req = ReadOwned(games, vapor_cleaner::kRequest, request_text);
    result = ReadOwned(games, vapor_cleaner::kResult, result_text);
    if (req == ReadState::invalid || (req == ReadState::valid && request_text != text)) return CleanerState::blocked;
    if (result == ReadState::valid && result_text == text + "ok\n") {
      if (observe() != ProtectedState::removed || !valid()) return CleanerState::blocked;
      if (req == ReadState::valid && (!games.RemoveRegularFile(vapor_cleaner::kRequest) || !games.Sync())) return CleanerState::blocked;
      if (!valid() || !games.RemoveRegularFile(vapor_cleaner::kResult) || !games.Sync()) return CleanerState::blocked;
      return observe() == ProtectedState::removed && valid() ? CleanerState::complete : CleanerState::blocked;
    }
    // A result can be observed while the helper is still writing it. A missing,
    // partial, or unreadable result is never treated as success or re-sent.
    if (req == ReadState::absent) return CleanerState::blocked;
    if (attempt + 1 < polls) wait();
  }
  return CleanerState::pending;
}
}  // namespace ww::steam_integration
