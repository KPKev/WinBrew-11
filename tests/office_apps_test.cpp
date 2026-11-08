// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
static void put(const char *p, const char *v) {
  FILE *f = std::fopen(p, "wb");
  assert(f);
  assert(std::fwrite(v, 1, std::strlen(v), f) == std::strlen(v));
  assert(std::fclose(f) == 0);
}
static void capture(ww::Shell &s, const char *path) {
  s.draw();
  FILE *f = std::fopen(path, "wb");
  assert(f);
  std::fprintf(f, "P6\n%d %d\n255\n", ww::W, ww::H);
  for (int i = 0; i < ww::W * ww::H; ++i)
    std::fwrite(s.pixels + i, 1, 3, f);
  std::fclose(f);
}
int main() {
  namespace fs = std::filesystem;
  fs::create_directories("build/office-host/files");
  const char *path = "build/office-host/files/model.csv";
  put(path, "Item,Amount\r\nApples,12\r\nPears,8\r\nTotal,=SUM(B2:B3)\r\n");
  auto model = std::make_unique<ww::SheetDocument>();
  assert(model->load(path));
  assert(model->display(3, 1) == "20");
  assert(model->set(1, 1, "18"));
  assert(model->display(3, 1) == "26");
  assert(model->set(4, 1, "=AVERAGE(B2:B3)"));
  assert(model->display(4, 1) == "13");
  assert(model->set(5, 1, "=MIN(B2:B3)+MAX(B2:B3)*2"));
  assert(model->display(5, 1) == "44");
  assert(model->set(6, 1, "=COUNT(B2:B3)"));
  assert(model->display(6, 1) == "2");
  assert(model->set(7, 1, "=1/0"));
  assert(model->display(7, 1) == "#DIV/0!");
  assert(model->set(8, 1, "=B9"));
  assert(model->display(8, 1) == "#CYCLE!");
  assert(model->set(9, 1, "=A101"));
  assert(model->display(9, 1) == "#REF!");
  assert(model->set(10, 1, "=2*(3+4)-1"));
  assert(model->display(10, 1) == "13");
  assert(model->set(11, 1, "comma, quote\" and\nnew line"));
  assert(model->save());
  assert(!model->dirty());
  auto second = std::make_unique<ww::SheetDocument>();
  assert(second->load(path));
  assert(second->cells[11 * 26 + 1] == model->cells[11 * 26 + 1]);
  put("build/office-host/files/bad.csv", "\"unterminated");
  assert(!second->load("build/office-host/files/bad.csv"));
  assert(second->display(3, 1) == "26");
  assert(model->set(0, 0, "Changed"));
  put(path, "external edit\n");
  assert(!model->save());
  assert(model->storage.result == ww::DocumentResult::conflict);
  auto s = std::make_unique<ww::Shell>();
  assert(s->init("assets"));
  std::snprintf(s->preferences, sizeof(s->preferences),
                "build/office-host/files/preferences.cfg");
  s->input_device = ww::InputDevice::keyboard;
  s->activate(1423);
  assert(s->window == 6);
  std::string first = s->sheet.storage.path;
  assert(fs::exists(first));
  s->activate(1600);
  s->insert_text("12");
  s->physical_key(0x2b, 0, 0);
  assert(s->sheet.cells[0] == "12" && s->sheet.column == 1);
  s->character('=');
  s->insert_text("A1*2");
  s->physical_key(0x28, 0, 0);
  assert(s->sheet.display(0, 1) == "24");
  s->activate(93);
  assert(s->sheet_prompt && s->window == 6);
  s->activate(1532);
  assert(!s->sheet_prompt && s->sheet.dirty());
  s->activate(93);
  s->activate(1530);
  assert(!s->sheet_prompt && !s->sheet.dirty() && s->window == 0);
  s->activate(1423);
  assert(s->window == 6 && first != s->sheet.storage.path);
  assert(s->sheet.cells[0].empty());
  s->sheet.set(0, 0, "Product");
  s->sheet.set(0, 1, "Price");
  s->sheet.set(0, 2, "Quantity");
  s->sheet.set(0, 3, "Total");
  s->sheet.set(1, 0, "Sketchbook");
  s->sheet.set(1, 1, "12");
  s->sheet.set(1, 2, "3");
  s->sheet.set(1, 3, "=B2*C2");
  s->sheet.set(2, 0, "Brushes");
  s->sheet.set(2, 1, "8");
  s->sheet.set(2, 2, "2");
  s->sheet.set(2, 3, "=B3*C3");
  s->sheet.set(4, 0, "Total");
  s->sheet.set(4, 3, "=SUM(D2:D3)");
  capture(*s, "build/office-host/spreadsheet.ppm");
  s->dark = true;
  capture(*s, "build/office-host/spreadsheet-dark.ppm");
  s->dark = false;
  assert(s->count > 128);
  bool last_row = false;
  for (int i = 0; i < s->count; ++i)
    if (s->hits[i].id == 1739)
      last_row = true;
  assert(last_row);
  s->activate(93);
  s->draw();
  bool cancel = false;
  for (int i = 0; i < s->count; ++i)
    if (s->hits[i].id == 1532)
      cancel = true;
  assert(cancel);
  s->activate(1532);
  assert(s->sheet.save());
  s->activate(1421);
  assert(s->window == 4);
  s->insert_text("A little room for your ideas.\n\nNotes, plans and everyday "
                 "things.\nSaved right here in WinBrew 11.");
  capture(*s, "build/office-host/notepad.ppm");
  s->activate(37);
  assert(s->notepad_prompt);
  s->activate(610);
  assert(s->window == 5);
  auto &paint = s->paint;
  paint.blank();
  paint.activate(*s, 733);
  paint.stroke(100, 100, true);
  paint.stroke(280, 210, false);
  paint.end_stroke();
  assert(paint.pixel(100, 100) == paint.color);
  assert(paint.pixel(190, 155) == 0xffffffff);
  paint.undo();
  assert(paint.pixel(100, 100) == 0xffffffff);
  paint.redo();
  assert(paint.pixel(100, 100) == paint.color);
  paint.activate(*s, 732);
  paint.stroke(50, 50, true);
  paint.stroke(75, 75, false);
  paint.stroke(100, 50, false);
  paint.end_stroke();
  assert(paint.pixel(75, 75) == 0xffffffff);
  assert(paint.pixel(75, 50) == paint.color);
  paint.activate(*s, 734);
  paint.stroke(350, 100, true);
  paint.stroke(600, 300, false);
  paint.end_stroke();
  assert(paint.pixel(475, 200) == 0xffffffff);
  paint.activate(*s, 735);
  paint.stroke(100, 100, true);
  assert(paint.color == paint.pixel(100, 100));
  capture(*s, "build/office-host/paint.ppm");
  paint.blank();
  s->activate(1422);
  assert(paint.saving());
  for (int frames = 0; paint.saving() && frames < 100000; ++frames)
    paint.tick(*s);
  assert(!paint.saving() && paint.last_saved[0] &&
         fs::exists(paint.last_saved));
  assert(!paint.dirty());
  s->window = 0;
  s->context = true;
  s->context_submenu = 3;
  s->context_x = 500;
  s->context_y = 180;
  capture(*s, "build/office-host/new-menu.ppm");
  s->window = 0;
  s->activate(1420);
  assert(fs::is_directory("build/office-host/files/Desktop/New folder"));
  s->draw();
  bool desktop_folder = false;
  int desktop_id = -1;
  for (int i = 0; i < s->desktop_file_count; ++i)
    if (s->desktop_files[i].directory &&
        std::strstr(s->desktop_files[i].name, "New folder")) {
      desktop_folder = true;
      desktop_id = 2000 + i;
      break;
    }
  assert(desktop_folder);
  s->input_device=ww::InputDevice::mouse;
  bool clicked=false;for(int i=0;i<s->count;++i)if(s->hits[i].id==desktop_id){const auto r=s->hits[i].r;s->pointer(r.x+r.w/2,r.y+r.h/2,true,false,true);clicked=true;break;}assert(clicked);
  assert(s->window == 1);
  std::string new_parent = s->file_path;
  s->draw();
  s->file_context = true;
  s->file_context_item = false;
  s->file_context_bounds = {700, 400, 320, 224};
  s->file_action(808);
  s->draw();
  bool submenu_folder = false, submenu_text = false;
  for (int i = 0; i < s->count; ++i) {
    if (s->hits[i].id == 1420)
      submenu_folder = true;
    if (s->hits[i].id == 1421)
      submenu_text = true;
  }
  assert(submenu_folder && submenu_text);
  s->activate(1420);
  assert(fs::is_directory(new_parent + "/New folder"));
  assert(s->file_rename_prompt);
  s->activate(2177); // Finish the new-folder naming interaction first.
  s->activate(1421);
  assert(s->window == 4 &&
         std::string(s->document.path).find(new_parent + "/") == 0);
  assert(fs::exists(s->document.path));
  std::puts("Office model, save conflict, CSV, input, dirty prompts and Paint "
            "shape history passed.");
}
