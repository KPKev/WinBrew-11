#include "../src/notepad_document.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#endif
namespace {
const char *path = "build/notepad-fixture/document.txt";
void put(const char *name, const std::string &value) {
  FILE *f = std::fopen(name, "wb");
  assert(f && std::fwrite(value.data(), 1, value.size(), f) == value.size());
  assert(std::fclose(f) == 0);
}
std::string get(const char *name) {
  FILE *f = std::fopen(name, "rb");
  assert(f);
  std::string result;
  char bytes[1024];
  while (const auto count = std::fread(bytes, 1, sizeof(bytes), f))
    result.append(bytes, count);
  assert(!std::ferror(f));
  std::fclose(f);
  return result;
}
enum class Failure {
  none,
  create,
  write,
  zero_write,
  permissions,
  sync,
  close,
  verify,
  replace
};
struct Fault {
  Failure failure;
};
ww::DocumentIo faulty(Fault &fault) {
  auto io = ww::document_io();
  io.context = &fault;
  io.create = [](void *ctx, const char *p, unsigned m) {
    return static_cast<Fault *>(ctx)->failure == Failure::create
               ? -EACCES
               : ww::document_io().create(nullptr, p, m);
  };
  io.write = [](void *ctx, int fd, const char *data, size_t count) -> int64_t {
    const auto failure = static_cast<Fault *>(ctx)->failure;
    if (failure == Failure::write)
      return -ENOSPC;
    if (failure == Failure::zero_write)
      return 0;
    if (failure == Failure::verify && count) {
      const char wrong = 'X';
      return ww::document_io().write(nullptr, fd, &wrong, 1);
    }
    return ww::document_io().write(nullptr, fd, data, count > 3 ? 3 : count);
  };
  io.permissions = [](void *ctx, int fd, unsigned mode) {
    return static_cast<Fault *>(ctx)->failure == Failure::permissions
               ? -EPERM
               : ww::document_io().permissions(nullptr, fd, mode);
  };
  io.sync = [](void *ctx, int fd) {
    return static_cast<Fault *>(ctx)->failure == Failure::sync
               ? -EIO
               : ww::document_io().sync(nullptr, fd);
  };
  io.close = [](void *ctx, int fd) {
    const int result = ww::document_io().close(nullptr, fd);
    return static_cast<Fault *>(ctx)->failure == Failure::close ? -EIO : result;
  };
  io.replace = [](void *ctx, const char *from, const char *to) {
    return static_cast<Fault *>(ctx)->failure == Failure::replace
               ? -EACCES
               : ww::document_io().replace(nullptr, from, to);
  };
  return io;
}
} // namespace
int main() {
  std::filesystem::create_directories("build/notepad-fixture");
  ww::TextDocument d;
  const std::string bom = "\xef\xbb\xbf";
  put(path, bom + "First\r\nSecond\r\n");
  assert(d.load(path) && d.bom && !d.dirty());
  assert(d.newline == ww::TextDocument::Newline::crlf);
  d.end(true);
  assert(d.insert('!') && d.dirty());
  assert(d.insert('\n'));
  assert(std::string(d.text) == "First\r\nSecond\r\n!\r\n");
  d.erase();
  assert(std::string(d.text) == "First\r\nSecond\r\n!");
  assert(d.save() && !d.dirty());
  assert(get(path) == bom + "First\r\nSecond\r\n!");
  d.home(true);
  d.vertical(1);
  assert(d.caret == 7);
  d.move(-1);
  assert(d.caret == 5);
  d.move(1);
  assert(d.caret == 7);
  d.insert('s');
  assert(!d.close() && d.result == ww::DocumentResult::unsaved);
  assert(!d.load("missing.txt") && d.result == ww::DocumentResult::unsaved);
  assert(d.close(true));
  const Failure failures[] = {Failure::create,     Failure::write,
                              Failure::zero_write, Failure::permissions,
                              Failure::sync,       Failure::close,
                              Failure::verify,     Failure::replace};
  for (auto failure : failures) {
    put(path, "Original contents\n");
    assert(d.load(path));
    d.end(true);
    assert(d.insert('!'));
    Fault fault{failure};
    auto io = faulty(fault);
    assert(!d.save(&io) && d.dirty());
    assert(get(path) == "Original contents\n");
    assert(std::string(d.text) == "Original contents\n!");
    for (const auto &entry :
         std::filesystem::directory_iterator("build/notepad-fixture"))
      assert(entry.path().filename().string().find(".wine11-note-") != 0);
    d.close(true);
  }
  put(path, "Partial writes are retried completely.");
  assert(d.load(path));
  d.end(true);
  d.insert('!');
  Fault good{Failure::none};
  auto io = faulty(good);
  assert(d.save(&io) && get(path) == "Partial writes are retried completely.!");
  d.insert('?');
  put(path, "Changed by another program");
  assert(!d.save() && d.result == ww::DocumentResult::conflict);
  assert(get(path) == "Changed by another program");
  d.close(true);
  put(path, std::string(ww::TextDocument::Capacity, 'a'));
  assert(d.load(path));
  assert(!d.insert('x') && d.result == ww::DocumentResult::capacity &&
         !d.dirty());
  put("build/notepad-fixture/large.txt",
      std::string(ww::TextDocument::Capacity + 1, 'b'));
  assert(!d.load("build/notepad-fixture/large.txt") &&
         d.result == ww::DocumentResult::too_large &&
         d.length == ww::TextDocument::Capacity);
  assert(get("build/notepad-fixture/large.txt").size() ==
         ww::TextDocument::Capacity + 1);
  put("build/notepad-fixture/binary.bin", std::string("TEXT\0BINARY", 11));
  assert(!d.load("build/notepad-fixture/binary.bin") &&
         d.result == ww::DocumentResult::binary);
  put("build/notepad-fixture/unicode.txt", "Hello \xc3\xa9");
  assert(!d.load("build/notepad-fixture/unicode.txt") &&
         d.result == ww::DocumentResult::encoding);
  assert(d.length == ww::TextDocument::Capacity);
  d.close(true);
#ifndef _WIN32
  put(path, "Read-only text");
  assert(chmod(path, 0444) == 0);
  assert(d.load(path) && d.read_only);
  d.insert('!');
  assert(!d.save() && d.result == ww::DocumentResult::read_only);
  assert(get(path) == "Read-only text");
  assert(chmod(path, 0600) == 0);
#endif
  std::puts(
      "PASS: text editing, CRLF/BOM preservation, atomic save, partial writes, "
      "all precommit failure stages, conflicts, readonly and binary/size "
      "guards");
}
