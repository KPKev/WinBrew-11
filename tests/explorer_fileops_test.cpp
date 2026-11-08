#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace fs = std::filesystem;
namespace {
void put(const fs::path &p, const std::string &value) {
  FILE *f = std::fopen(p.string().c_str(), "wb");
  assert(f && std::fwrite(value.data(), 1, value.size(), f) == value.size());
  assert(std::fclose(f) == 0);
}
std::string get(const fs::path &p) {
  FILE *f = std::fopen(p.string().c_str(), "rb");
  assert(f);
  std::string out;
  char buffer[4096];
  while (auto n = std::fread(buffer, 1, sizeof(buffer), f))
    out.append(buffer, n);
  assert(!std::ferror(f));
  std::fclose(f);
  return out;
}
void finish(ww::fileops::Transfer &job) {
  for (int i = 0; job.busy() && i < 10000; ++i)
    job.step();
  assert(!job.busy());
}
enum class Failure {
  none,
  create,
  read,
  write,
  zero,
  rewind,
  sync,
  permissions,
  close,
  verify
};
struct Fault {
  Failure failure;
  bool rewound = false, wrote = false;
};
ww::fileops::Io faulty(Fault &f) {
  auto io = ww::fileops::system_io();
  io.context = &f;
  io.open = [](void *ctx, const char *path, bool create) {
    return create && static_cast<Fault *>(ctx)->failure == Failure::create
               ? -EACCES
               : ww::fileops::system_io().open(nullptr, path, create);
  };
  io.read = [](void *ctx, int fd, void *bytes, size_t size) -> int64_t {
    auto &f = *static_cast<Fault *>(ctx);
    if (f.failure == Failure::read)
      return -EIO;
    auto count = ww::fileops::system_io().read(nullptr, fd, bytes,
                                               size > 1021 ? 1021 : size);
    if (count > 0 && f.failure == Failure::verify && f.rewound) {
      static_cast<char *>(bytes)[0] ^= 1;
      f.failure = Failure::none; // Corrupt only one of the verification reads.
    }
    return count;
  };
  io.write = [](void *ctx, int fd, const void *bytes, size_t size) -> int64_t {
    auto &f = *static_cast<Fault *>(ctx);
    f.wrote = true;
    if (f.failure == Failure::write)
      return -ENOSPC;
    if (f.failure == Failure::zero)
      return 0;
    return ww::fileops::system_io().write(nullptr, fd, bytes,
                                          size > 4093 ? 4093 : size);
  };
  io.rewind = [](void *ctx, int fd) {
    auto &f = *static_cast<Fault *>(ctx);
    f.rewound = true;
    return f.failure == Failure::rewind
               ? -EIO
               : ww::fileops::system_io().rewind(nullptr, fd);
  };
  io.permissions = [](void *ctx, int fd, unsigned mode) {
    return static_cast<Fault *>(ctx)->failure == Failure::permissions
               ? -EPERM
               : ww::fileops::system_io().permissions(nullptr, fd, mode);
  };
  io.sync = [](void *ctx, int fd) {
    return static_cast<Fault *>(ctx)->failure == Failure::sync
               ? -EIO
               : ww::fileops::system_io().sync(nullptr, fd);
  };
  io.close = [](void *ctx, int fd) {
    auto &f = *static_cast<Fault *>(ctx);
    const int code = ww::fileops::system_io().close(nullptr, fd);
    return f.failure == Failure::close && f.wrote ? -EIO : code;
  };
  return io;
}
} // namespace
int main() {
  using namespace ww::fileops;
  const auto root =
      fs::path("build") /
      ("fileops-fixture-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "source");
  fs::create_directories(root / "destination");
  const auto source = root / "source" / "data.bin";
  std::string payload(Transfer::Chunk * 3 + 29, 0);
  for (size_t i = 0; i < payload.size(); ++i)
    payload[i] = char(i * 37);
  put(source, payload);
  Info props{};
  assert(inspect(source.string().c_str(), props) == 0 && props.regular &&
         props.size == payload.size());
  Clipboard clip;
  assert(clip.copy(source.string().c_str()));
  {
    Transfer job;
    assert(job.begin(clip, (root / "destination").string().c_str()));
    job.step();
    assert(job.busy() && job.copied == Transfer::Chunk);
    finish(job);
    assert(job.state == State::complete && get(job.destination) == payload &&
           get(source) == payload);
    Transfer second;
    assert(second.begin(clip, (root / "destination").string().c_str()));
    assert(std::string(second.destination).find("data - Copy.bin") !=
           std::string::npos);
    finish(second);
    assert(second.state == State::complete &&
           get(second.destination) == payload);
    assert(get(root / "destination" / "data.bin") == payload);
    Transfer same;
    assert(same.begin(clip, source.parent_path().string().c_str()));
    assert(std::string(same.destination) != source.string());
    finish(same);
    assert(same.state == State::complete && get(source) == payload);
  }
  const Failure failures[] = {
      Failure::create,      Failure::read,   Failure::write,
      Failure::zero,        Failure::rewind, Failure::sync,
      Failure::permissions, Failure::close,  Failure::verify};
  int test = 0;
  for (auto mode : failures) {
    const auto folder = root / ("failure" + std::to_string(test++));
    fs::create_directory(folder);
    put(folder / "data.bin", "Existing destination is protected");
    Fault fault{mode};
    auto io = faulty(fault);
    Transfer job;
    if (job.begin(clip, folder.string().c_str(), &io))
      finish(job);
    assert(job.state == State::failed);
    assert(get(source) == payload &&
           get(folder / "data.bin") == "Existing destination is protected");
    if (job.destination_created)
      assert(fs::exists(job.destination));
  }
  {
    const auto folder = root / "partial";
    fs::create_directory(folder);
    Fault okay{Failure::none};
    auto io = faulty(okay);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str(), &io));
    finish(job);
    assert(job.state == State::complete && get(job.destination) == payload);
  }
  {
    const auto folder = root / "cancel";
    fs::create_directory(folder);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str()));
    job.step();
    job.cancel();
    assert(job.state == State::cancelled &&
           fs::file_size(job.destination) == Transfer::Chunk &&
           get(source) == payload);
  }
  {
    const auto folder = root / "changed";
    fs::create_directory(folder);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str()));
    job.step();
    auto changed = payload;
    changed[0] ^= 1;
    put(source, changed);
    finish(job);
    assert(job.state == State::failed && get(source) == changed);
    put(source, payload);
    assert(clip.copy(source.string().c_str()));
  }
  {
    const auto folder = root / "source-replaced";
    fs::create_directory(folder);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str()));
    job.step();
    fs::rename(source, source.parent_path() / "held-original.bin");
    put(source, "Replacement source");
    finish(job);
    assert(job.state == State::failed && get(source) == "Replacement source");
    assert(get(source.parent_path() / "held-original.bin") == payload);
    put(source, payload);
    assert(clip.copy(source.string().c_str()));
  }
  {
    const auto folder = root / "destination-replaced";
    fs::create_directory(folder);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str()));
    job.step();
    fs::rename(job.destination, folder / "held-copy.bin");
    put(job.destination, "Replacement destination");
    finish(job);
    assert(job.state == State::failed &&
           get(job.destination) == "Replacement destination" &&
           get(source) == payload);
  }
  {
    const auto folder = root / "empty";
    fs::create_directory(folder);
    const auto empty = root / "empty.txt";
    put(empty, "");
    Clipboard zero;
    assert(zero.copy(empty.string().c_str()));
    Transfer job;
    assert(job.begin(zero, folder.string().c_str()));
    finish(job);
    assert(job.state == State::complete && get(job.destination).empty());
  }
  {
    // Windows-mounted WSL paths disallow renaming a directory with an open
    // child handle. The Linux temporary filesystem permits the actual race.
    const auto folder = fs::temp_directory_path() / (root.filename().string() + "-folder");
    const auto held = fs::temp_directory_path() / (root.filename().string() + "-held");
    fs::create_directory(folder);
    Transfer job;
    assert(job.begin(clip, folder.string().c_str()));
    job.step();
    fs::rename(folder, held);
    fs::create_directory(folder);
    put(folder / "existing.txt", "Untouched replacement folder");
    finish(job);
    assert(job.state == State::failed);
    assert(get(folder / "existing.txt") == "Untouched replacement folder");
    assert(get(source) == payload);
    assert(fs::remove(held / "data.bin"));
    assert(fs::remove(held));
    assert(fs::remove(folder / "existing.txt"));
    assert(fs::remove(folder));
  }
  {
    uint64_t fake_size = Transfer::MaximumBytes + 1;
    auto io = system_io();
    io.context = &fake_size;
    io.info = [](void *ctx, int fd, Info &info) {
      const int code = system_io().info(nullptr, fd, info);
      if (code == 0 && info.regular) info.size = *static_cast<uint64_t *>(ctx);
      return code;
    };
    Clipboard large;
    assert(large.copy(source.string().c_str(), &io));
    Transfer job;
    assert(!job.begin(large, (root / "destination").string().c_str(), &io));
    assert(job.error == EFBIG && !job.destination_created);
    fake_size = 0; // A virtual/pseudo-file may report zero but still yield data.
    assert(large.copy(source.string().c_str(), &io));
    assert(job.begin(large, (root / "destination").string().c_str(), &io));
    finish(job);
    assert(job.state == State::failed && get(source) == payload);
    io.info = [](void *, int fd, Info &info) {
      const int code = system_io().info(nullptr, fd, info);
      info.inode = 0;
      return code;
    };
    Clipboard unverifiable;
    assert(!unverifiable.copy(source.string().c_str(), &io));
  }
#ifndef _WIN32
  {
    const auto link = root / "link.bin";
    assert(symlink(fs::absolute(source).c_str(), link.c_str()) == 0);
    Clipboard unsafe;
    assert(!unsafe.copy(link.string().c_str()) && !unsafe.ready);
    const auto fifo =
        fs::temp_directory_path() / (root.filename().string() + "-fifo");
    assert(mkfifo(fifo.c_str(), 0600) == 0);
    assert(!unsafe.copy(fifo.string().c_str()));
    assert(std::remove(fifo.c_str()) == 0);
    assert(unsafe.copy(source.parent_path().string().c_str()));
    const auto dirlink = root / "directory-link";
    assert(symlink(fs::absolute(root / "destination").c_str(),
                   dirlink.c_str()) == 0);
    Transfer job;
    assert(!job.begin(clip, dirlink.string().c_str()) &&
           !job.destination_created);
  }
#endif
  std::puts("PASS: bounded copy/verification, exclusive no-overwrite naming, "
            "same-folder copy, "
            "failure/cancel retention, source/destination replacement and "
            "link/special-file rejection");
}
