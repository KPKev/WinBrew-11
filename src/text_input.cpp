// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cstdio>
#include <cstring>
namespace ww {
static char *buffer(Shell &s) {
  if (s.text_target == TextTarget::document)
    return s.document.text;
  if (s.text_target == TextTarget::vapor_search)
    return s.vapor_search;
  return s.text_target == TextTarget::address ? s.address : s.search;
}
static size_t capacity(const Shell &s) {
  if (s.text_target == TextTarget::document)
    return TextDocument::Capacity + 1;
  if (s.text_target == TextTarget::vapor_search)
    return sizeof(s.vapor_search);
  return s.text_target == TextTarget::address ? sizeof(s.address)
                                              : sizeof(s.search);
}
void Shell::begin_text(TextTarget target) {
  text_target = target;
  address_edit = target == TextTarget::address;
  text_caret = target == TextTarget::document ? int(document.caret)
                                              : int(std::strlen(buffer(*this)));
  text_selection = false;
  touch_keyboard = input_device == InputDevice::controller;
  keyboard_shift = keyboard_symbols = false;
  calendar = quick = tray = context = false;
  if (target == TextTarget::search) {
    start = all = true;
  }
}
void Shell::end_text() {
  text_target = TextTarget::none;
  address_edit = false;
  touch_keyboard = false;
  text_selection = false;
}
void Shell::clear_text() {
  if (text_target == TextTarget::none)
    return;
  if (text_target == TextTarget::document)
    document.clear();
  else
    buffer(*this)[0] = 0;
  text_caret = 0;
  text_selection = false;
  if (text_target == TextTarget::vapor_search) { steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0; }
}
void Shell::insert_text(const char *value) {
  for (; *value; ++value)
    character(*value);
}
void Shell::move_caret(int delta) {
  if (text_target == TextTarget::none)
    return;
  int n = int(std::strlen(buffer(*this)));
  if (text_selection) {
    text_caret = delta < 0 ? 0 : n;
    if (text_target == TextTarget::document)
      document.caret = size_t(text_caret);
    text_selection = false;
    return;
  }
  if (text_target == TextTarget::document) {
    document.move(delta);
    text_caret = int(document.caret);
    return;
  }
  text_caret += delta;
  text_caret = text_caret < 0 ? 0 : text_caret > n ? n : text_caret;
}
void Shell::erase_text(bool forward) {
  if (text_target == TextTarget::none)
    return;
  auto *text = buffer(*this);
  size_t n = std::strlen(text);
  if (text_selection) {
    clear_text();
    return;
  }
  if (text_target == TextTarget::document) {
    document.erase(forward);
    text_caret = int(document.caret);
    return;
  }
  text_caret = text_caret < 0 ? 0 : text_caret > int(n) ? int(n) : text_caret;
  if (forward) {
    if (size_t(text_caret) < n)
      std::memmove(text + text_caret, text + text_caret + 1, n - text_caret);
  } else if (text_caret > 0) {
    std::memmove(text + text_caret - 1, text + text_caret, n - text_caret + 1);
    --text_caret;
  }
  if (text_target == TextTarget::vapor_search) { steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0; }
}
void Shell::character(char c) {
  if (window == 5 || paint.modal()) return;
  if (dialog || notepad_prompt)
    return;
  if (text_target == TextTarget::document ||
      (window == 4 && document.active && text_target == TextTarget::none)) {
    if (c != '\n' && c != '\t' && (c < 32 || c > 126))
      return;
    if (text_target != TextTarget::document) {
      input_device = InputDevice::keyboard;
      begin_text(TextTarget::document);
    }
    if (text_selection)
      clear_text();
    if (!document.insert(c))
      notepad_error();
    text_caret = int(document.caret);
    focus = -1;
    return;
  }
  if (c < 32 || c > 126)
    return;
  if (text_target == TextTarget::none) {
    input_device = InputDevice::keyboard;
    begin_text(window == 3 && address_edit ? TextTarget::address
                                           : TextTarget::search);
  }
  if (text_selection)
    clear_text();
  char *text = buffer(*this);
  size_t n = std::strlen(text);
  if (n >= capacity(*this) - 1)
    return;
  text_caret = text_caret < 0 ? 0 : text_caret > int(n) ? int(n) : text_caret;
  if (text_target == TextTarget::search && c >= 'A' && c <= 'Z')
    c += 32;
  std::memmove(text + text_caret + 1, text + text_caret, n - text_caret + 1);
  text[text_caret++] = c;
  focus = -1;
  if (text_target == TextTarget::vapor_search) { steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0; }
  if (text_target == TextTarget::search)
    start = all = true;
}
void Shell::commit_text() {
  if (text_target == TextTarget::vapor_search) { end_text(); return; }
  if (text_target == TextTarget::document) {
    character('\n');
    return;
  }
  if (text_target == TextTarget::address) {
    activate(131);
    return;
  }
  touch_keyboard = false;
  if (!search[0])
    return;
  const char *names[] = {"quake ii rtx", "game library games", "file explorer",
                         "settings", "web chrome browser google"};
  for (int i = 0; i < 5; ++i)
    if (std::strstr(names[i], search)) {
      activate(i == 1 ? 35 : 30 + i);
      return;
    }
}
static char printable(uint16_t key, bool shift, bool caps) {
  if (key >= 4 && key <= 0x1d)
    return char(((shift != caps) ? 'A' : 'a') + key - 4);
  if (key >= 0x1e && key <= 0x27)
    return (shift ? "!@#$%^&*()" : "1234567890")[key - 0x1e];
  if (key >= 0x59 && key <= 0x62)
    return "1234567890"[key - 0x59];
  switch (key) {
  case 0x2c:
    return ' ';
  case 0x2d:
    return shift ? '_' : '-';
  case 0x2e:
    return shift ? '+' : '=';
  case 0x2f:
    return shift ? '{' : '[';
  case 0x30:
    return shift ? '}' : ']';
  case 0x31:
    return shift ? '|' : '\\';
  case 0x33:
    return shift ? ':' : ';';
  case 0x34:
    return shift ? '"' : '\'';
  case 0x35:
    return shift ? '~' : '`';
  case 0x36:
    return shift ? '<' : ',';
  case 0x37:
    return shift ? '>' : '.';
  case 0x38:
    return shift ? '?' : '/';
  case 0x54:
    return '/';
  case 0x55:
    return '*';
  case 0x56:
    return '-';
  case 0x57:
    return '+';
  case 0x63:
    return '.';
  default:
    return 0;
  }
}
void Shell::physical_key(uint16_t usage, uint8_t modifiers, uint32_t locks) {
  input_device = InputDevice::keyboard;
  bool shift = (modifiers & 0x22) != 0, control = (modifiers & 0x11) != 0;
  if (!window && !dialog && !file_context && !file_properties &&
      !file_transfer.busy() && !start && !calendar && !quick && !tray &&
      text_target == TextTarget::none && control && shift &&
      !(modifiers & 0xcc) && usage >= 0x1f && usage <= 0x21) {
    icon_size = 2 - (usage - 0x1f);
    context = false;
    save_preferences();
    return;
  }
  if (window == 5 && control && !dialog && !paint.modal()) {
    if (usage == 0x16) { paint.activate(*this, 702); return; }
    if (usage == 0x1d) { if (shift) paint.redo(); else paint.undo(); return; }
    if (usage == 0x1c) { paint.redo(); return; }
  }
  if (control && usage == 0x16 && document.active && !dialog &&
      !notepad_prompt) {
    notepad_action(601); // Ctrl+S
    return;
  }
  if (control && usage == 0x1a && window == 4 && !dialog && !notepad_prompt) {
    notepad_close(); // Ctrl+W
    return;
  }
  if (text_target == TextTarget::document && !dialog && !notepad_prompt) {
    if (usage == 0x4a || usage == 0x4d) {
      if (usage == 0x4a)
        document.home(control);
      else
        document.end(control);
      text_caret = int(document.caret);
      text_selection = false;
      return;
    }
    if (!control &&
        (usage == 0x52 || usage == 0x51 || usage == 0x4b || usage == 0x4e)) {
      document.vertical(usage == 0x52   ? -1
                        : usage == 0x51 ? 1
                        : usage == 0x4b ? -8
                                        : 8);
      text_caret = int(document.caret);
      text_selection = false;
      return;
    }
    if (!control && !shift && usage == 0x2b) {
      character('\t');
      return;
    }
    if (!control && (usage == 0x28 || usage == 0x58) &&
        (focus < 0 || focus == 600 || (focus >= 200 && focus <= 248))) {
      commit_text();
      return;
    }
  }
  if (control && usage == 0x0f && !dialog) {
    activate(34);
    begin_text(TextTarget::address);
    text_selection = true;
    return;
  }
  if (control && usage == 4 && text_target != TextTarget::none) {
    text_selection = true;
    return;
  }
  if (control || (modifiers & 0xcc))
    return;
  switch (usage) {
  case 0x3a:
    key(Key::start);
    return;
  case 0x3b:
    key(Key::library);
    return;
  case 0x28:
  case 0x58:
    if (text_target != TextTarget::none && !dialog &&
        (focus < 0 || focus == 43 || focus == 130 ||
         (focus >= 200 && focus <= 248)))
      commit_text();
    else
      key(Key::accept);
    return;
  case 0x2c:
    if (text_target == TextTarget::none && focus >= 0) {
      key(Key::accept);
      return;
    }
    break;
  case 0x29:
    key(Key::back);
    return;
  case 0x2b:
    key(shift ? Key::previous : Key::next);
    return;
  case 0x2a:
    if (text_target != TextTarget::none)
      erase_text();
    else
      key(Key::erase);
    return;
  case 0x4c:
    erase_text(true);
    return;
  case 0x4a:
    if (text_target != TextTarget::none) {
      text_caret = 0;
      text_selection = false;
    }
    return;
  case 0x4d:
    if (text_target != TextTarget::none) {
      text_caret = int(std::strlen(buffer(*this)));
      text_selection = false;
    }
    return;
  case 0x50:
    if (text_target != TextTarget::none)
      move_caret(-1);
    else
      key(Key::left);
    return;
  case 0x4f:
    if (text_target != TextTarget::none)
      move_caret(1);
    else
      key(Key::right);
    return;
  case 0x52:
    key(Key::up);
    return;
  case 0x51:
    key(Key::down);
    return;
  default:
    break;
  }
  if (char c = printable(usage, shift, (locks & 2) != 0))
    character(c);
}
void Shell::draw_text_field(int x, int y, int available, TextTarget target,
                            const char *placeholder) {
  const char *value = target == TextTarget::address ? address : search;
  bool active = text_target == target;
  if (!value[0]) {
    text(x, y, placeholder, 0, muted());
    if (active)
      rect({x, y - 1, 2, 23}, accent_color());
    return;
  }
  if (!active) {
    text_fit(x, y, available, value, 0, ink());
    return;
  }
  char prefix[256]{};
  int n = int(std::strlen(value)), caret = text_caret < 0   ? 0
                                           : text_caret > n ? n
                                                            : text_caret;
  int first = 0;
  std::memcpy(prefix, value, caret);
  while (first < caret && width(prefix + first) > available - 12)
    ++first;
  char visible[256];
  std::snprintf(visible, sizeof(visible), "%s", value + first);
  while (width(visible) > available && visible[0])
    visible[std::strlen(visible) - 1] = 0;
  if (text_selection)
    round({x - 2, y - 3, width(visible) + 4, 27}, 2, accent_color());
  text(x, y, visible, 0, text_selection ? 0xffffffff : ink());
  if (!text_selection)
    rect({x + width(prefix + first), y - 1, 2, 23}, accent_color());
}
} // namespace ww
