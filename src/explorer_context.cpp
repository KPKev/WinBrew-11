// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
namespace ww {
namespace {
bool joined(char *out, size_t capacity, const char *root, const char *name) {
  const size_t n = std::strlen(root);
  const int result = std::snprintf(out, capacity, "%s%s%s", root,
                                   n && root[n - 1] == '/' ? "" : "/", name);
  return result > 0 && size_t(result) < capacity;
}
void report(Shell &shell, const char *value) {
  shell.dialog = true;
  shell.focus = 60;
  std::snprintf(shell.message, sizeof(shell.message), "%s", value);
}
const char *filename(const char *path) {
  const char *slash = std::strrchr(path, '/');
  return slash ? slash + 1 : path;
}
} // namespace
bool Shell::file_context_open() {
  if (dialog || notepad_modal() || paint.modal() ||
      files_busy() || file_rename_prompt || file_properties)
    return false;
  shortcut_context_id = 0; shortcut_context_app = 0;file_context_desktop=false;file_menu_kind=0;
  int target = -1; Rect target_rect{};
  for(int i=count-1;i>=0;--i) {
    if((input_device==InputDevice::controller && hits[i].id==focus) ||
       (input_device!=InputDevice::controller && hits[i].r.has(mx,my))) {
      target=hits[i].id;target_rect=hits[i].r;break;
    }
  }
  const bool taskbar = target_rect.y >= H-54;
  const bool desktop_item = target>=2000 && target<2064;
  const bool builtin = target==10||target==11||target==13||target==30||target==32||target==33||target==34||target==35||target==36||target==37||target==38;
  const bool steam_shortcut = target>=1200 && target<1328;
  if((taskbar && (builtin || steam_shortcut)) || (!taskbar && (desktop_item || builtin || steam_shortcut) && target_rect.w<=130)) {
    if(!desktop_item) {
      shortcut_context_id=target;shortcut_context_taskbar=taskbar;
      if(steam_shortcut)shortcut_context_app=rendered_shortcut_apps[target-1200];
      file_context_item=false;
    } else {
      std::snprintf(file_context_path,sizeof(file_context_path),"%s",desktop_rendered_paths[target-2000]);
      std::snprintf(file_context_folder,sizeof(file_context_folder),"%s",desktop_files[target-2000].directory?file_context_path:desktop_folder_path);
      file_context_desktop=true;file_context_item=true;file_context_info={};fileops::inspect(file_context_path,file_context_info);focus=target;
    }
    const int height=desktop_item?200:taskbar?40:72;
    file_context_bounds={std::max(0,std::min(mx,W-240)),std::max(0,std::min(my,H-54-height)),240,height};
    file_context=true;file_new=false;context=start=calendar=quick=tray=false;end_text();
    focus=shortcut_context_id==11?815:shortcut_context_id?811:800;return true;
  }
  if(window==1&&!file_games){
    std::string path;int location=0;bool navigation=false;
    int navindex=target>=2400&&target<2656?target-2400:target>=2700&&target<2956?target-2700:-1;
    if(navindex>=0&&navindex<int(file_nav.size())){path=file_nav[navindex].path;location=file_nav[navindex].location;navigation=!file_nav[navindex].group&&location!=401;}
    else if(target>=3000&&target<3016&&target-3000<int(file_drives.size())){path=file_drives[target-3000].path;location=file_drives[target-3000].location;navigation=true;}
    else if(target>=430&&target<=435){static const char*names[]={"Desktop","Downloads","Documents","Pictures","Music","Videos"};path=file_user_root()+"/"+names[target-430];location=target;navigation=true;}
    if(navigation){file_context_nav_location=location;std::snprintf(file_context_path,sizeof(file_context_path),"%s",path.c_str());file_context_bounds={std::max(0,std::min(mx,W-240)),std::max(0,std::min(my,H-54-104)),240,104};file_context=true;file_menu_kind=5;file_new=false;context=false;end_text();focus=2150;return true;}
  }
  if(window!=1)return false;
  if (file_games || !file_path[0])
    return file_window_bounds.has(mx, my);
  int selected = -1;
  Rect anchor{};
  if (input_device == InputDevice::controller && focus >= 500 && focus < 580) {
    selected = file_hit_index(focus);
    for (int i = 0; i < count; ++i)
      if (hits[i].id == focus)
        anchor = hits[i].r;
  } else {
    if (!file_window_bounds.has(mx, my))
      return false;
    if (!file_content_bounds.has(mx, my) || file_games || !file_path[0])
      return true;
    for (int i = count - 1; i >= 0; --i)
      if (hits[i].id >= 500 && hits[i].id < 580 && hits[i].r.has(mx, my)) {
        selected = file_hit_index(hits[i].id);
        break;
      }
  }
  if (selected >= file_count)
    selected = -1;
  if(selected>=0){if(std::find(file_selection.begin(),file_selection.end(),files[selected].name)==file_selection.end())file_select_row(selected);}else {file_selection.clear();file_selected=-1;}
  file_selected = selected;
  file_context_item = selected >= 0;
  std::snprintf(file_context_folder, sizeof(file_context_folder), "%s",
                file_path);
  std::snprintf(file_context_path, sizeof(file_context_path), "%s", file_path);
  if (file_context_item) {
    if (!joined(file_context_path, sizeof(file_context_path), file_path,
                files[selected].name)) {
      report(*this, "This path is too long to open.");
      return true;
    }
    if (files[selected].directory)
      std::snprintf(file_context_folder, sizeof(file_context_folder), "%s",
                    file_context_path);
  }
  file_context_info={};if(file_context_item)fileops::inspect(file_context_path,file_context_info);
  const int h = file_context_item ? (files[selected].directory?296:264) : 264;
  int x = mx, y = my;
  if (anchor.w) {
    x = anchor.x + 60;
    y = anchor.y + anchor.h / 2;
  }
  x = x < 0 ? 0 : x > W - 240 ? W - 240 : x;
  y = y < 0 ? 0 : y > H - 54 - h ? H - 54 - h : y;
  file_context_bounds = {x, y, 240, h};
  file_context = true;
  file_new = false;
  context = start = calendar = quick = tray = false;
  end_text();
  focus = file_context_item ? 800 : file_clipboard.ready ? 803 : 805;
  return true;
}
void Shell::empty_recycle_request(const char *folder) {
  if(file_transfer.busy() || file_purge.busy())return;
  char store[1024];
  file_context=context=false;
  if(!fileops::recovery_location(folder,store,sizeof(store)) || !file_purge.prepare(store)) {
    report(*this,"This Recycle Bin could not be checked. Nothing was deleted. Links, inaccessible items or an oversized bin need attention.");return;
  }
  if(!file_purge.ready){report(*this,"This Recycle Bin is already empty.");return;}
  dialog=recycle_confirmation=true;focus=60;
  std::snprintf(message,sizeof(message),"Permanently delete %zu files and folders (%llu bytes) from this Recycle Bin? This cannot be undone.",file_purge.remaining,static_cast<unsigned long long>(file_purge.bytes));
}
void Shell::file_action(int id) {
  if(id==808&&file_context){file_new=!file_new;int x=file_context_bounds.x+file_context_bounds.w+4;if(x+240>W)x=std::max(0,file_context_bounds.x-244);file_new_bounds={x,std::min(H-54-168,file_context_bounds.y+64),240,168};focus=1420;return;}
  if (id == 819)return;
  if(id==816){file_begin_rename();return;}
  if(id==817){file_browser_action(2147);return;}
  if(id==818){file_browser_action(2146);return;}
  if (id == 814) {
    if(file_context && shortcut_context_id==11){char desktop[1024];desktop_folder(desktop,sizeof(desktop));empty_recycle_request(desktop);}
    else if(window==1&&!file_games)empty_recycle_request(file_path);
    return;
  }
  if(id==815 && file_context && shortcut_context_id==11){file_context=false;activate(11);return;}
  if (id == 807) {
    file_batch_active=false;file_batch_queue.clear();
    if(file_recycle_active){file_recycle_active=false;file_recycle_queue.clear();file_refresh();desktop_files_next_scan=0;report(*this,"Recycling stopped. Items already moved remain recoverable in their Recycle Bin.");return;}
    if(file_purge.busy()){
      file_purge.cancel();
      report(*this,file_purge.removed?"Stopped emptying. Items already deleted cannot be recovered. Remaining items were kept.":"Emptying canceled. Your items were kept.");
      return;
    }
    file_transfer.cancel();
    return;
  }
  if (id == 806) {
    file_properties = false;
    focus = -1;
    return;
  }
  if (files_busy() || file_rename_prompt || file_properties)
    return;
  if (!file_context && !(id == 800 && file_context_path[0]))
    return;
  file_context = false;
  if(id==811 || id==812){
    if(!shortcut_context_id)return;
    const auto placement=id==811?VaporShortcuts::Taskbar:VaporShortcuts::Desktop;
    if(shortcut_context_app){
      if(!vapor_shortcuts.toggle(shortcut_context_app,placement))report(*this,"The shortcut change could not be saved.");
    }else{
      int target=shortcut_context_id==13?30:shortcut_context_id==10?32:shortcut_context_id;
      if(id==811){taskbar_hidden^=std::uint64_t(1)<<target;}else desktop_hidden|=std::uint64_t(1)<<shortcut_context_id;
      save_preferences();
    }
    return;
  }
  if(id==809){
    if(!file_context_desktop && file_selection.size()>1){file_recycle_selection();return;}
    fileops::Info info{};char recovery[1024]{};
    if(fileops::inspect(file_context_path,info)<0 || !fileops::unchanged(info,file_context_info) || !fileops::trash(file_context_path,file_context_info,recovery,sizeof(recovery))) {
      report(*this,"The item could not be moved to recovery storage. It may be in use or read-only.");return;
    }
    if(file_clipboard.ready && !std::strcmp(file_clipboard.path,file_context_path))file_clipboard.ready=false;
    if(window==1 && !file_games)file_refresh();
    desktop_files_next_scan=0;
    report(*this,"Deleted item kept in .winbrew-trash in its original folder. Recycle Bin opens Desktop recovery storage.");return;
  }
  if (id == 805) {
    file_refresh();
    return;
  }
  if (id == 800) {
    fileops::Info info{};
    if (fileops::inspect(file_context_path, info) < 0) {
      report(*this, "This file or folder could not be opened. Links and "
                    "special files are unsupported.");
      return;
    }
    if (info.directory) {
      window=1;file_games=false;file_open(file_context_path);
    }
    else if (!info.regular)
      report(*this, "Only regular files and folders can be opened here.");
    else if (SheetDocument::supports(file_context_path))
      sheet_open(file_context_path);
    else if (Paint::supports(file_context_path))
      paint.open(*this, file_context_path);
    else
      notepad_open(file_context_path);
    return;
  }
  if (id == 801 || id == 802) {
    if(!file_context_desktop&&file_selection.size()>1){file_copy_selection(id==802);return;}
    file_batch_clipboard.clear();
    if (!file_clipboard.copy(file_context_path))
      report(*this, "This item cannot be copied. Links and special files are unsupported.");
    else file_clipboard.move = id == 802;
    return;
  }
  if (id == 803) {
    file_transfer_from_clipboard=true;
    if(file_batch_clipboard.size()>1){file_batch_queue=file_batch_clipboard;file_batch_index=0;file_batch_active=true;file_batch_destination=file_context_folder;}
    file_transfer.begin(file_clipboard, file_context_folder);
    file_transfer_seen = fileops::State::idle;
    file_operations_tick();
    return;
  }
  if (id == 804) {
    file_properties_multiple=!file_context_desktop&&file_selection.size()>1;
    if(file_properties_multiple){file_properties_info={};for(auto&entry:files)if(std::find(file_selection.begin(),file_selection.end(),entry.name)!=file_selection.end()){fileops::Info info{};char path[1300];std::snprintf(path,sizeof(path),"%s/%s",file_path,entry.name);if(fileops::inspect(path,info)==0&&info.regular)file_properties_info.size+=info.size;}std::snprintf(file_properties_path,sizeof(file_properties_path),"%zu selected items",file_selection.size());file_properties=true;focus=806;return;}
    if (fileops::inspect(file_context_path, file_properties_info) < 0) {
      report(*this, "Properties are unavailable. The location may be "
                    "restricted, disconnected or a link.");
      return;
    }
    std::snprintf(file_properties_path, sizeof(file_properties_path), "%s",
                  file_context_path);
    file_properties = true;
    focus = 806;
  }
}
void Shell::file_operations_tick() {
  if(file_recycle_active){
    if(file_recycle_index<file_recycle_queue.size()){
      const auto&item=file_recycle_queue[file_recycle_index];char recovery[1024];
      if(!fileops::trash(item.path,item.info,recovery,sizeof(recovery))){file_recycle_active=false;file_refresh();desktop_files_next_scan=0;report(*this,"Some items could not be recycled. Items already moved remain recoverable; remaining files were kept.");return;}
      ++file_recycle_index;
    }
    if(file_recycle_index>=file_recycle_queue.size()){file_recycle_active=false;file_recycle_queue.clear();file_refresh();desktop_files_next_scan=0;report(*this,"The selected items were moved to their Recycle Bin.");}
    return;
  }
  if(file_purge.busy()){
    file_purge.step();
    if(!file_purge.busy()){
      desktop_files_next_scan=0;
      if(file_clipboard.ready && std::string(file_clipboard.path).find(file_purge.root+"/")==0)file_clipboard.ready=false;
      if(window==1 && !file_games && std::string(file_path).find(file_purge.root)==0)file_open(file_purge.root.c_str());
      report(*this,file_purge.state==fileops::State::complete?"The Recycle Bin is empty.":"Emptying stopped because an item changed or could not be removed. Some items may already be permanently deleted. Remaining items were kept.");
    }
    return;
  }
  if (file_transfer.busy())
    file_transfer.step();
  if(file_batch_active && !file_transfer.busy()){
    if(file_transfer.state==fileops::State::complete && file_batch_index+1<file_batch_queue.size()){
      ++file_batch_index;file_transfer.begin(file_batch_queue[file_batch_index],file_batch_destination.c_str());file_transfer_seen=fileops::State::idle;return;
    }
    if(file_transfer.state==fileops::State::failed || file_transfer.state==fileops::State::cancelled || file_batch_index+1>=file_batch_queue.size()){
      file_batch_active=false;file_batch_queue.clear();if(file_transfer_from_clipboard&&file_clipboard.move&&file_transfer.state==fileops::State::complete){file_clipboard.ready=false;file_batch_clipboard.clear();}
    }
  }
  if (file_transfer.state == file_transfer_seen)
    return;
  file_transfer_seen = file_transfer.state;
  if (file_transfer.busy() || file_transfer.state == fileops::State::idle)
    return;
  if (window == 1 && !file_games)file_refresh();
  desktop_files_next_scan=0;
  if (file_transfer.state == fileops::State::complete) {
    if(file_transfer.moving&&file_transfer_from_clipboard)file_clipboard.ready=false;
    for (int i = 0; i < file_count; ++i)
      if (!std::strcmp(files[i].name, filename(file_transfer.destination)))
        file_selected = i;
    return;
  }
  if (file_transfer.state == fileops::State::cancelled)
    report(*this, "Transfer cancelled. The incomplete destination was kept. Source retained.");
  else if (file_transfer.destination_created)
    report(*this, "Transfer stopped. The incomplete destination was kept. Source retained.");
  else {
    dialog = true;
    focus = 60;
    std::snprintf(
        message, sizeof(message),
        "Copy could not start (%s, %d). Existing files are unchanged.",
        file_transfer.stage, file_transfer.error);
  }
}
void Shell::file_actions_ui() {
  if(file_context&&file_menu_kind){
    const auto r=file_context_bounds;panel(r);add(r,819);
    auto item=[&](int row,int id,const char*label,bool enabled=true){Rect hit{r.x+4,r.y+4+row*32,r.w-8,31};if(enabled)button(hit,id,label);else center(hit.x,hit.y+3,hit.w,label,0,muted());};
    if(file_menu_kind==1){const char*names[]={"Name","Date modified","Type","Size","Ascending","Descending"};for(int i=0;i<6;++i)item(i,2110+i,names[i]);}
    else if(file_menu_kind==2){const char*names[]={"Details","List","Tiles"};for(int i=0;i<3;++i)item(i,2120+i,names[i]);}
    else if(file_menu_kind==3){const char*names[]={"Folder","Text document","PNG image","Excel Workbook (.xlsx)","CSV spreadsheet"};for(int i=0;i<5;++i)item(i,1420+i,names[i]);}
    else if(file_menu_kind==4){item(0,2130,"Select all");item(1,2131,file_select_mode?"Hide selection checkboxes":"Selection checkboxes");item(2,2132,file_show_hidden?"Hide hidden items":"Show hidden items");item(3,2133,"Copy folder path");item(4,2134,"Properties");}
    else if(file_menu_kind==5){item(0,2150,"Open");item(1,2151,"Open in new tab");item(2,2152,"Properties",file_context_path[0]);}
  }else if (file_context) {
    const auto r = file_context_bounds;
    panel(r);
    add(r, 819);
    auto item = [&](int row, int id, const char *label, bool enabled) {
      const Rect hit{r.x + 4, r.y + 4 + row * 32, r.w - 8, 31};
      if (enabled)
        button(hit, id, label);
      else
        center(hit.x, hit.y + (hit.h-24)/2, hit.w, label, 0, muted());
    };
    if(shortcut_context_id==11){
      item(0,815,"Open Recycle Bin",true);
      item(1,814,"Empty Recycle Bin",true);
    } else if(shortcut_context_id){
      const int target=shortcut_context_id==13?30:shortcut_context_id==10?32:shortcut_context_id;
      const bool pinned=shortcut_context_app?(vapor_shortcuts.placements(shortcut_context_app)&VaporShortcuts::Taskbar):!(taskbar_hidden&(std::uint64_t(1)<<target));
      item(0,811,pinned?"Unpin from taskbar":"Pin to taskbar",true);
      if(!shortcut_context_taskbar)item(1,812,"Remove desktop shortcut",true);

    } else if (file_context_item) {
      const bool multiple=!file_context_desktop&&file_selection.size()>1;
      const bool folder=file_context_info.directory&&!file_context_desktop;
      int row=0;item(row++,800,"Open",!multiple);
      if(folder)item(row++,818,"Open in new tab",!multiple);
      item(row++,801,"Copy",true);item(row++,802,"Cut",true);item(row++,803,"Paste",file_clipboard.ready);
      if(!file_context_desktop)item(row++,816,"Rename",!multiple);
      item(row++,809,"Delete",true);
      if(!file_context_desktop)item(row++,817,"Copy path",!multiple);
      item(row++,804,"Properties",true);

    } else {
      item(0, 803, "Paste", file_clipboard.ready);
      item(1, 805, "Refresh", true);
      item(2, 808, "New >", true);
      item(3, 814, "Empty folder Recycle Bin", true);
      item(4, 2107, "Sort", true);
      item(5, 2108, "View", true);
      item(6, 2130, "Select all", true);
      item(7, 804, "Properties", true);

    }
  }
  if(file_context&&file_new&&!file_menu_kind){panel(file_new_bounds);const char*names[]={"Folder","Text document","PNG image","Excel Workbook (.xlsx)","CSV spreadsheet"};for(int i=0;i<5;++i)button({file_new_bounds.x+4,file_new_bounds.y+4+i*32,file_new_bounds.w-8,31},1420+i,names[i]);}
  if (file_properties) {
    rect({0, 0, W, H}, 0x38000000);
    const Rect r{500, 275, 920, 520};
    panel(r);
    text(r.x + 30, r.y + 24, "Properties", 2, ink());
    text_fit(r.x + 30, r.y + 80, r.w - 60, filename(file_properties_path), 1,
             ink());
    text(r.x + 30, r.y + 140, "Type", 0, muted());
    text(r.x + 200, r.y + 140,
         file_properties_multiple?"Multiple items":file_properties_info.directory ? "Folder"
         : file_properties_info.regular ? "File"
                                        : "Special file",
         0, ink());
    text(r.x + 30, r.y + 190, "Location", 0, muted());
    text_fit(r.x + 200, r.y + 190, r.w - 230, file_friendly_path(file_properties_path).c_str(), 0, ink());
    char size[90];
    if (file_properties_info.directory)
      std::snprintf(size, sizeof(size), "Contents not counted");
    else
      std::snprintf(size, sizeof(size), "%llu bytes",
                    static_cast<unsigned long long>(file_properties_info.size));
    text(r.x + 30, r.y + 240, "Size", 0, muted());
    text(r.x + 200, r.y + 240, size, 0, ink());
    char modified[90] = "Unavailable";
    std::time_t time = std::time_t(file_properties_info.modified);
    if (file_properties_info.modified)if (const auto *when = std::localtime(&time))
      std::strftime(modified, sizeof(modified), "%Y-%m-%d  %H:%M:%S", when);
    text(r.x + 30, r.y + 290, "Modified", 0, muted());
    text(r.x + 200, r.y + 290, modified, 0, ink());
    text(r.x + 30, r.y + 340, "Attributes", 0, muted());
    text(r.x + 200, r.y + 340,
         file_properties_multiple?"Mixed permissions":file_properties_info.mode & 0222 ? "Writable permissions"
                                          : "Read-only permissions",
         0, ink());
    button({r.x + r.w - 174, r.y + r.h - 70, 144, 44}, 806, "Close");
  }
  if(file_rename_prompt){
    rect({0,0,W,H},0x55000000);Rect r{540,touch_keyboard?225:370,840,260};panel(r);
    text(r.x+30,r.y+26,"Rename",1,ink());text(r.x+30,r.y+72,"Enter a new name for this item.",0,muted());
    Rect field{r.x+30,r.y+110,r.w-60,44};round(field,4,dark?0xff191919:0xffffffff);add(field,2178);
    text_fit(field.x+12,field.y+10,field.w-24,file_rename_name,0,ink());
    if(text_target==TextTarget::file_rename&&!text_selection){char prefix[256];std::snprintf(prefix,sizeof(prefix),"%.*s",std::max(0,text_caret),file_rename_name);rect({field.x+12+std::min(field.w-28,width(prefix)),field.y+9,2,25},accent_color());}
    button({r.x+492,r.y+192,146,42},2176,"Rename");button({r.x+652,r.y+192,146,42},2177,"Cancel");
  }
  if(file_recycle_active){rect({0,0,W,H},0x38000000);Rect r{540,350,840,280};panel(r);text(r.x+30,r.y+28,"Moving items to Recycle Bin",1,ink());char status[100];std::snprintf(status,sizeof(status),"%zu of %zu items",file_recycle_index,file_recycle_queue.size());text(r.x+30,r.y+90,status,0,ink());button({r.x+r.w-174,r.y+r.h-70,144,44},807,"Stop");}
  if(file_purge.busy()){
    rect({0,0,W,H},0x38000000);const Rect r{540,350,840,300};panel(r);
    text(r.x+30,r.y+28,"Emptying Recycle Bin",1,ink());
    char progress[120];std::snprintf(progress,sizeof(progress),"%zu of %zu files and folders removed",file_purge.removed,file_purge.entries.size()-1);
    text(r.x+30,r.y+88,progress,0,ink());
    text(r.x+30,r.y+128,"Permanent deletion cannot be undone.",0,muted());
    button({r.x+r.w-174,r.y+r.h-70,144,44},807,"Stop");
  }
  if (file_transfer.busy()) {
    rect({0, 0, W, H}, 0x38000000);
    const Rect r{540, 350, 840, 360};
    panel(r);
    text(r.x + 30, r.y + 28,
         file_transfer.state == fileops::State::copying ? file_transfer.moving ? "Moving item" : "Copying item"
                                                        : "Verifying copy",
         1, ink());
    text_fit(r.x + 30, r.y + 84, r.w - 60, filename(file_transfer.source), 0,
             ink());
    text_fit(r.x + 30, r.y + 125, r.w - 60, file_transfer.destination, 0,
             muted());
    const uint64_t done = file_transfer.state == fileops::State::copying
                              ? file_transfer.copied
                              : file_transfer.verified;
    const int percent =
        file_transfer.total ? int(done * 100 / file_transfer.total) : 100;
    round({r.x + 30, r.y + 183, r.w - 60, 12}, 6,
          dark ? 0xff514d48 : 0xffd8d2cc);
    round({r.x + 30, r.y + 183, (r.w - 60) * percent / 100, 12}, 6,
          accent_color());
    char progress[100];
    std::snprintf(progress, sizeof(progress), "%d%% | %llu / %llu bytes",
                  percent, static_cast<unsigned long long>(done),
                  static_cast<unsigned long long>(file_transfer.total));
    text(r.x + 30, r.y + 215, progress, 0, muted());
    text(r.x + 30, r.y + 255, "Existing files are never overwritten.", 0,
         muted());
    button({r.x + r.w - 180, r.y + r.h - 60, 150, 42}, 807, "Cancel copy");
  }
}
} // namespace ww
