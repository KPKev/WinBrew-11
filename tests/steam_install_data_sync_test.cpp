#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <unistd.h>
namespace playden::steam { bool SyncInstallData(const std::string&, const std::string&, bool, std::uint64_t, bool); }
int main() {
  char root[] = "/tmp/wine11-sync-data-XXXXXX"; assert(mkdtemp(root));
  const std::string dir = std::string(root) + "/dir";
  assert(std::filesystem::create_directory(dir));
  int fd = open((dir + "/file").c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
  assert(write(fd, "x", 1) == 1 && close(fd) == 0);
  assert(playden::steam::SyncInstallData(root, "dir/file", false, 1, true));
  assert(!playden::steam::SyncInstallData(root, "dir/file", false, 2, true));
  assert(!playden::steam::SyncInstallData(root, "../dir/file", false, 1, true));
  assert(symlink("dir", (std::string(root) + "/alias").c_str()) == 0);
  assert(!playden::steam::SyncInstallData(root, "alias/file", false, 1, true));
  assert(symlink("file", (dir + "/link").c_str()) == 0);
  assert(!playden::steam::SyncInstallData(root, "dir/link", false, 1, true));
  assert(std::filesystem::exists(dir + "/file"));
  std::filesystem::remove_all(root);
}
