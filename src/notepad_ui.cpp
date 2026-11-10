// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cerrno>
#include "privilege.hpp"
#include "notepad_search.hpp"
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <algorithm>
namespace ww {
namespace {
bool leaves_editor(int id) {
  return (id >= 10 && id <= 13) || (id >= 30 && id <= 39) ||
         (id >= 100 && id <= 108) || ((id >= 400 && id <= 409)||(id>=420&&id<=436)) ||
         (id >= 500 && id < 580) || (id >= 700 && id <= 749) || id == 93 ||
         id == 602 || id == 800 || (id>=2000&&id<2064) || (id >= 1420 && id <= 1431);
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
void Shell::notepad_name_init() {
  const char *slash=std::strrchr(document.path,'/');
  std::snprintf(notepad_filename,sizeof(notepad_filename),"%.255s",document.path[0]?(slash?slash+1:document.path):"Untitled.txt");
  notepad_save_as_mode=false;notepad_replace=false;notepad_save_target.reset();
}
bool Shell::notepad_modal_hit(int id) const {
  if(touch_keyboard && !notepad_replace && id>=200&&id<=248)return true;
  if(notepad_replace)return id==609||id==619;
  if(id==618)return true;
  return notepad_prompt?(id>=610&&id<=612):(id==607||id==608);
}
void Shell::notepad_save_finished() {
  const int action=notepad_pending_action;
  char path[1024];std::snprintf(path,sizeof(path),"%s",notepad_pending_path);
  notepad_prompt=notepad_save_dialog=notepad_replace=false;
  notepad_pending_action=-1;notepad_pending_path[0]=0;notepad_save_target.reset();end_text();
  desktop_files_next_scan=0;
  if(action==-2){notepad_open(path);return;}
  if(action==-3){notepad_tab_remove();return;}
  if(action==602){notepad_close();return;}
  if(action>=0){activate(action);return;}
  window=4;begin_text(TextTarget::document);
}
void Shell::notepad_save_submit() {
  const std::string name=notepad_filename;
  if(name.empty() || name=="." || name==".." || name.back()==' ' || name.back()=='.' || name.find_first_of("/\\:*?\"<>|")!=std::string::npos){
    dialog=true;focus=60;std::snprintf(message,sizeof(message),"Enter a file name without slashes or reserved filename characters.");return;
  }
  std::string filename=name;
  if(filename.find('.')==std::string::npos)filename+=".txt";
  if(filename.size()>255){dialog=true;focus=60;std::snprintf(message,sizeof(message),"That file name is too long.");return;}
  const char *slash=std::strrchr(document.path,'/');
  const int parent=slash?int(slash-document.path+1):0;
  char folder[1024]{};
  if (!document.path[0]) desktop_folder(folder, sizeof(folder));
  int n=document.path[0] ? std::snprintf(notepad_save_path,sizeof(notepad_save_path),"%.*s%s",parent,document.path,filename.c_str()) : std::snprintf(notepad_save_path,sizeof(notepad_save_path),"%s/%s",folder,filename.c_str());
  if (!document.path[0] && !folder[0]) { document.fail(DocumentResult::invalid_path); notepad_error(); return; }
  if(n<0 || n>=int(sizeof(notepad_save_path))){document.fail(DocumentResult::invalid_path);notepad_error();return;}
  if(!notepad_save_as_mode && !std::strcmp(notepad_save_path,document.path)){
    if(document.save())notepad_save_finished();else notepad_error();return;
  }
  std::snprintf(notepad_filename,sizeof(notepad_filename),"%s",filename.c_str());
  fileops::Info target{};
  const int found=fileops::inspect(notepad_save_path,target);
  if(found==0){
    if(!target.regular){document.fail(DocumentResult::invalid_path);notepad_error();return;}
    notepad_save_target=std::make_unique<TextDocument>();
    if(!notepad_save_target->load(notepad_save_path)){
      document.fail(notepad_save_target->result,notepad_save_target->error);notepad_save_target.reset();notepad_error();return;
    }
    notepad_replace=true;end_text();focus=619;return;
  }
  if(found!=-ENOENT){document.fail(DocumentResult::create,-found);notepad_error();return;}
  if(document.save_as(notepad_save_path))notepad_save_finished();else notepad_error();
}
void Shell::notepad_error() {
  dialog = true;
  focus = 60;
  std::snprintf(message, sizeof(message), "%s%s",
                document_result_message(document.result),
                document.error
                    ? " Check that the location is writable and connected."
                    : "");
}
void Shell::notepad_tab_init() {
  if (notepad_tabs.empty() && document.active) { notepad_tabs.push_back(std::make_unique<TextDocument>(document)); notepad_tab = 0; }
}
void Shell::notepad_tab_switch(int index) {
  notepad_tab_init();
  if (index < 0 || index >= int(notepad_tabs.size()) || index == notepad_tab) return;
  *notepad_tabs[notepad_tab] = document;
  document = *notepad_tabs[index]; notepad_tab = index;
  notepad_first_line = notepad_first_column = 0; notepad_match = size_t(-1);
  begin_text(TextTarget::document);
}
void Shell::notepad_tab_new() {
  notepad_tab_init();
  if (notepad_tabs.size() >= 8) { dialog=true; focus=60; std::snprintf(message,sizeof(message),"Close a tab before opening another."); return; }
  if (!notepad_tabs.empty()) *notepad_tabs[notepad_tab] = document;
  auto blank = std::make_unique<TextDocument>(); blank->active = true;
  notepad_tabs.push_back(std::move(blank)); notepad_tab = int(notepad_tabs.size())-1;
  document = *notepad_tabs[notepad_tab]; notepad_first_line = notepad_first_column = 0;
  notepad_match = size_t(-1); begin_text(TextTarget::document);
}
void Shell::notepad_tab_remove() {
  notepad_tab_init();
  if (notepad_tabs.size() <= 1) { notepad_tabs.clear(); document.close(true); end_text(); window=1; focus=-1; return; }
  notepad_tabs.erase(notepad_tabs.begin()+notepad_tab);
  if (notepad_tab >= int(notepad_tabs.size())) notepad_tab = int(notepad_tabs.size())-1;
  document = *notepad_tabs[notepad_tab]; notepad_first_line = notepad_first_column = 0;
  begin_text(TextTarget::document);
}
bool Shell::notepad_open(const char *path) {
  if (!path || !*path)
    return false;
  if (document.active && !std::strcmp(document.path, path)) {
    window = 4;
    begin_text(TextTarget::document);
    return true;
  }
  notepad_tab_init();
  for (int i=0;i<int(notepad_tabs.size());++i) if (i != notepad_tab && !std::strcmp(notepad_tabs[i]->path,path)) { notepad_tab_switch(i); window=4; return true; }
  auto incoming=std::make_unique<TextDocument>();
  if (!incoming->load(path)) { document.fail(incoming->result,incoming->error); notepad_error(); return false; }
  if (notepad_tabs.size() >= 8) { dialog=true;focus=60;std::snprintf(message,sizeof(message),"Close a tab before opening another.");return false; }
  if (!notepad_tabs.empty()) *notepad_tabs[notepad_tab]=document;
  notepad_tabs.push_back(std::move(incoming));notepad_tab=int(notepad_tabs.size())-1;document=*notepad_tabs[notepad_tab];
  notepad_first_line = notepad_first_column = 0;
  window = 4;
  start = calendar = context = quick = tray = false;
  begin_text(TextTarget::document);
  return true;
}
bool Shell::notepad_guard(int id) {
  if(id==36&&document.active)return true;
  if (notepad_modal())
    return id == 60 || notepad_modal_hit(id);
  if (!leaves_editor(id))
    return true;
  notepad_tab_init();
  if (!document.dirty()) {
    for (int i=0;i<int(notepad_tabs.size());++i) if(i!=notepad_tab && notepad_tabs[i]->dirty()) { notepad_tab_switch(i); break; }
  }
  if (!document.dirty()) {
    if (document.active)
      document.close();
    notepad_tabs.clear();
    return true;
  }
  notepad_pending_action = id;
  notepad_pending_path[0] = 0;
  notepad_prompt = true;notepad_name_init();
  window = 4;
  start = calendar = context = quick = tray = false;
  end_text();
  focus = 612;
  return false;
}
void Shell::notepad_close() {
  if (document.dirty()) { notepad_pending_action=-3; notepad_prompt=true; notepad_name_init(); end_text(); focus=612; return; }
  notepad_tab_remove();
}
void Shell::notepad_action(int id) {
  if(id==618 && notepad_modal() && !notepad_replace){begin_text(TextTarget::filename);text_selection=true;focus=618;return;}
  if(id==608 || id==612){
    if(!notepad_modal())return;
    notepad_prompt=notepad_save_dialog=notepad_replace=false;notepad_save_target.reset();
    notepad_pending_action=-1;notepad_pending_path[0]=0;window=4;begin_text(TextTarget::document);return;
  }
  if(notepad_replace){
    if(id==619){notepad_replace=false;notepad_save_target.reset();begin_text(TextTarget::filename);focus=618;return;}
    if(id==609){
      if(document.save_as(notepad_save_path,notepad_save_target.get()))notepad_save_finished();
      else{notepad_replace=false;notepad_save_target.reset();notepad_error();}
    }
    return;
  }
  if(id==607 && notepad_save_dialog){notepad_save_submit();return;}
  if (notepad_prompt && id >= 613) return;
  if (id >= 613 && id <= 615) { notepad_menu = notepad_menu == id - 612 ? 0 : id - 612; return; }
  notepad_menu = 0;
  if (id == 616) { if (text_selection) clear_text(); return; }
  if (id == 617) { auto now = std::time(nullptr); char stamp[80]; std::strftime(stamp,sizeof(stamp),"%H:%M %m/%d/%Y",std::localtime(&now));begin_text(TextTarget::document);insert_text(stamp);return; }
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
    if(id==610){notepad_save_submit();return;}
    if(id==611){document.close(true);notepad_save_finished();}
    return;
  }
  if (notepad_prompt)
    return;
  if (!document.active)
    return;
  if (id >= 630 && id < 638) { notepad_tab_switch(id-630); return; }
  if (id == 638) { notepad_tab_new(); return; }
  if (id == 627 || id == 628) {
    if (id == 627 ? document.undo() : document.redo()) { notepad_match=size_t(-1);text_selection=false;text_caret=int(document.caret); }
    begin_text(TextTarget::document);return;
  }
  if (id == 620) { notepad_search_open = true; notepad_menu = 0; notepad_match = size_t(-1); notepad_search_message[0] = 0; begin_text(TextTarget::notepad_find); return; }
  if (id == 621 || id == 622) { begin_text(id == 621 ? TextTarget::notepad_find : TextTarget::notepad_replace_text); return; }
  if (id == 625) { notepad_search_open = false; notepad_match = size_t(-1); begin_text(TextTarget::document); return; }
  if (id == 623) {
    const size_t start = notepad_match == size_t(-1) ? document.caret : notepad_match + 1;
    notepad_match = notepad_search::find_next(std::string_view(document.text, document.length), notepad_query, start);
    if (notepad_match == size_t(-1)) std::snprintf(notepad_search_message, sizeof(notepad_search_message), "No matches found");
    else { document.caret = notepad_match; std::snprintf(notepad_search_message, sizeof(notepad_search_message), "Match found"); }
    return;
  }
  if (id == 626) {
    const size_t q=std::strlen(notepad_query), r=std::strlen(notepad_replacement);
    if (document.read_only) { document.fail(DocumentResult::read_only); notepad_error(); return; }
    if (!q || notepad_match == size_t(-1) || notepad_match > document.length || q > document.length-notepad_match || std::memcmp(document.text+notepad_match,notepad_query,q)) {
      std::snprintf(notepad_search_message,sizeof(notepad_search_message),"Find a match first"); return;
    }
    if (r > TextDocument::Capacity-(document.length-q)) { std::snprintf(notepad_search_message,sizeof(notepad_search_message),"Result too large"); return; }
    document.checkpoint();
    std::memmove(document.text+notepad_match+r,document.text+notepad_match+q,document.length-notepad_match-q+1);
    std::memcpy(document.text+notepad_match,notepad_replacement,r);
    document.length=document.length-q+r;document.caret=notepad_match+r;notepad_match=size_t(-1);
    std::snprintf(notepad_search_message,sizeof(notepad_search_message),"Replaced");return;
  }
  if (id == 624) {
    if (document.read_only) { document.fail(DocumentResult::read_only); notepad_error(); return; }
    std::string result; size_t count = 0;
    if (!notepad_search::replace_all(std::string_view(document.text, document.length), notepad_query, notepad_replacement, TextDocument::Capacity, result, count)) {
      std::snprintf(notepad_search_message, sizeof(notepad_search_message), "Empty search or result too large"); return;
    }
    if (count && (result.size()!=document.length || std::memcmp(result.data(),document.text,document.length)!=0)) document.checkpoint();
    std::memcpy(document.text, result.data(), result.size()); document.length = result.size(); document.text[document.length] = 0;
    document.caret = std::min(document.caret, document.length); notepad_match = size_t(-1);
    std::snprintf(notepad_search_message, sizeof(notepad_search_message), "%zu replacements", count); return;
  }
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
  if (id == 601 && document.path[0]) {
    if (!document.save()) notepad_error();
    return;
  }
  if (id == 601 || id==606) {
    notepad_name_init();notepad_save_as_mode=id==606;notepad_save_dialog=true;
    notepad_pending_action=-1;notepad_pending_path[0]=0;
    begin_text(TextTarget::filename);text_selection=true;focus=618;
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
  if(!notepad_modal())return;
  rect({0,0,W,H},0x55000000);
  const Rect modal{540,touch_keyboard?210:365,840,320};panel(modal);
  text(modal.x+30,modal.y+26,notepad_replace?"Replace existing file?":notepad_prompt?"Save changes to your document?":notepad_save_as_mode?"Save As":"Save document",1,ink());
  text(modal.x+30,modal.y+65,"File name",0,muted());
  const Rect field{modal.x+30,modal.y+92,modal.w-60,44};
  round(field,4,dark?0xff191919:0xffffffff);
  if(!notepad_replace)add(field,618);
  std::string shown=notepad_filename;size_t first=0;
  const size_t caret=std::min(shown.size(),size_t(std::max(0,text_caret)));
  while(first<caret && width(shown.substr(first,caret-first).c_str())>field.w-28)++first;
  const std::string visible=shown.substr(first);
  if(text_target==TextTarget::filename && text_selection)round({field.x+10,field.y+9,std::min(field.w-20,width(visible.c_str())),26},2,accent_color());
  text_fit(field.x+10,field.y+10,field.w-20,visible.c_str(),0,text_target==TextTarget::filename&&text_selection?0xffffffff:ink());
  if(text_target==TextTarget::filename&&!text_selection)rect({field.x+10+width(shown.substr(first,caret-first).c_str()),field.y+9,2,26},accent_color());
  if(notepad_replace){
    text(modal.x+30,modal.y+156,"A file with this name already exists.",0,ink());
    text(modal.x+30,modal.y+185,"Replace it with your current document?",0,muted());
    button({modal.x+462,modal.y+246,146,44},609,"Replace");
    button({modal.x+622,modal.y+246,180,44},619,"Back");
  }else{
    text(modal.x+30,modal.y+156,"Click the file name to edit it before saving.",0,muted());
    text(modal.x+30,modal.y+185,"Your edits stay open until saving succeeds.",0,muted());
    button({modal.x+302,modal.y+246,146,44},notepad_prompt?610:607,"Save");
    if(notepad_prompt)button({modal.x+462,modal.y+246,146,44},611,"Don't save");
    button({modal.x+622,modal.y+246,180,44},notepad_prompt?612:608,"Cancel");
  }
}
void Shell::notepad_ui(Rect wr) {
  const int x = wr.x + 16, y = wr.y + 58;
  rect({wr.x + 1, wr.y + 43, wr.w - 2, 45}, dark ? 0xff272727 : 0xfff3f3f3);
  button({x, wr.y + 46, 72, 36}, 613, "File");
  button({x + 80, wr.y + 46, 72, 36}, 614, "Edit");
  button({x + 162, wr.y + 46, 100, 36}, 615, "View");
  notepad_tab_init();
  const int tab_width = std::min(210, (wr.w-340)/std::max(1,int(notepad_tabs.size())+1));
  for (int i=0;i<int(notepad_tabs.size());++i) {
    const TextDocument &tab = i==notepad_tab ? document : *notepad_tabs[i];
    const char *name=std::strrchr(tab.path,'/'); char title[300];
    std::snprintf(title,sizeof(title),"%s%.255s",tab.dirty()?"* ":"",tab.path[0]?(name?name+1:tab.path):"Untitled");
    Rect r{wr.x+132+i*tab_width,wr.y+4,tab_width-2,38};
    round(r,6,i==notepad_tab?(dark?0xff393939:0xfff7f7f7):(dark?0xff272727:0xffe8e8e8));
    add(r,630+i);text_fit(r.x+8,r.y+10,r.w-(i==notepad_tab?40:16),title,0,ink());
    if (i==notepad_tab) button({r.x+r.w-30,r.y+3,27,30},602,"x");
  }
  button({wr.x+132+int(notepad_tabs.size())*tab_width,wr.y+6,36,32},638,"+");
  Rect paper{wr.x+1,wr.y+91,wr.w-2,wr.h-139};
  if (touch_keyboard && paper.y + paper.h > 610)
    paper.h = 610 - paper.y;
  if (paper.h < 80)
    paper.h = 80;
  notepad_body = paper;
  rect(paper, dark ? 0xff1f1f1f : 0xffffffff);
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
    else if (notepad_search_open && notepad_match != size_t(-1)) {
      const size_t visible_start=at+first, visible_end=visible_start+count;
      const size_t match_end=notepad_match+std::strlen(notepad_query);
      const size_t lo=std::max(visible_start,notepad_match), hi=std::min(visible_end,match_end);
      if (hi>lo) {
        const size_t left=lo-visible_start, right=hi-visible_start;
        const char saved_left=line[left], saved_right=line[right];
        line[left]=0; const int x_offset=width(line); line[left]=saved_left;
        line[right]=0; const int extent=width(line); line[right]=saved_right;
        round({paper.x+12+x_offset,top,extent-x_offset,25},2,dark?0xff594d28:0xff88c9f9);
      }
    }
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
                "Ln %d, Col %d    |    %s    |    %s%s", caret_line + 1,
                caret_column + 1,
                document.newline == TextDocument::Newline::crlf ? "Windows (CRLF)" :
                document.newline == TextDocument::Newline::cr ? "Macintosh (CR)" : "Unix (LF)",
                document.bom ? "UTF-8 BOM" : "ASCII",
                document.read_only ? "    |    Read-only" : "");
  text_fit(x + 4, paper.y + paper.h + 15, wr.w - 280, status, 0, muted());
  if (notepad_menu) {
    Rect menu{x + (notepad_menu - 1) * 80, wr.y + 85, 250, notepad_menu == 3 ? 90 : notepad_menu==1?180:264};
    panel(menu);
    if (notepad_menu == 1) {
      button({menu.x+5,menu.y+5,240,40},638,"New tab     Ctrl+T");
      button({menu.x+5,menu.y+47,240,40},601,"Save       Ctrl+S");
      button({menu.x+5,menu.y+89,240,40},606,"Save As   Ctrl+Shift+S");
      button({menu.x+5,menu.y+131,240,40},602,"Close      Ctrl+W");
    } else if (notepad_menu == 2) {
      button({menu.x+5,menu.y+5,240,40},627,"Undo       Ctrl+Z");
      button({menu.x+5,menu.y+47,240,40},628,"Redo       Ctrl+Y");
      button({menu.x+5,menu.y+89,240,40},605,"Select all   Ctrl+A");
      button({menu.x+5,menu.y+131,240,40},616,"Delete selection");
      button({menu.x+5,menu.y+173,240,40},617,"Insert date / time");
      button({menu.x+5,menu.y+215,240,40},620,"Find / replace  Ctrl+F");
    } else {
      button({menu.x+5,menu.y+5,240,40},603,"Page up");
      button({menu.x+5,menu.y+47,240,40},604,"Page down");
    }
  }
  if (notepad_search_open) {
    const Rect box{wr.x+wr.w-690, wr.y+91, 670, 174}; panel(box);
    const Rect query{box.x+12,box.y+12,290,42}, replacement{box.x+12,box.y+61,290,42};
    round(query,4,dark?0xff191919:0xffffffff); round(replacement,4,dark?0xff191919:0xffffffff);
    add(query,621); add(replacement,622);
    text_fit(query.x+8,query.y+10,query.w-16,notepad_query[0]?notepad_query:"Find",0,notepad_query[0]?ink():muted());
    text_fit(replacement.x+8,replacement.y+10,replacement.w-16,notepad_replacement[0]?notepad_replacement:"Replace with",0,notepad_replacement[0]?ink():muted());
    button({box.x+314,box.y+12,140,42},623,"Find next");
    button({box.x+314,box.y+61,140,42},626,"Replace");
    button({box.x+464,box.y+61,190,42},624,"Replace all");
    button({box.x+544,box.y+12,110,42},625,"Close");
    text_fit(box.x+12,box.y+120,box.w-24,notepad_search_message,0,muted());
  }
}
} // namespace ww
// Spreadsheet UI and New-document integration use the existing shell renderer.
namespace ww {
bool Shell::sheet_commit() {
  if (!sheet.editing)
    return true;
  if (!sheet.set(sheet.row, sheet.column, sheet.edit))
    return false;
  sheet.editing = false;
  if (text_target == TextTarget::sheet)
    end_text();
  return true;
}
bool Shell::sheet_open(const char *path) {
  if (!sheet_commit() || !sheet.load(path)) {
    dialog = true;
    focus = 60;
    std::snprintf(message, sizeof(message), "%s", sheet.status);
    return false;
  }
  window = 6;
  start = context = calendar = quick = tray = false;
  end_text();
  return true;
}
bool Shell::sheet_guard(int id) {
  if (sheet_prompt)
    return id >= 1530 && id <= 1532;
  if (window != 6 && id != 1423 && id != 1424)
    return true;
  const bool leaving = (id >= 10 && id <= 13) || (id >= 30 && id <= 39) ||
                       ((id >= 400 && id <= 409)||(id>=420&&id<=436)) || (id >= 500 && id < 580) ||
                       (id >= 100 && id <= 108) || id == 40 || id == 41 ||
                       id == 91 || id == 93 || id == 800 ||
                       (id >= 1420 && id <= 1431);
  if (!leaving)
    return true;
  if (!sheet_commit()) {
    dialog = true;
    focus = 60;
    std::snprintf(message, sizeof(message), "%s", sheet.status);
    return false;
  }
  if ((id == 93 || id == 1423 || id == 1424) && sheet.dirty()) {
    sheet_prompt = true;
    window = 6;
    sheet_pending = id;
    focus = 1532;
    return false;
  }
  return true;
}
void Shell::new_document(int kind) {
  // Never reuse a name: exclusive creation protects all existing files.
  char folder[1024];
  if (window == 1 && !file_games && file_path[0])
    std::snprintf(folder, sizeof(folder), "%s", file_path);
  else {
    std::snprintf(folder, sizeof(folder), "%s", preferences);
    char *slash = std::strrchr(folder, '/'), *back = std::strrchr(folder, '\\');
    if (back && (!slash || back > slash))
      slash = back;
    if (slash) {
      if (slash == folder)
        slash[1] = 0;
      else
        *slash = 0;
    } else
      std::snprintf(folder, sizeof(folder), ".");
  }
  const bool desktop_location = !(window == 1 && !file_games && file_path[0]);
  auto make_directory = [](const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return ww::privilege::run([&] { return ::mkdir(path, 0700); });
#endif
  };
  if (desktop_location) {
    desktop_folder(folder, sizeof(folder));
    if(!folder[0]){dialog=true;focus=60;std::snprintf(message,sizeof(message),"The Desktop path is too long.");return;}
    if (make_directory(folder) < 0 && errno != EEXIST) {
      dialog = true;
      focus = 60;
      std::snprintf(message, sizeof(message),
                    "The Desktop folder could not be created.");
      return;
    }
  }
  context = start = false;
  if (kind == 1420) {
    char path[1024];
    bool created = false;
    for (int i = 1; i <= 999; ++i) {
      int n = i == 1
                  ? std::snprintf(path, sizeof(path), "%s/New folder", folder)
                  : std::snprintf(path, sizeof(path), "%s/New folder (%d)",
                                  folder, i);
      if (n < 0 || n >= int(sizeof(path)))
        break;
      if (make_directory(path) == 0) {
        created = true;
        break;
      }
      if (errno != EEXIST)
        break;
    }
    if (!created) {
      dialog = true;
      focus = 60;
      std::snprintf(message, sizeof(message),
                    "The new folder could not be created in this location.");
      return;
    }
    desktop_files_refresh();
    if (window == 1)
      file_refresh();
    return;
  }
  if (kind == 1422) {
    paint.show(*this);
    paint.blank();
    paint.begin_save(*this, folder);
    return;
  }
  std::string initial;
  if (kind == 1423) {
    auto book = xlsx::Workbook::blank();
    std::vector<xlsx::Part> parts;
    std::string error;
    if (!book.encode(initial, parts, error)) {
      dialog = true;
      focus = 60;
      std::snprintf(message, sizeof(message), "%s", error.c_str());
      return;
    }
  }
  const auto &io = document_io();
  char path[1024];
  int fd = -1;
  for (int n = 1; n <= 999; ++n) {
    const int count = std::snprintf(
        path, sizeof(path), "%s/%s (%d).%s", folder,
        (kind == 1423 || kind == 1424) ? "New worksheet" : "New text document",
        n,
        kind == 1423   ? "xlsx"
        : kind == 1424 ? "csv"
                       : "txt");
    if (count < 0 || count >= int(sizeof(path)))
      break;
    fd = io.create(io.context, path, 0600);
    if (fd >= 0)
      break;
    if (fd != -EEXIST)
      break; // EEXIST on both target platforms.
  }
  if (fd < 0) {
    dialog = true;
    focus = 60;
    std::snprintf(message, sizeof(message),
                  "Could not create a document in this folder. Choose a "
                  "writable folder in File Explorer.");
    return;
  }
  bool written = true;
  size_t offset = 0;
  while (offset < initial.size()) {
    auto count = io.write(io.context, fd, initial.data() + offset,
                          initial.size() - offset);
    if (count <= 0 || uint64_t(count) > initial.size() - offset) {
      written = false;
      break;
    }
    offset += size_t(count);
  }
  const int synced = io.sync(io.context, fd), closed = io.close(io.context, fd);
  if (!written || synced < 0 || closed < 0) {
    dialog = true;
    focus = 60;
    std::snprintf(message, sizeof(message),
                  "The new file could not be flushed safely. Reopen it from "
                  "File Explorer.");
    return;
  }
  desktop_files_refresh();
  if (kind == 1423 || kind == 1424) {
    sheet.storage.close(true);
    sheet_open(path);
  } else
    notepad_open(path);
}
void Shell::sheet_action(int id) {
  if (id >= 1530 && id <= 1532) {
    if (!sheet_prompt)
      return;
    if (id == 1532) {
      sheet_prompt = false;
      sheet_pending = -1;
      return;
    }
    if (id == 1530 && !sheet.save())
      return;
    int next = sheet_pending;
    sheet_prompt = false;
    sheet_pending = -1;
    if (id == 1531)
      sheet.storage.close(true);
    activate(next);
    return;
  }
  if (sheet_prompt)
    return;
  if (id == 1540) {
    if (sheet_commit())
      sheet.add_sheet();
    return;
  }
  if (id >= 1550 && id < 1566) {
    if (sheet_commit())
      sheet.select_sheet(id - 1550);
    return;
  }
  if (id == 1541 || id == 1542) {
    if (sheet_commit())
      sheet.select_sheet(
          std::max(0, std::min(int(sheet.workbook.pages.size()) - 1,
                               sheet.selected_sheet + (id == 1541 ? -1 : 1))));
    return;
  }
  if (id == 1506) {
    if (!sheet_commit())
      return;
    std::string stem = sheet.storage.path;
    auto dot = stem.rfind('.');
    if (dot != stem.npos)
      stem.resize(dot);
    for (int i = 1; i <= 999; ++i) {
      std::string path = stem + " - Copy " + std::to_string(i) + ".xlsx";
      if (sheet.export_xlsx(path.c_str()))
        break;
      if (sheet.storage.error != EEXIST)
        break;
    }
    return;
  }
  if (id == 1505) {
    if (sheet_commit()) {
      window = 1;
      file_location_open(402);
    }
    return;
  }
  if (id >= 1520 && id <= 1524) {
    if (!sheet_commit())
      return;
    const char *functions[] = {"SUM", "AVERAGE", "MIN", "MAX", "COUNT"};
    char formula[80];
    const int first = sheet.row ? 1 : 2, last = sheet.row ? sheet.row : 100;
    std::snprintf(formula, sizeof(formula), "=%s(%c%d:%c%d)",
                  functions[id - 1520], char('A' + sheet.column), first,
                  char('A' + sheet.column), last);
    sheet.set(sheet.row, sheet.column, formula);
    return;
  }
  if (id >= 1600 && id < 1900) {
    if (!sheet_commit())
      return;
    int index = id - 1600;
    sheet.row = sheet.first_row + index / 10;
    sheet.column = sheet.first_column + index % 10;
    if (sheet.row >= 100 || sheet.column >= 26)
      return;
    std::snprintf(sheet.edit, sizeof(sheet.edit), "%s",
                  sheet.cells[sheet.row * 26 + sheet.column].c_str());
    if (!sheet.editable()) {
      std::snprintf(sheet.status, sizeof(sheet.status),
                    "This cell is preserved but cannot be edited here "
                    "(read-only, long text or shared/array formula).");
      return;
    }
    sheet.editing = true;
    begin_text(TextTarget::sheet);
    text_selection = true;
    return;
  }
  if (id == 1500) {
    if (sheet_commit())
      new_document(1423);
  } else if (id == 1501) {
    if (sheet_commit())
      sheet.save();
  } else if (id == 1502) {
    if (!sheet.editing) {
      std::snprintf(sheet.edit, sizeof(sheet.edit), "%s",
                    sheet.cells[sheet.row * 26 + sheet.column].c_str());
      sheet.editing = true;
    }
    begin_text(TextTarget::sheet);
  } else if (id == 1503) {
    if (sheet_commit()) {
      sheet.row = std::min(99, sheet.row + 1);
    }
  } else if (id == 1504) {
    sheet.editing = false;
    end_text();
  } else if (id >= 1510 && id <= 1513) {
    if (!sheet_commit())
      return;
    if (id == 1510)
      sheet.first_row = std::max(0, sheet.first_row - 10);
    if (id == 1511)
      sheet.first_row = std::min(90, sheet.first_row + 10);
    if (id == 1512)
      sheet.first_column = std::max(0, sheet.first_column - 5);
    if (id == 1513)
      sheet.first_column = std::min(16, sheet.first_column + 5);
  }
}
void Shell::sheet_ui(Rect wr) {
  const auto green = dark ? 0xff71cf9c : 0xff487421;
  const auto paper = dark ? 0xff202020 : 0xffffffff;
  const auto grid = dark ? 0xff494949 : 0xffdfdfdf;
  const int x = wr.x, y = wr.y;
  rect({x + 1, y + 43, wr.w - 2, wr.h - 44}, dark ? 0xff292929 : 0xfff3f3f3);
  rect({x + 1, y + 43, wr.w - 2, 42}, dark ? 0xff214629 : 0xffe3f0e8);
  button({x + 10, y + 47, 65, 34}, 1505, "File");
  text(x + 102, y + 54, "Home", 0, green);
  rect({x + 100, y + 80, 65, 3}, green);
  text(x + 202, y + 54, "Formulas", 0, muted());
  text(x + 340, y + 54, sheet.xlsx_format ? "Excel workbook" : "CSV worksheet",
       0, muted());
  button({x + wr.w - 200, y + 47, 182, 34}, 1506, "Save XLSX copy");
  round({x + 12, y + 94, wr.w - 24, 78}, 7, paper);
  button({x + 24, y + 109, 135, 43}, 1423, "New sheet");
  button({x + 168, y + 109, 100, 43}, 1501, "Save");
  const char *functions[] = {"SUM", "AVERAGE", "MIN", "MAX", "COUNT"};
  const int offsets[] = {296, 390, 530, 620, 710},
            widths[] = {90, 136, 86, 86, 110};
  for (int i = 0; i < 5; ++i)
    button({x + offsets[i], y + 99, widths[i], 36}, 1520 + i, functions[i]);
  text(x + 296, y + 139, "Use =A1+B1 or =SUM(A1:A10)", 0, muted());
  char cell[20];
  std::snprintf(cell, sizeof(cell), "%c%d", 'A' + sheet.column, sheet.row + 1);
  round({x + 12, y + 183, 95, 40}, 3, paper);
  center(x + 12, y + 192, 95, cell, 0, ink());
  text(x + 122, y + 192, "fx", 0, green);
  round({x + 161, y + 183, wr.w - 375, 40}, 3, paper);
  add({x + 161, y + 183, wr.w - 375, 40}, 1502);
  if (sheet.editing)
    draw_text_field(x + 173, y + 192, wr.w - 405, TextTarget::sheet, "");
  else
    text_fit(x + 173, y + 192, wr.w - 405,
             sheet.cells[sheet.row * 26 + sheet.column].c_str(), 0, ink());
  button({x + wr.w - 204, y + 183, 86, 40}, 1503, "Enter");
  button({x + wr.w - 112, y + 183, 98, 40}, 1504, "Cancel");
  const int gx = x + 58, gy = y + 267, cw = (wr.w - 76) / 10, rh = 30;
  int visible = std::min(23, (wr.h - 350) / rh);
  if (touch_keyboard)
    visible = std::min(visible, std::max(1, (600 - gy) / rh));
  rect({x + 12, y + 235, wr.w - 24, visible * rh + 32}, paper);
  for (int c = 0; c < 10 && sheet.first_column + c < 26; ++c) {
    char label[2] = {char('A' + sheet.first_column + c), 0};
    center(gx + c * cw, y + 242, cw, label, 0, ink());
  }
  for (int r = 0; r < visible && sheet.first_row + r < 100; ++r) {
    char label[16];
    std::snprintf(label, sizeof(label), "%d", sheet.first_row + r + 1);
    center(x + 12, gy + r * rh + 5, 44, label, 0, muted());
    for (int c = 0; c < 10 && sheet.first_column + c < 26; ++c) {
      Rect cellrect{gx + c * cw, gy + r * rh, cw, rh};
      int rr = sheet.first_row + r, cc = sheet.first_column + c;
      line(cellrect.x, cellrect.y, cellrect.x + cw, cellrect.y, grid);
      line(cellrect.x, cellrect.y, cellrect.x, cellrect.y + rh, grid);
      const auto value = sheet.display(rr, cc);
      text_fit(cellrect.x + 6, cellrect.y + 5, cw - 12, value.c_str(), 0,
               value[0] == '#' ? 0xff4444c4 : ink());
      add(cellrect, 1600 + r * 10 + c);
    }
  }
  const int sc = sheet.column - sheet.first_column,
            sr = sheet.row - sheet.first_row;
  if (sc >= 0 && sc < 10 && sr >= 0 && sr < visible) {
    Rect r{gx + sc * cw, gy + sr * rh, cw, rh};
    rect({r.x, r.y, r.w, 2}, green);
    rect({r.x, r.y, 2, r.h}, green);
    rect({r.x + r.w - 2, r.y, 2, r.h}, green);
    rect({r.x, r.y + r.h - 2, r.w, 2}, green);
    rect({r.x + r.w - 4, r.y + r.h - 4, 6, 6}, green);
  }
  if (sheet.xlsx_format) {
    const int first = (sheet.selected_sheet / 4) * 4;
    for (int i = first;
         i < std::min(first + 4, int(sheet.workbook.pages.size())); ++i) {
      Rect tab{x + 16 + (i - first) * 130, y + wr.h - 80, 124, 36};
      button(tab, 1550 + i, "");
      text_fit(tab.x + 8, tab.y + 9, 108, sheet.workbook.pages[i].name.c_str(),
               0, i == sheet.selected_sheet ? green : muted());
      if (i == sheet.selected_sheet)
        rect({tab.x, tab.y + 35, tab.w, 2}, green);
    }
    button({x + 542, y + wr.h - 80, 40, 36}, 1541, "<");
    button({x + 586, y + wr.h - 80, 40, 36}, 1542, ">");
    button({x + 620, y + wr.h - 80, 28, 36}, 1540, "+");
  } else
    text(x + 24, y + wr.h - 70, "Sheet1", 0, green);
  button({x + 650, y + wr.h - 80, 98, 36}, 1510, "Page up");
  button({x + 752, y + wr.h - 80, 112, 36}, 1511, "Page down");
  button({x + 880, y + wr.h - 80, 70, 36}, 1512, "<");
  button({x + 956, y + wr.h - 80, 70, 36}, 1513, ">");
  text_fit(x + 24, y + wr.h - 31, wr.w - 360, sheet.status, 0, muted());
  text(x + wr.w - 300, y + wr.h - 31,
       sheet.xlsx_format
           ? (sheet.dirty() ? "Unsaved changes | XLSX" : "Saved | XLSX | 100%")
           : (sheet.dirty() ? "Unsaved changes | CSV" : "Saved | CSV | 100%"),
       0, muted());
  if (sheet_prompt) {
    rect(wr, 0x66000000);
    Rect box{x + (wr.w - 650) / 2, y + (wr.h - 250) / 2, 650, 250};
    panel(box);
    text(box.x + 28, box.y + 28, "Save changes to your worksheet?", 1, ink());
    text_fit(box.x + 28, box.y + 85, box.w - 56, sheet.status, 0, muted());
    button({box.x + 25, box.y + 175, 170, 44}, 1530, "Save");
    button({box.x + 210, box.y + 175, 190, 44}, 1531, "Don't save");
    button({box.x + 415, box.y + 175, 210, 44}, 1532, "Cancel");
  }
}
} // namespace ww
