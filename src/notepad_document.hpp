// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include "xlsx_codec.hpp"
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
  bool active = false, read_only = false, bom = false, original_bom = false;
  enum class Newline { lf, crlf, cr } newline = Newline::lf;
  DocumentResult result = DocumentResult::ok;
  int error = 0;
  struct EditState { std::string text; size_t caret; };
  std::vector<EditState> undo_history, redo_history;
  void checkpoint();
  bool undo();
  bool redo();
  bool dirty() const;
  bool load(const char *, bool discard = false);
  bool save(const DocumentIo *io = nullptr);
  bool save_as(const char *, const TextDocument *confirmed_target = nullptr,
               const DocumentIo *io = nullptr);
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
// Portable spreadsheet model; CSV storage shares the editor's verified atomic I/O.
struct SheetDocument {
  static constexpr int Rows = 100, Columns = 26;
  TextDocument storage;
  xlsx::Workbook workbook;
  bool xlsx_format = false;
  int selected_sheet = 0;
  std::array<std::string, Rows * Columns> cells{};
  mutable std::array<std::string, Rows * Columns> calculated{};
  mutable std::array<bool, Rows * Columns> cached{};
  int row = 0, column = 0, first_row = 0, first_column = 0;
  char edit[256]{}, status[180] = "Ready";
  bool editing = false;
  bool load(const char *);
  bool set(int row, int column, const char *);
  bool save(const DocumentIo *io = nullptr);
  bool dirty() const { return storage.active && (xlsx_format ? workbook.changed : storage.dirty()); }
  static bool supports(const char *);
  bool load_xlsx(const char *);
  bool save_xlsx(const DocumentIo *);
  bool export_xlsx(const char *);
  bool select_sheet(int);
  bool add_sheet();
  bool editable() const;
  std::string display(int row, int column) const;
  double number(int row, int column, int depth, int &budget, std::string &error) const;
};
} // namespace ww
