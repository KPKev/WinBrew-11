#include "../src/shell.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
void put(const fs::path&p,const std::string&s){std::ofstream(p)<<s;}
void capture(ww::Shell&s,const char*path){s.draw();FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",ww::W,ww::H);for(int i=0;i<ww::W*ww::H;++i)fwrite(s.pixels+i,1,3,f);fclose(f);}
int index(ww::Shell&s,const char*n){for(int i=0;i<s.file_count;i++)if(std::string(s.files[i].name)==n)return i;assert(false);return -1;}
void settle(ww::Shell&s){for(int i=0;s.files_busy()&&i<2000;i++)s.tick();assert(!s.files_busy());if(s.dialog)s.activate(60);}
int main(){auto root=fs::absolute(fs::path("build")/("explorer-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));auto desk=root/"Desktop";fs::create_directories(desk/"Projects");fs::create_directories(desk/"Destination");put(desk/"Notes.txt","Hello from WinBrew\nA readable preview.");put(desk/"Report 10.txt","ten");put(desk/"Report 2.txt","two");put(desk/".hidden.txt","hidden");for(int i=0;i<50;i++)put(desk/("Document "+std::to_string(i)+".txt"),"sample");
assert(ww::fileops::volume_mount_matches("/mnt/usb0","/mnt/usb0/"));
assert(!ww::fileops::volume_mount_matches("/mnt/usb0","/mnt"));
assert(!ww::fileops::volume_mount_matches("/mnt/usb0","/mnt/usb01"));
assert(!ww::fileops::volume_mount_matches("/mnt/usb0", ""));
uint64_t total=0,available=0;assert(!ww::fileops::volume_space(root.string().c_str(),total,available,root.string().c_str())&&total==0&&available==0);assert(ww::fileops::volume_space(root.string().c_str(),total,available)&&total>0&&available<=total);assert(!ww::fileops::volume_space((root/"absent").string().c_str(),total,available)&&total==0&&available==0);
ww::Shell s;assert(s.init("assets",nullptr,nullptr,nullptr,(root/"settings.ini").string().c_str()));s.window=1;s.file_location_open(420);s.draw();capture(s,"build/explorer-this-pc.ppm");s.file_open(desk.string().c_str());s.draw();assert(s.file_current_label()=="Desktop");assert(s.file_friendly_path(desk.string().c_str())=="C:\\Desktop");assert(s.file_matches.size()==55);assert(index(s,"Report 2.txt")<index(s,"Report 10.txt"));capture(s,"build/explorer-details.ppm");
s.file_browser_action(2103);s.insert_text("Report");s.draw();assert(s.file_matches.size()==2);s.end_text();s.file_browser_action(2125);s.draw();s.file_select_row(index(s,"Notes.txt"));s.file_browser_action(2127);capture(s,"build/explorer-preview.ppm");s.file_browser_action(2127);
s.file_begin_rename();assert(s.file_rename_prompt);s.insert_text("Renamed.txt");s.file_rename_submit();assert(!s.file_rename_prompt&&fs::exists(desk/"Renamed.txt"));s.file_begin_rename();s.insert_text("Report 2.txt");s.file_rename_submit();assert(s.dialog&&fs::exists(desk/"Renamed.txt"));s.activate(60);s.file_browser_action(2177);
s.file_select_row(index(s,"Renamed.txt"));s.file_select_row(index(s,"Report 2.txt"),true);assert(s.file_selection.size()==2);s.file_copy_selection(false);s.file_open((desk/"Destination").string().c_str());s.file_browser_action(2142);settle(s);assert(fs::exists(desk/"Destination"/"Renamed.txt")&&fs::exists(desk/"Destination"/"Report 2.txt"));
s.file_history_move(-1);assert(std::string(s.file_path)==desk.string());s.file_tab_new((desk/"Projects").string().c_str());assert(s.file_tabs.size()==2);s.file_tab_switch(0);assert(std::string(s.file_path)==desk.string());s.file_tab_close(1);assert(s.file_tabs.size()==1);
s.file_browser_action(2102);s.insert_text("C:\\Desktop\\Destination");s.file_address_submit();assert(std::string(s.file_path)==(desk/"Destination").string());s.file_up();s.draw();s.file_scroll_by(20);s.draw();assert(s.file_scroll>0);s.file_browser_action(2122);capture(s,"build/explorer-tiles.ppm");s.file_browser_action(2120);s.file_scroll=0;s.draw();
s.file_select_row(index(s,"Renamed.txt"));s.file_drag_begin();assert(s.file_drag_active);s.draw();int dest=index(s,"Projects"),slot=-1;for(int i=0;i<s.file_visible_count;i++)if(s.file_visible[i]==dest)slot=i;assert(slot>=0);for(int i=0;i<s.count;i++)if(s.hits[i].id==500+slot){s.mx=s.hits[i].r.x+12;s.my=s.hits[i].r.y+12;}s.file_drop();settle(s);assert(fs::exists(desk/"Projects"/"Renamed.txt")&&!fs::exists(desk/"Renamed.txt"));
ww::fileops::Info info;int error=0;auto project=desk/"Projects";assert(ww::fileops::inspect(project.string().c_str(),info)==0);assert(ww::fileops::rename_item(project.string().c_str(),info,"Work",error));assert(fs::exists(desk/"Work"/"Renamed.txt"));assert(!ww::fileops::rename_item((desk/"Work").string().c_str(),info,"Destination",error));assert(fs::exists(desk/"Destination"));
s.file_refresh();s.file_scroll=0;s.file_page=s.file_compat_page=0;s.draw();
// Right-click navigation gets location actions, not destructive item actions.
s.input_device=ww::InputDevice::mouse;
for(int i=0;i<s.count;i++)if(s.hits[i].id>=2400&&s.hits[i].id<2656){auto n=s.hits[i].id-2400;if(s.file_nav[n].label=="Documents"){s.mx=s.hits[i].r.x+70;s.my=s.hits[i].r.y+10;break;}}
assert(s.file_context_open()&&s.file_menu_kind==5);s.draw();bool newtab=false;for(int i=0;i<s.count;i++)if(s.hits[i].id==2151)newtab=true;assert(newtab);s.file_browser_action(2151);assert(s.file_tabs.size()==2);s.file_tab_close(s.file_tab);s.file_open(desk.string().c_str());
s.file_browser_action(2107);assert(s.file_menu_kind==1);s.file_browser_action(2113);assert(s.file_sort==3);s.file_browser_action(2110);s.file_sort_descending=false;s.file_sort_list();s.file_browser_action(2132);s.draw();assert(s.file_matches.size()==s.files.size());s.file_browser_action(2132);
// Cut a batch and recycle the moved fixture items without touching other files.
s.file_open((desk/"Destination").string().c_str());s.draw();s.file_browser_action(2130);s.file_copy_selection(true);s.file_open((desk/"Work").string().c_str());s.file_browser_action(2142);settle(s);assert(!fs::exists(desk/"Destination"/"Renamed.txt")&&!fs::exists(desk/"Destination"/"Report 2.txt"));assert(fs::exists(desk/"Work"/"Report 2.txt"));s.file_refresh();s.file_select_row(index(s,"Report 2.txt"));s.file_recycle_selection();settle(s);assert(!fs::exists(desk/"Work"/"Report 2.txt")&&fs::exists(desk/"Work"/"Renamed.txt"));
s.file_open(desk.string().c_str());s.dark=true;capture(s,"build/explorer-dark.ppm");
// A disappeared device must vanish even when its old connection directory exists.
auto detached=root.parent_path()/(root.filename().string()+"-detached");fs::create_directory(detached);put(detached/"old.txt","fixture");s.file_open(detached.string().c_str());s.file_drives.push_back({"USB test",detached.string(),"E:",0});s.file_branches.push_back({detached.string(),{"stale child"}});s.file_drive_refresh();assert(s.file_drives.size()==2&&s.file_drives[0].letter=="C:"&&s.file_drives[1].letter=="D:");assert(s.file_error==ENODEV&&s.files.empty()&&s.file_branches.empty());s.file_location_open(420);s.draw();capture(s,"build/explorer-connected-drives-only.ppm");assert(detached.parent_path()==fs::absolute("build"));fs::remove_all(detached);
assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);std::cout<<"PASS: Explorer actual UI navigation, search, natural sort, rename/no-overwrite, multi-copy, tabs/history, aliases, scrolling, preview and drag-move\n";
}
