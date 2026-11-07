#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
namespace playden::steam { bool SyncInstallWinner(const std::string&); }
static std::atomic<int> fail_type{0};
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat s{};
  if (fstat(fd, &s) == 0 && ((fail_type == 1 && S_ISREG(s.st_mode)) || (fail_type == 2 && S_ISDIR(s.st_mode)))) {
    fail_type = 0; errno = EIO; return -1;
  }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/wine11-winner-sync-XXXXXX"; assert(mkdtemp(root));
  const std::string record = std::string(root) + "/.playden-install.json";
  assert(!playden::steam::SyncInstallWinner(root));
  int fd = open(record.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
  assert(write(fd, "record", 6) == 6 && close(fd) == 0);
  assert(playden::steam::SyncInstallWinner(root));
  fail_type = 1; assert(!playden::steam::SyncInstallWinner(root));
  fail_type = 2; assert(!playden::steam::SyncInstallWinner(root));
  assert(std::filesystem::exists(record));
  assert(unlink(record.c_str()) == 0);
  assert(symlink("/etc/passwd", record.c_str()) == 0);
  assert(!playden::steam::SyncInstallWinner(root));
  assert(unlink(record.c_str()) == 0 && rmdir(root) == 0);
}
