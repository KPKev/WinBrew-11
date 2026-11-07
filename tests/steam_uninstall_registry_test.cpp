#include "../src/steam_integration/uninstall_registry.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
static std::atomic<bool> arm{false}, fail_sync{false};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
  int result = __real_renameat(a, b, c, d);
  if (result == 0 && arm.exchange(false)) fail_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/vapor-registry-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    const auto file = paths->games.path() / ".playden-games";
    auto write = [&](const char* s) { std::ofstream out(file, std::ios::trunc); out << s; out.close(); assert(chmod(file.c_str(), 0600) == 0); };
    auto read = [&] { std::ifstream in(file); return std::string(std::istreambuf_iterator<char>(in), {}); };
    const char* original = "489830\tinternal\t123\n365670\tinternal\t1000\n440\tinternal\t200\n";
    auto plan = PlanInstalledRegistryRow(365670, original); assert(plan);
    write(original);
    // Existing Vapor installs use 0666 inside the private 0700 games root.
    assert(chmod(file.c_str(), 0666) == 0);
    assert(ReadInstalledRegistry(paths->games, *lease).value() == original);
    const auto extra_link = paths->games.path() / "registry-link";
    assert(link(file.c_str(), extra_link.c_str()) == 0);
    assert(!ReadInstalledRegistry(paths->games, *lease));
    assert(unlink(extra_link.c_str()) == 0);
    assert(ReadInstalledRegistry(paths->games, *lease).value() == original);
    assert(chmod(file.c_str(), 0777) == 0);
    assert(!ReadInstalledRegistry(paths->games, *lease));
    assert(chmod(file.c_str(), 0666) == 0);
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::removed);
    assert(read() == "489830\tinternal\t123\n440\tinternal\t200\n");
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::already_absent);
    assert(InspectReinstallRegistry(paths->games, *lease, *plan) == ReinstallRegistry::absent);
    assert(QueueReinstallRegistry(paths->games, *lease, *plan) == QueueReinstall::queued);
    assert(QueueReinstallRegistry(paths->games, *lease, *plan) == QueueReinstall::already_queued);
    write("489830\tinternal\t123\n440\tinternal\t200\n"); arm = true;
    assert(QueueReinstallRegistry(paths->games, *lease, *plan) == QueueReinstall::indeterminate);
    assert(QueueReinstallRegistry(paths->games, *lease, *plan) == QueueReinstall::already_queued);
    write("489830\tinternal\t123\n440\tinternal\t200\n365670\tinternal\t0\n");
    assert(InspectReinstallRegistry(paths->games, *lease, *plan) == ReinstallRegistry::queued);
    write("489830\tinternal\t123\n440\tinternal\t200\n365670\tinternal\t500\n");
    assert(InspectReinstallRegistry(paths->games, *lease, *plan) == ReinstallRegistry::installed);
    write("489830\tinternal\t999\n440\tinternal\t200\n365670\tinternal\t0\n");
    assert(InspectReinstallRegistry(paths->games, *lease, *plan) == ReinstallRegistry::blocked);
    write("365670\tinternal\t1000\n365670\tinternal\t1000\n");
    assert(!PlanInstalledRegistryRow(365670, read()));
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::blocked);
    assert(read() == "365670\tinternal\t1000\n365670\tinternal\t1000\n");
    write("365670\tinternal\t0\tpaused\n");
    assert(!PlanInstalledRegistryRow(365670, read()));
    const char* with_paused = "365670\tinternal\t1000\n730\tinternal\t0\tpaused\n";
    const auto paused_plan = PlanInstalledRegistryRow(365670, with_paused); assert(paused_plan);
    write(with_paused);
    assert(chmod(file.c_str(), 0666) == 0);
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *paused_plan) == RegistryRemoval::removed);
    assert(read() == "730\tinternal\t0\tpaused\n");
    assert(QueueReinstallRegistry(paths->games, *lease, *paused_plan) == QueueReinstall::queued);
    assert(read() == "730\tinternal\t0\tpaused\n365670\tinternal\t0\n");
    assert(!PlanInstalledRegistryRow(365670, "365670\tinternal\t1000\n730\tinternal\t0\n"));
    write("365670\tinternal\t0\tpaused\n");
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::blocked);
    write(original); arm = true;
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::indeterminate);
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::already_absent);
    assert(read() == "489830\tinternal\t123\n440\tinternal\t200\n");
    write("365670\tinternal\t1000");
    assert(!PlanInstalledRegistryRow(365670, read()));
    assert(RemoveInstalledRegistryRow(paths->games, *lease, *plan) == RegistryRemoval::blocked);
  }
  close(anchor); std::filesystem::remove_all(root);
}
