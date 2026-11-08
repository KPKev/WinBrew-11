#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
namespace {
const char *path = "build/notepad-input/document.txt";
void put(const char *p, const char *text) {
  FILE *f = std::fopen(p, "wb");
  assert(f);
  std::fputs(text, f);
  assert(std::fclose(f) == 0);
}
std::string get(const char *p) {
  FILE *f = std::fopen(p, "rb");
  assert(f);
  char bytes[1024]{};
  auto count = std::fread(bytes, 1, sizeof(bytes), f);
  assert(!std::ferror(f));
  std::fclose(f);
  return std::string(bytes, count);
}
ww::Rect hit(ww::Shell &s, int id) {
  s.draw();
  for (int i = s.count - 1; i >= 0; --i)
    if (s.hits[i].id == id)
      return s.hits[i].r;
  assert(false);
  return {};
}
} // namespace
int main() {
  std::filesystem::create_directories("build/notepad-input");
  put(path, "");
  ww::Shell s;
  assert(s.init("assets"));
  s.input_device = ww::InputDevice::keyboard;
  assert(s.notepad_open(path));
  assert(s.window == 4 && s.document.active && !s.touch_keyboard);
  s.physical_key(4);    // a
  s.physical_key(0x28); // Enter creates a new line, without leaving the editor.
  s.physical_key(5);    // b
  s.physical_key(0x2b); // Tab inserts text.
  s.physical_key(6);    // c
  assert(std::string(s.document.text) == "a\nb\tc" && s.document.dirty());
  s.physical_key(0x16, 1); // Ctrl+S opens the editable save-name dialog.
  assert(s.notepad_save_dialog);
  s.notepad_action(607);
  assert(!s.document.dirty() && get(path) == "a\nb\tc");
  s.physical_key(4, 1); // Ctrl+A
  s.physical_key(7);    // d replaces the selected document.
  assert(std::string(s.document.text) == "d" && s.document.dirty());
  s.activate(32);
  assert(s.notepad_prompt && s.window == 4 && s.notepad_pending_action == 32);
  // Underlying apps and right-click cannot replace the unsaved modal.
  s.activate(30);
  s.open_context();
  assert(s.pending == ww::Action::none && !s.context);
  const auto cancel = hit(s, 612);
  s.pointer(cancel.x + cancel.w - 2, cancel.y + 5, true);
  assert(!s.notepad_prompt && s.window == 4 && s.document.dirty());
  assert(std::string(s.document.text) == "d");
  s.document.read_only = true;
  s.activate(32);
  s.notepad_action(610);
  assert(s.dialog && s.notepad_prompt && s.document.dirty() &&
         get(path) == "a\nb\tc");
  const auto okay = hit(s, 60);
  s.pointer(okay.x + 4, okay.y + 4, true);
  assert(!s.dialog && s.notepad_prompt);
  s.key(ww::Key::back);
  assert(!s.notepad_prompt && s.document.dirty());
  s.document.read_only = false;
  s.activate(32);
  const auto save = hit(s, 610);
  s.pointer(save.x + 5, save.y + save.h - 2, true);
  assert(!s.notepad_prompt && s.window == 1 && !s.document.active);
  assert(get(path) == "d");
  // A mouse-selected insertion point and the USB keyboard share one buffer.
  assert(s.notepad_open(path));
  const auto body = hit(s, 600);
  s.input_device = ww::InputDevice::mouse;
  s.pointer(body.x + 12, body.y + 12, true);
  assert(s.document.caret == 0 && !s.touch_keyboard);
  s.physical_key(8); // e
  assert(std::string(s.document.text) == "ed");
  // Selecting the editor with a controller opens its touch keyboard; hardware
  // typing continues to work while that keyboard is displayed.
  s.input_device = ww::InputDevice::controller;
  s.notepad_action(600);
  assert(s.touch_keyboard);
  s.activate(210);   // q
  s.activate(248);   // New line
  s.physical_key(9); // f
  assert(std::string(s.document.text) == "eq\nfd");
  s.key(ww::Key::back); // Hide keyboard, retaining the unsaved document.
  assert(!s.touch_keyboard && s.document.dirty());
  s.key(ww::Key::back);
  assert(s.notepad_prompt && s.window == 4);
  s.notepad_action(611); // Explicit discard.
  assert(!s.document.active && s.window == 1 && get(path) == "d");
  // Choosing a different file while dirty also offers Save / Don't save /
  // Cancel.
  assert(s.notepad_open(path));
  s.character('x');
  put("build/notepad-input/other.txt", "Another document");
  assert(!s.notepad_open("build/notepad-input/other.txt") && s.notepad_prompt);
  s.notepad_action(610);
  assert(s.window == 4 && std::string(s.document.text) == "Another document");
  assert(get(path) == "xd");
  s.character('!');
  s.activate(30); // Game launches wait for unsaved edits.
  assert(s.notepad_prompt && s.pending == ww::Action::none);
  s.notepad_action(612);
  assert(s.pending == ww::Action::none && s.document.dirty());
  s.activate(30);
  s.notepad_action(611);
  assert(s.pending == ww::Action::quake && !s.document.active);
  std::puts(
      "PASS: Notepad USB/touch/mouse input, multiline editing, Ctrl+S, "
      "modal hit surfaces, save/discard/cancel and launch/navigation guards");
}
