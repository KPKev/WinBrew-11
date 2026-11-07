#include "experimental_anchored_repair.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static bool short_write = false, read_error = false;
extern "C" ssize_t __real_pwrite(int, const void*, size_t, off_t);
extern "C" ssize_t __wrap_pwrite(int fd, const void* data, size_t len, off_t offset) {
  if (short_write) return len ? static_cast<ssize_t>(len - 1) : 0;
  return __real_pwrite(fd, data, len, offset);
}
extern "C" ssize_t __real_pread(int, void*, size_t, off_t);
extern "C" ssize_t __wrap_pread(int fd, void* data, size_t len, off_t offset) {
  if (read_error) { errno = EIO; return -1; }
  return __real_pread(fd, data, len, offset);
}
static std::string Read(const std::filesystem::path& path) { std::ifstream f(path); std::string s; f >> s; return s; }
int main() {
  char temp[] = "/tmp/wine11-anchored-repair-XXXXXX"; assert(mkdtemp(temp));
  const auto base = std::filesystem::path(temp), root = base / "root", external = base / "external";
  std::filesystem::create_directories(root / "data"); std::filesystem::create_directory(external);
  { std::ofstream(root / "data/game") << "AAAA"; std::ofstream(external / "sentinel") << "SAFE"; }
  const int rootfd = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC); assert(rootfd >= 0);
  struct stat r{}, f{}; assert(!fstat(rootfd, &r) && !stat((root / "data/game").c_str(), &f));
  auto wrong = r; ++wrong.st_ino;
  assert(!ExperimentalAnchoredRepair::Open(rootfd, wrong, "data/game", f));
  auto file = ExperimentalAnchoredRepair::Open(rootfd, r, "data/game", f); assert(file);
  char contents[4]{}; assert(file->Read(0, contents, sizeof(contents)) && std::string(contents,4) == "AAAA");
  read_error = true; assert(!file->Read(0, contents, sizeof(contents))); read_error = false;
  short_write = true; assert(!file->Write(0, "BBBB", 4)); short_write = false;
  assert(Read(root / "data/game") == "AAAA");
  assert(file->Write(0, "BBBB", 4) && Read(root / "data/game") == "BBBB");
  std::filesystem::rename(root / "data", root / "moved");
  std::filesystem::create_directory(root / "data");
  assert(!file->Write(0, "XXXX", 4) && Read(root / "moved/game") == "BBBB");
  std::filesystem::remove(root / "data");
  std::filesystem::rename(root / "moved", root / "data");
  assert(file->Read(0, contents, sizeof(contents)));
  std::filesystem::rename(root / "data/game", root / "data/old");
  std::filesystem::create_symlink(external / "sentinel", root / "data/game");
  assert(!file->Write(0, "XXXX", 4) && Read(external / "sentinel") == "SAFE");
  assert(!ExperimentalAnchoredRepair::Open(rootfd, r, "data/game", f));
  std::filesystem::create_directory_symlink(external, root / "link");
  assert(!ExperimentalAnchoredRepair::Open(rootfd, r, "link/sentinel", f));
  std::filesystem::create_hard_link(external / "sentinel", root / "data/hard");
  struct stat hard{}; assert(!stat((root / "data/hard").c_str(), &hard));
  assert(!ExperimentalAnchoredRepair::Open(rootfd, r, "data/hard", hard));
  assert(Read(external / "sentinel") == "SAFE");
  file.reset(); close(rootfd); std::filesystem::remove_all(base);
}
