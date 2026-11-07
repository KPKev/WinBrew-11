#include "../src/steam_integration/owner_blender_request.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
using namespace playden::platform;
int main() {
  char root[] = "/tmp/vapor-owner-blender-XXXXXX"; assert(mkdtemp(root));
  {
    auto games = ManagedDirectory::Open(root, DirectoryPolicy::kPrivate); assert(games);
    const int lock = games->CreateExclusiveFile("wine11-install.lock", 0600); assert(lock >= 0 && close(lock) == 0);
    auto lease = ManagedFileLock::Acquire(*games, "wine11-install.lock", LockMode::kPublisher); assert(lease);
    const auto path = games->path() / kOwnerBlenderRequestName;
    const auto create = [&](const char* value) {
      const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600); assert(fd >= 0);
      assert(fchmod(fd, 0600) == 0 && write(fd, value, std::strlen(value)) == static_cast<ssize_t>(std::strlen(value)));
      assert(fsync(fd) == 0 && close(fd) == 0);
    };
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    assert(ClaimOwnerBlenderRequest(*games, *lease, now) == OwnerBlenderClaim::absent);
    create("wrong");
    assert(ClaimOwnerBlenderRequest(*games, *lease, now) == OwnerBlenderClaim::rejected);
    assert(std::filesystem::exists(path)); assert(unlink(path.c_str()) == 0);
    create(kOwnerBlenderRequestText);
    const auto created_now = static_cast<std::int64_t>(std::time(nullptr));
    assert(chmod(path.c_str(), 0666) == 0);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now) == OwnerBlenderClaim::rejected);
    assert(chmod(path.c_str(), 0600) == 0);
    const auto extra = games->path() / "second-link"; assert(link(path.c_str(), extra.c_str()) == 0);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now) == OwnerBlenderClaim::rejected);
    assert(unlink(extra.c_str()) == 0);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now + 601) == OwnerBlenderClaim::rejected);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now) == OwnerBlenderClaim::claimed);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now) == OwnerBlenderClaim::absent);
    assert(symlink("elsewhere", path.c_str()) == 0);
    assert(ClaimOwnerBlenderRequest(*games, *lease, created_now) == OwnerBlenderClaim::rejected);
    assert(unlink(path.c_str()) == 0);
  }
  std::filesystem::remove_all(root);
}
