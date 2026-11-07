// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cstdio>
#include <cstring>
namespace ww {
namespace {
bool leaves_editor(int id) {
  return (id >= 10 && id <= 13) || (id >= 30 && id <= 39) ||
         (id >= 100 && id <= 108) || (id >= 400 && id <= 409) ||
         (id >= 500 && id < 510) || (id >= 700 && id <= 749) || id == 93 ||
         id == 602 || id == 800;
}
size_t next_line(const TextDocument &d, size_t at) {
  while (at < d.length && d.text[at] != '\r' && d.text[at] != '\n')
    ++at;
  if (at < d.length && d.text[at++] == '\r' && at < d.length &&
      d.text[at] == '\n')
    ++at;
  return at;
}
void position(const TextDocument &d, int &line, int &column) {
  line = column = 0;
  for (size_t at = 0; at < d.caret && at < d.length; ++at) {
    if (d.text[at] == '\r' || d.text[at] == '\n') {
      if (d.text[at] == '\r' && at + 1 < d.caret && d.text[at + 1] == '\n')
        ++at;
      ++line;
      column = 0;
    } else
      ++column;
  }
}
} // namespace
void Shell::notepad_error() {
  dialog = true;
  focus = 60;
  std::snprintf(message, sizeof(message), "%s%s",
                document_result_message(document.result),
                document.error
                    ? " Check that the location is writable and connected."
                    : "");
}
bool Shell::notepad_open(const char *path) {
  if (!path || !*path)
    return false;
  if (document.active && !std::strcmp(document.path, path)) {
    window = 4;
    begin_text(TextTarget::document);
    return true;
  }
  if (document.dirty()) {
    std::snprintf(notepad_pending_path, sizeof(notepad_pending_path), "%s",
                  path);
    notepad_pending_action = -2;
    notepad_prompt = true;
    window = 4;
    end_text();
    focus = 612;
    return false;
  }
  if (!document.load(path)) {
    notepad_error();
    return false;
  }
  notepad_first_line = notepad_first_column = 0;
  window = 4;
  start = calendar = context = quick = tray = false;
  begin_text(TextTarget::document);
  return true;
}
bool Shell::notepad_guard(int id) {
  if (notepad_prompt)
    return id == 60;
  if (!leaves_editor(id))
    return true;
  if (!document.dirty()) {
    if (document.active)
      document.close();
    return true;
  }
  notepad_pending_action = id;
  notepad_pending_path[0] = 0;
  notepad_prompt = true;
  window = 4;
  start = calendar = context = quick = tray = false;
  end_text();
  focus = 612;
  return false;
}
void Shell::notepad_close() {
  if (!notepad_guard(602))
    return;
  document.close();
  end_text();
  window = 1;
  focus = -1;
}
void Shell::notepad_action(int id) {
  if (id >= 610 && id <= 612) {
    if (!notepad_prompt)
      return;
    if (id == 612) {
      notepad_prompt = false;
      notepad_pending_action = -1;
      notepad_pending_path[0] = 0;
      window = 4;
      begin_text(TextTarget::document);
      return;
    }
    if (id == 610 && !document.save()) {
      notepad_error();
      return;
    }
    const int action = notepad_pending_action;
    char path[1024];
    std::snprintf(path, sizeof(path), "%s", notepad_pending_path);
    if (id == 611)
      document.close(true);
    notepad_prompt = false;
    notepad_pending_action = -1;
    notepad_pending_path[0] = 0;
    if (action == -2) {
      notepad_open(path);
      return;
    }
    if (action == 602) {
      notepad_close();
      return;
    }
    activate(action);
    return;
  }
  if (notepad_prompt)
    return;
  if (!document.active)
    return;
  if (id == 600) {
    if (input_device == InputDevice::mouse && notepad_body.has(mx, my)) {
      int row = (my - notepad_body.y - 12) / 28 + notepad_first_line;
      size_t at = 0;
      for (int i = 0; i < row && at < document.length; ++i)
        at = next_line(document, at);
      size_t finish = at;
      while (finish < document.length && document.text[finish] != '\r' &&
             document.text[finish] != '\n')
        ++finish;
      at += size_t(notepad_first_column) < finish - at
                ? size_t(notepad_first_column)
                : finish - at;
      const int wanted = mx - notepad_body.x - 12;
      int offset = 0;
      while (at < finish) {
        char glyph[2] = {document.text[at] == '\t' ? ' ' : document.text[at],
                         0};
        const int advance = width(glyph);
        if (offset + advance / 2 >= wanted)
          break;
        offset += advance;
        ++at;
      }
      document.caret = at;
    }
    begin_text(TextTarget::document);
  }
  if (id == 601) {
    if (!document.save())
      notepad_error();
  }
  if (id == 602)
    notepad_close();
  if (id == 603)
    document.vertical(-8);
  if (id == 604)
    document.vertical(8);
  if (id == 605) {
    begin_text(TextTarget::document);
    text_selection = true;
  }
}
void Shell::notepad_prompt_ui() {
  if (!notepad_prompt)
    return;
  rect({0, 0, W, H}, 0x55000000);
  Rect modal{540, 382, 840, 286};
  panel(modal);
  text(modal.x + 30, modal.y + 28, "Save changes to your document?", 1, ink());
  const char *name = std::strrchr(document.path, '/');
  text_fit(modal.x + 30, modal.y + 79, modal.w - 60,
           name ? name + 1 : document.path, 0, muted());
  text(modal.x + 30, modal.y + 125, "Your edits are still open in Notepad.", 0,
       muted());
  button({modal.x + 302, modal.y + 206, 146, 44}, 610, "Save");
  button({modal.x + 462, modal.y + 206, 146, 44}, 611, "Don't save");
  button({modal.x + 622, modal.y + 206, 180, 44}, 612, "Cancel");
}
void Shell::notepad_ui(Rect wr) {
  const int x = wr.x + 22, y = wr.y + 68;
  const char *name = std::strrchr(document.path, '/');
  char title[300];
  std::snprintf(title, sizeof(title), "%s%.256s", document.dirty() ? "* " : "",
                name ? name + 1 : document.path);
  text_fit(x + 4, y, wr.w - 410, title, 1, ink());
  button({wr.x + wr.w - 350, y - 8, 100, 42}, 601, "Save");
  button({wr.x + wr.w - 240, y - 8, 110, 42}, 605, "Select all");
  button({wr.x + wr.w - 120, y - 8, 96, 42}, 602, "Close");
  Rect paper{x, y + 56, wr.w - 44, wr.h - 190};
  if (touch_keyboard && paper.y + paper.h > 610)
    paper.h = 610 - paper.y;
  if (paper.h < 80)
    paper.h = 80;
  notepad_body = paper;
  round(paper, 5, dark ? 0xff292623 : 0xffffffff);
  add(paper, 600);
  const int visible_lines = (paper.h - 26) / 28;
  int caret_line = 0, caret_column = 0;
  position(document, caret_line, caret_column);
  if (caret_line < notepad_first_line)
    notepad_first_line = caret_line;
  if (caret_line >= notepad_first_line + visible_lines)
    notepad_first_line = caret_line - visible_lines + 1;
  if (notepad_first_line < 0)
    notepad_first_line = 0;
  size_t at = 0;
  for (int row = 0; row < notepad_first_line && at < document.length; ++row)
    at = next_line(document, at);
  for (int row = 0; row < visible_lines; ++row) {
    size_t end = at;
    while (end < document.length && document.text[end] != '\r' &&
           document.text[end] != '\n')
      ++end;
    const bool caret_here = caret_line == notepad_first_line + row;
    // Horizontal movement follows the caret and leaves all bytes intact.
    size_t first = caret_here ? size_t(notepad_first_column) : 0;
    if (first > end - at)
      first = end - at;
    if (caret_here) {
      if (document.caret < at + first)
        first = document.caret - at;
      char prefix[128];
      while (first < document.caret - at) {
        size_t length = document.caret - at - first;
        if (length >= sizeof(prefix)) {
          ++first;
          continue;
        }
        std::memcpy(prefix, document.text + at + first, length);
        prefix[length] = 0;
        if (width(prefix) <= paper.w - 42)
          break;
        ++first;
      }
      notepad_first_column = int(first);
    }
    char line[160];
    size_t count = end - at - first;
    if (count >= sizeof(line))
      count = sizeof(line) - 1;
    std::memcpy(line, document.text + at + first, count);
    for (size_t i = 0; i < count; ++i)
      if (line[i] == '\t')
        line[i] = ' ';
    line[count] = 0;
    while (count && width(line) > paper.w - 28)
      line[--count] = 0;
    const int top = paper.y + 12 + row * 28;
    if (text_selection && count)
      round({paper.x + 10, top, width(line) + 4, 25}, 2, accent_color());
    text(paper.x + 12, top, line, 0, text_selection ? 0xffffffff : ink());
    if (caret_here && !text_selection && text_target == TextTarget::document) {
      size_t prefix = document.caret - at - first;
      if (prefix > count)
        prefix = count;
      line[prefix] = 0;
      rect({paper.x + 12 + width(line), top, 2, 24}, accent_color());
    }
    if (end == document.length)
      break;
    at = next_line(document, at);
  }
  char status[200];
  std::snprintf(status, sizeof(status),
                "Ln %d, Col %d  |  %zu / 65536 bytes  |  %s%s", caret_line + 1,
                caret_column + 1, document.length,
                document.read_only ? "Read-only"
                : document.dirty() ? "Unsaved changes"
                                   : "Saved",
                document.bom ? "  |  UTF-8 BOM" : "  |  ASCII");
  text_fit(x + 4, paper.y + paper.h + 15, wr.w - 280, status, 0, muted());
  button({wr.x + wr.w - 226, paper.y + paper.h + 5, 94, 38}, 603, "Page up");
  button({wr.x + wr.w - 122, paper.y + paper.h + 5, 98, 38}, 604, "Page down");
}
} // namespace ww
