// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace ww {
enum class DocumentResult {
  ok,
  unsaved,
  invalid_path,
  open,
  read,
  too_large,
  binary,
  encoding,
  read_only,
  conflict,
  create,
  write,
  permissions,
  sync,
  close,
  verify,
  replace,
  capacity
};
// All I/O callbacks return a nonnegative result or a negative errno. The real
// implementation uses exclusive creation and atomic replacement; tests can
// inject storage failures without ever weakening those production operations.
struct DocumentIo {
  void *context = nullptr;
  int (*create)(void *, const char *, unsigned) = nullptr;
  int64_t (*write)(void *, int, const char *, size_t) = nullptr;
  int (*permissions)(void *, int, unsigned) = nullptr;
  int (*sync)(void *, int) = nullptr;
  int (*close)(void *, int) = nullptr;
  int (*replace)(void *, const char *, const char *) = nullptr;
  int (*remove)(void *, const char *) = nullptr;
};
const DocumentIo &document_io();
const char *document_result_message(DocumentResult);
struct TextDocument {
  static constexpr size_t Capacity = 64 * 1024;
  char text[Capacity + 1]{}, original[Capacity + 1]{}, path[1024]{};
  size_t length = 0, original_length = 0, caret = 0;
  unsigned mode = 0600;
  bool active = false, read_only = false, bom = false;
  enum class Newline { lf, crlf, cr } newline = Newline::lf;
  DocumentResult result = DocumentResult::ok;
  int error = 0;
  bool dirty() const;
  bool load(const char *, bool discard = false);
  bool save(const DocumentIo *io = nullptr);
  bool close(bool discard = false);
  bool insert(char);
  void erase(bool forward = false);
  void clear();
  void move(int);
  void home(bool whole_document = false);
  void end(bool whole_document = false);
  void vertical(int lines);
  bool fail(DocumentResult, int = 0);
};
} // namespace ww
