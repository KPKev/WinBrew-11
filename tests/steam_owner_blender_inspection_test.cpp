#include "../src/steam_integration/owner_blender_inspection.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
using namespace playden;
void Run(bool pending) {
  char root[] = "/tmp/vapor-owner-inspect-XXXXXX"; assert(mkdtemp(root));
  const int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = PrepareNativePaths(root, anchor); assert(paths);
    auto lease = platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", platform::LockMode::kPublisher); assert(lease);
    const auto game = paths->games.path() / "365670"; assert(mkdir(game.c_str(), 0700) == 0);
    const std::string registry = "365670\tinternal\t1000\n730\tinternal\t0\tpaused\n";
    { std::ofstream out(paths->games.path() / ".playden-games"); out << registry; }
    assert(chmod((paths->games.path() / ".playden-games").c_str(), 0666) == 0);
    UninstallJournal journal(paths->state, paths->state_lease, paths->games, *lease);
    if (pending) {
      struct stat st{}; assert(paths->games.InspectChild("365670", &st));
      UninstallJournal::Entry entry{UninstallJournal::Phase::intent, 365670,
          "11111111-2222-4333-8444-555555555555", static_cast<std::uint64_t>(st.st_dev),
          static_cast<std::uint64_t>(st.st_ino), "FAKE78008", false};
      entry.registry_before = registry; assert(journal.Begin(entry));
    }
    homebrew::RuntimeSnapshot host; host.status = homebrew::RuntimeHostStatus::kReady; host.installs_known = true;
    homebrew::RuntimeInstallView record; record.runtime_number = 365670; record.game = GameId::Steam(365670);
    record.title_id = "FAKE78008"; record.failure_code = "test-refusal"; record.failure = "test failure\ncontinued";
    host.installs.push_back(record);
    installs::InstallSnapshot snapshot; snapshot.installed.push_back({GameId::Steam(365670), 1000, game.string(), "internal", true});
    Storage storage(paths->state.path().string());
    assert(WriteOwnerBlenderInspection(paths->games, *lease, journal, storage, host, &snapshot, true));
    std::ifstream in(paths->games.path() / kOwnerInspectionName);
    const std::string report(std::istreambuf_iterator<char>(in), {});
    assert(report.find(pending ? "phase=1\n" : "phase=0\n") != std::string::npos);
    assert(report.find("registry_read=1\n") != std::string::npos && report.find("registry_plan=1\n") != std::string::npos);
    assert(report.find("host_failure=test failure continued\n") != std::string::npos);
    if (pending) assert(report.find("state_probe_write=") == std::string::npos);
    else assert(report.find("state_probe_write=1\n") != std::string::npos && report.find("state_probe_removed=1\n") != std::string::npos);
    assert(!WriteOwnerBlenderInspection(paths->games, *lease, journal, storage, host, &snapshot, true));
    assert(journal.Read().phase == (pending ? UninstallJournal::Phase::intent : UninstallJournal::Phase::absent));
  }
  close(anchor); std::filesystem::remove_all(root);
}
int main() { Run(false); Run(true); }
