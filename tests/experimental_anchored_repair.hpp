// Host-only proof. Cooperating writers and authoritative caller-supplied identities
// are required; this does not establish a safe production or PS5 write boundary.
#pragma once
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

class ExperimentalAnchoredRepair {
 public:
  static std::unique_ptr<ExperimentalAnchoredRepair> Open(int root, const struct stat& expected_root,
      const std::string& path, const struct stat& expected_file) {
    auto out = std::unique_ptr<ExperimentalAnchoredRepair>(new ExperimentalAnchoredRepair);
    out->root_ = dup(root); if (out->root_ < 0) return {};
    struct stat actual{};
    if (fstat(out->root_, &actual) || !Same(actual, expected_root) || !S_ISDIR(actual.st_mode)) return {};
    int current = out->root_;
    std::size_t start = 0;
    for (;;) {
      const auto slash = path.find('/', start);
      const auto part = path.substr(start, slash == std::string::npos ? slash : slash - start);
      if (part.empty() || part == "." || part == "..") return {};
      if (slash == std::string::npos) { out->parent_ = current; out->name_ = part; break; }
      const int next = openat(current, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      if (next < 0) return {};
      out->dirs_.push_back(next);
      out->dir_names_.push_back(part);
      struct stat info{};
      if (fstat(next, &info) || !S_ISDIR(info.st_mode) || info.st_dev != expected_root.st_dev || info.st_uid != geteuid()) return {};
      current = next; start = slash + 1;
    }
    out->file_ = openat(out->parent_, out->name_.c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (out->file_ < 0 || fstat(out->file_, &actual) || !S_ISREG(actual.st_mode) ||
        actual.st_uid != geteuid() || actual.st_nlink != 1 || actual.st_size < 0 ||
        !Same(actual, expected_file) || actual.st_size != expected_file.st_size) return {};
    out->root_identity_ = expected_root; out->file_identity_ = expected_file;
    return out->Validate() ? std::move(out) : nullptr;
  }
  ~ExperimentalAnchoredRepair() {
    if (file_ >= 0) close(file_);
    for (int fd : dirs_) close(fd);
    if (root_ >= 0) close(root_);
  }
  bool Read(off_t offset, void* data, std::size_t size) {
    return Range(offset, size) && Validate() && pread(file_, data, size, offset) == static_cast<ssize_t>(size) && Validate();
  }
  bool Write(off_t offset, const void* data, std::size_t size) {
    return Range(offset, size) && Validate() && pwrite(file_, data, size, offset) == static_cast<ssize_t>(size) && Validate();
  }
 private:
  static bool Same(const struct stat& a, const struct stat& b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
  bool Range(off_t offset, std::size_t size) const {
    return offset >= 0 && offset <= file_identity_.st_size && size <= static_cast<std::size_t>(file_identity_.st_size - offset);
  }
  bool Validate() const {
    struct stat root{}, file{}, named{};
    if (fstat(root_, &root) || !Same(root, root_identity_) || !S_ISDIR(root.st_mode)) return false;
    int parent = root_;
    for (std::size_t i = 0; i < dirs_.size(); ++i) {
      struct stat opened{}, linked{};
      if (fstat(dirs_[i], &opened) || !S_ISDIR(opened.st_mode) ||
          opened.st_dev != root_identity_.st_dev || opened.st_uid != geteuid() ||
          fstatat(parent, dir_names_[i].c_str(), &linked, AT_SYMLINK_NOFOLLOW) ||
          !S_ISDIR(linked.st_mode) || !Same(opened, linked)) return false;
      parent = dirs_[i];
    }
    return !fstat(file_, &file) &&
      S_ISREG(file.st_mode) && file.st_uid == geteuid() && file.st_nlink == 1 &&
      Same(file, file_identity_) && file.st_size == file_identity_.st_size &&
      !fstatat(parent_, name_.c_str(), &named, AT_SYMLINK_NOFOLLOW) && Same(file, named) && S_ISREG(named.st_mode);
  }
  int root_ = -1, parent_ = -1, file_ = -1;
  std::vector<int> dirs_;
  std::vector<std::string> dir_names_;
  std::string name_;
  struct stat root_identity_{}, file_identity_{};
};
