// SPDX-License-Identifier: GPL-3.0-or-later
#include "owner_blender_inspection.hpp"
#include "uninstall_registry.hpp"
#include "platform/ps5/title_allow_list.h"
#include "platform/random_id.h"
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
namespace {
std::string Safe(std::string value) {
  if (value.size() > 160) value.resize(160);
  for (char& c : value) if (c == '\n' || c == '\r' || static_cast<unsigned char>(c) < 32) c = ' ';
  return value;
}
}
bool WriteOwnerBlenderInspection(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const UninstallJournal& journal,
    const playden::Storage& state_storage, const playden::homebrew::RuntimeSnapshot& host,
    const playden::installs::InstallSnapshot* installs, bool maintenance_ready) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto valid = [&] { return games.policy() == playden::platform::DirectoryPolicy::kPrivate &&
      games.Validate() && lease.Validate(games, "wine11-install.lock"); };
  if (!valid()) return false;
  struct stat previous{}; errno = 0;
  if (games.InspectChild(kOwnerInspectionName, &previous) || errno != ENOENT || !valid()) return false;
  const auto entry = journal.Read();
  std::string report = "phase=" + std::to_string(static_cast<unsigned>(entry.phase)) + "\n" +
      "journal_app=" + std::to_string(entry.app) + "\n" + "journal_title=" + Safe(entry.title) + "\n" +
      "journal_reinstall=" + std::to_string(entry.reinstall) + "\n" +
      "host_status=" + std::to_string(static_cast<unsigned>(host.status)) + "\n" +
      "host_installs_known=" + std::to_string(host.installs_known) + "\n" +
      "maintenance_ready=" + std::to_string(maintenance_ready) + "\n";
  unsigned host_matches = 0;
  for (const auto& record : host.installs) {
    if (record.runtime_number != 365670 && record.game != playden::GameId::Steam(365670)) continue;
    ++host_matches;
    report += "host_title=" + Safe(record.title_id) + "\n" +
        "host_state=" + std::to_string(static_cast<unsigned>(record.state)) + "\n" +
        "host_step=" + Safe(record.step) + "\n" + "host_failure_code=" + Safe(record.failure_code) + "\n" +
        "host_failure=" + Safe(record.failure) + "\n" +
        "host_recovery_pending=" + std::to_string(record.recovery_pending) + "\n" +
        "host_awaiting_registration=" + std::to_string(record.awaiting_registration) + "\n";
  }
  report += "host_matches=" + std::to_string(host_matches) + "\n";
  if (const auto refused = host.refusals.find(365670); refused != host.refusals.end())
    report += "host_refusal=" + Safe(refused->second) + "\n";
  if (const auto refused = host.play_refusals.find(365670); refused != host.play_refusals.end())
    report += "host_play_refusal=" + Safe(refused->second) + "\n";
  if (host.launch && host.launch->runtime_number == 365670)
    report += "host_launch_state=" + std::to_string(static_cast<unsigned>(host.launch->state)) + "\n" +
        "host_launch_code=" + Safe(host.launch->code) + "\n" + "host_launch_message=" + Safe(host.launch->message) + "\n";
  if (installs) {
    unsigned matches = 0; bool root_matches = false;
    for (const auto& item : installs->installed) if (item.id == playden::GameId::Steam(365670)) {
      ++matches; root_matches = item.available && item.volume_id == "internal" &&
          item.root == (games.path() / "365670").string();
    }
    report += "installed_matches=" + std::to_string(matches) + "\n" +
        "installed_root_matches=" + std::to_string(root_matches) + "\n" +
        "jobs=" + std::to_string(installs->jobs.size()) + "\n" +
        "uninstalling=" + std::to_string(installs->uninstalling.size()) + "\n";
  } else report += "install_worker=stopped\n";
  struct stat root{}, game{}; errno = 0;
  const bool root_ok = fstat(games.fd(), &root) == 0;
  const bool game_ok = games.InspectChild("365670", &game);
  const bool game_matches = root_ok && game_ok && S_ISDIR(game.st_mode) && game.st_uid == geteuid() && game.st_dev == root.st_dev;
  report += "game_identity_ok=" + std::to_string(game_matches) + "\n";
  const auto registry = ReadInstalledRegistry(games, lease); const int registry_errno = errno;
  report += "registry_read=" + std::to_string(bool(registry)) + "\n" +
      "registry_errno=" + std::to_string(registry_errno) + "\n" +
      "registry_plan=" + std::to_string(registry && bool(PlanInstalledRegistryRow(365670, *registry))) + "\n" +
      "random_uuid=" + std::to_string(bool(playden::RandomUuid())) + "\n";
  if (entry.phase == UninstallJournal::Phase::absent) {
    constexpr char probe[] = "vapor-owner-storage-probe-472aa895-6e24-4fd1-b035-9d6170ec3004";
    const playden::Bytes bytes{'o','k'};
    const auto written = state_storage.WriteDurableExclusive(probe, bytes);
    report += "state_probe_write=" + std::to_string(written.ok()) + "\n" +
        "state_probe_error=" + std::to_string(static_cast<unsigned>(written.error())) + "\n" +
        "state_probe_message=" + Safe(written.message()) + "\n";
    if (written.ok()) {
      const auto read = state_storage.ReadDurable(probe, 8);
      report += "state_probe_read=" + std::to_string(read.ok() && read.value() == bytes) + "\n";
      if (read.ok() && read.value() == bytes) {
        const auto removed = state_storage.RemoveDurable(probe);
        report += "state_probe_removed=" + std::to_string(removed.ok()) + "\n";
      }
    }
  }
  report += "report_complete=1\n";
  if (!valid()) return false;
  const int fd = games.CreateExclusiveFile(kOwnerInspectionName, 0600);
  if (fd < 0) return false;
  std::size_t done = 0; bool ok = true;
  while (done < report.size()) {
    const auto n = write(fd, report.data() + done, report.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  const bool synced = ok && fsync(fd) == 0;
  const bool closed = close(fd) == 0;
  return synced && closed && games.Sync() && valid();
}
}
