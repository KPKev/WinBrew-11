"""Retire Vapor's generated PS5 titles on the same filesystem as /data/homebrew.

The host has already journaled the title and operation before calling RetireTitle.
The private games lease is held by Vapor while it requests this work. The source
and destination identities are checked by the original ManagedDirectory flow.
"""
from pathlib import Path
import sys


def transform(source: str) -> str:
    old_path = 'config_.state_root / "trash"'
    new_path = 'HostPath("/data/wine11/wine11-games") / ".vapor-title-trash"'
    if source.count(old_path) != 4:
        raise ValueError('Unexpected title trash references')
    source = source.replace(old_path, new_path)
    old = '''  std::string reason;
  const auto trash = HostPath("/data/wine11/wine11-games") / ".vapor-title-trash";
  if (!rt::EnsurePrivateDirectories(config_.boundary, config_.state_root, trash, &reason))
    return RemovalFailure{"trash-unavailable", "Playden could not prepare its private trash: " + reason};
  auto homebrew = platform::ManagedDirectory::Open(config_.homebrew_root, platform::DirectoryPolicy::kApprovedTree);
  auto destination = platform::ManagedDirectory::Open(trash, platform::DirectoryPolicy::kApprovedTree);
  struct stat checked{};'''
    new = '''  // The app-private state lives on a different mount from /data/homebrew.
  // Its game storage is on the homebrew filesystem and is held private by
  // Vapor's install lease. Do not create or adopt the games root here.
  auto games = platform::ManagedDirectory::Open(HostPath("/data/wine11/wine11-games"), platform::DirectoryPolicy::kPrivate);
  std::optional<platform::ManagedDirectory> private_trash;
  if (games) private_trash = games->OpenChild(".vapor-title-trash", platform::DirectoryPolicy::kPrivate, true);
  auto destination = private_trash ? platform::ManagedDirectory::Open(private_trash->path(), platform::DirectoryPolicy::kApprovedTree)
                                   : std::nullopt;
  struct stat games_info{}, private_info{}, approved_info{};
  if (!games || !private_trash || !destination || fstat(games->fd(), &games_info) != 0 || games_info.st_dev != info.st_dev ||
      fstat(private_trash->fd(), &private_info) != 0 || fstat(destination->fd(), &approved_info) != 0 ||
      private_info.st_dev != approved_info.st_dev || private_info.st_ino != approved_info.st_ino ||
      !games->Sync() || !private_trash->Sync() || !destination->Validate())
    return RemovalFailure{"trash-unavailable", "Vapor could not prepare same-drive title storage."};
  auto homebrew = platform::ManagedDirectory::Open(config_.homebrew_root, platform::DirectoryPolicy::kApprovedTree);
  struct stat checked{};'''
    if source.count(old) != 1:
        raise ValueError('Unexpected title retirement implementation')
    source = source.replace(old, new, 1)
    old_sync = '      !homebrew->Sync() || !destination->Sync())\n    return RemovalFailure{"retire-failed",'
    new_sync = '      !homebrew->Sync() || !destination->Sync() || !private_trash->Validate())\n    return RemovalFailure{"retire-failed",'
    if source.count(old_sync) != 1:
        raise ValueError('Unexpected title retirement sync')
    source = source.replace(old_sync, new_sync, 1)
    old = '''      if (c.leftover_journals_known) {
        std::error_code error;
        for (std::filesystem::directory_iterator it(HostPath("/data/wine11/wine11-games") / ".vapor-title-trash", error), end;'''
    new = '''      if (c.leftover_journals_known) {
        std::error_code error;
        const auto pending_records = c.repository.Snapshot();
        for (std::filesystem::directory_iterator it(HostPath("/data/wine11/wine11-games") / ".vapor-title-trash", error), end;'''
    if source.count(old) != 1:
        raise ValueError('Unexpected title trash startup scan')
    source = source.replace(old, new, 1)
    old = '''          const bool pending = std::any_of(c.blocked_leftover_titles.begin(), c.blocked_leftover_titles.end(),
                                           [&](const auto& title) { return entry.rfind(title + "-", 0) == 0; });'''
    new = '''          const bool pending = std::any_of(c.blocked_leftover_titles.begin(), c.blocked_leftover_titles.end(),
                                           [&](const auto& title) { return entry.rfind(title + "-", 0) == 0; }) ||
              std::any_of(pending_records.begin(), pending_records.end(), [&](const auto& record) {
                return record.state == rt::PreparationState::kRemoving && record.journal &&
                       entry == record.title_id + "-" + record.journal->operation_id;
              });'''
    if source.count(old) != 1:
        raise ValueError('Unexpected pending-title scan')
    return source.replace(old, new, 1)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: prepare_vapor_title_trash.py INPUT OUTPUT')
    source, destination = map(Path, sys.argv[1:])
    destination.write_text(transform(source.read_text()), encoding='utf8', newline='\n')
