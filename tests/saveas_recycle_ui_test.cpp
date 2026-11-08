#include "../src/shell.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
void put(const fs::path&p,const std::string&s){std::ofstream(p,std::ios::binary)<<s;}
std::string get(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
ww::Rect hit(ww::Shell&s,int id){s.draw();for(int i=0;i<s.count;++i)if(s.hits[i].id==id)return s.hits[i].r;assert(false);return {};}
void click(ww::Shell&s,int id){auto r=hit(s,id);s.pointer(r.x+r.w/2,r.y+r.h/2,true);}
void name(ww::Shell&s,const char*value){s.activate(618);s.insert_text(value);assert(std::string(s.notepad_filename)==value);}
void capture(ww::Shell&s,const char*path){s.draw();FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",ww::W,ww::H);for(int i=0;i<ww::W*ww::H;++i)fwrite(s.pixels+i,1,3,f);fclose(f);}
int main(){auto root=fs::absolute(fs::path("build")/("saveas-purge-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));fs::create_directories(root/"Desktop");auto source=root/"Desktop"/"source.txt";put(source,"original");
 ww::Shell s;assert(s.init("assets",nullptr,nullptr,nullptr,(root/"settings.ini").string().c_str()));assert(s.notepad_open(source.string().c_str()));s.document.end(true);s.insert_text(" edited");s.activate(606);assert(s.notepad_save_dialog);s.physical_key(0x0f,1);assert(s.window==4&&s.text_target==ww::TextTarget::filename);name(s,"Named document.txt");capture(s,"build/save-as-name.ppm");click(s,607);assert(!s.notepad_modal()&&get(source)=="original"&&get(root/"Desktop"/"Named document.txt")=="original edited");
 put(root/"Desktop"/"existing.txt","keep existing");s.activate(606);name(s,"existing.txt");click(s,607);assert(s.notepad_replace&&get(root/"Desktop"/"existing.txt")=="keep existing");capture(s,"build/save-as-replace.ppm");click(s,619);assert(!s.notepad_replace&&s.notepad_save_dialog);click(s,607);assert(s.notepad_replace);click(s,609);assert(!s.notepad_modal()&&get(root/"Desktop"/"existing.txt")=="original edited");
 s.document.end(true);s.insert_text(" close edit");s.notepad_close();assert(s.notepad_prompt);click(s,618);s.insert_text("Closing name.txt");capture(s,"build/save-close-name.ppm");click(s,610);assert(!s.notepad_modal()&&s.window==1&&get(root/"Desktop"/"Closing name.txt")=="original edited close edit");
 assert(s.notepad_open(source.string().c_str()));s.activate(601);assert(s.notepad_save_dialog);name(s,"regular save");s.physical_key(0x28,0);assert(get(root/"Desktop"/"regular save.txt")=="original");
 s.input_device=ww::InputDevice::controller;s.activate(606);assert(s.touch_keyboard&&s.notepad_save_dialog);click(s,200);assert(std::string(s.notepad_filename)=="1");click(s,248);assert(!s.notepad_modal()&&get(root/"Desktop"/"1.txt")=="original");
 // Replacement refusal preserves unsaved edits if the confirmed target changes.
 s.document.end(true);s.insert_text(" unsafe");s.activate(606);name(s,"existing.txt");click(s,607);assert(s.notepad_replace);put(root/"Desktop"/"existing.txt","external change");click(s,609);assert(s.dialog&&s.document.dirty()&&get(root/"Desktop"/"existing.txt")=="external change");s.activate(60);s.activate(608);assert(s.document.dirty());
 s.document.close(true);s.window=0;s.end_text();s.input_device=ww::InputDevice::mouse;s.desktop_files_next_scan=0;s.draw();auto bin=hit(s,11);s.pointer(bin.x+5,bin.y+5,false);s.open_context();assert(s.file_context&&s.file_context_bounds.w==240&&s.file_context_bounds.h==72);capture(s,"build/recycle-empty-menu.ppm");
 fs::create_directories(root/"Desktop"/".winbrew-trash"/"box"/"nested");put(root/"Desktop"/".winbrew-trash"/"box"/"nested"/"deleted.txt","old");click(s,814);assert(s.recycle_confirmation&&s.focus==60);capture(s,"build/recycle-empty-confirm.ppm");click(s,60);assert(fs::exists(root/"Desktop"/".winbrew-trash"/"box"/"nested"/"deleted.txt"));
 s.draw();s.pointer(bin.x+5,bin.y+5,false);s.open_context();click(s,814);click(s,61);assert(s.file_purge.busy());for(int i=0;s.file_purge.busy()&&i<100;++i)s.tick();assert(s.dialog&&fs::is_empty(root/"Desktop"/".winbrew-trash")&&get(source)=="original");s.activate(60);
 // Disabled Paste stays centered and contributes no clickable hit.
 s.desktop_files_next_scan=0;s.window=0;s.draw();auto item=hit(s,2000);s.pointer(item.x+5,item.y+5,false);s.open_context();assert(s.file_context_bounds.w==240&&s.file_context_bounds.h==200);s.draw();bool paste=false;for(int i=0;i<s.count;++i)if(s.hits[i].id==803)paste=true;assert(!paste);capture(s,"build/context-compact-disabled.ppm");
 assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);std::cout<<"PASS: editable Save/Save As/close filename, replace/back/conflict, centered compact menus and confirmed bin emptying with live-file preservation\n";}
