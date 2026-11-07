#include "../src/steam_integration/owner_cleanup_probe.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
extern "C" int __real_fstatat(int, const char*, struct stat*, int);
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_fstatat(int fd, const char* name, struct stat* st, int flags) { return __real_fstatat(fd, name, st, flags); }
extern "C" int __wrap_unlinkat(int fd, const char* name, int flags) { return __real_unlinkat(fd, name, flags); }
int main() {
  assert(geteuid() == 0);
  const char* user = std::getenv("SUDO_UID"); const uid_t app_uid = user ? static_cast<uid_t>(std::strtoul(user, nullptr, 10)) : 1000;
  assert(app_uid != 0);
  char root[] = "/tmp/vapor-owner-probe-XXXXXX"; assert(mkdtemp(root) && chmod(root,0755) == 0);
  const auto base=std::filesystem::path(root), path=base/"wine11-games";
  assert(mkdir(path.c_str(),0700) == 0 && chown(path.c_str(),app_uid,-1) == 0);
  { std::ofstream outside(base/"outside"); outside << "keep"; }
  assert(seteuid(app_uid) == 0);
  int lock=open((path/"wine11-install.lock").c_str(),O_CREAT|O_EXCL|O_WRONLY,0600); assert(lock >= 0 && close(lock) == 0);
  { std::ofstream registry(path/".playden-games"); registry << "730\tinternal\t0\tpaused\n"; }
  assert(chmod((path/".playden-games").c_str(),0600) == 0);
  auto games=playden::platform::ManagedDirectory::Open(path,playden::platform::DirectoryPolicy::kPrivate); assert(games);
  auto lease=playden::platform::ManagedFileLock::Acquire(*games,"wine11-install.lock",playden::platform::LockMode::kPublisher); assert(lease);
  assert(ww::steam_integration::RunOwnerCleanupProbe(*games,*lease,base));
  std::ifstream input(path/".vapor-cleanup-probe-47fc159f-096c-4399-a934-4455637be846.txt");
  const std::string report((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
  assert(report.find("wrapped_stat=0 errno=0\n") != std::string::npos);
  assert(report.find("wrapped_unlink=0 errno=0\n") != std::string::npos);
  assert(report.find("remove_state=2 ") != std::string::npos && report.find("probe_complete=1\n") != std::string::npos);
  assert(!std::filesystem::exists(path/"4294967286") && std::filesystem::exists(base/"outside"));
  assert(!ww::steam_integration::RunOwnerCleanupProbe(*games,*lease,base));
  lease.reset(); games.reset(); assert(seteuid(0) == 0); std::filesystem::remove_all(base);
}
