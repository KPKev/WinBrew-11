#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifndef _WIN32
#include <sys/stat.h>
#endif
namespace fs=std::filesystem;
int main(){
#ifdef _WIN32
  std::cout<<"SKIP: POSIX inaccessible folder fixture\n";
#else
  const auto root=fs::temp_directory_path()/("winbrew-recycle-access-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto console=root/"console",profile=console/"WinBrew/Users/Default",bin=profile/"Desktop/.winbrew-trash",blocked=profile/"private-folder",old=profile/"accessible/.winbrew-trash/12-0",outside=console/"system/.winbrew-trash/14-0";
  fs::create_directories(bin);fs::create_directories(old);fs::create_directories(outside);fs::create_directories(blocked/".winbrew-trash/13-0");fs::create_directories(root/"private");
  {std::ofstream(outside/"untouched.txt")<<"outside";std::ofstream(bin/"current.txt")<<"current";std::ofstream(old/"older.txt")<<"old";std::ofstream(blocked/".winbrew-trash/13-0/hidden.txt")<<"hidden";std::ofstream(console/"live.txt")<<"live";}
  assert(chmod(blocked.c_str(),0)==0);
  auto s=std::make_unique<ww::Shell>();assert(s->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
  std::snprintf(s->console_storage_root,sizeof(s->console_storage_root),"%s",console.string().c_str());
  std::snprintf(s->user_files_root,sizeof(s->user_files_root),"%s",profile.string().c_str());
  s->file_location_open(436);for(int i=0;s->file_recycle_active&&i<10000;++i)s->tick();
  assert(!s->file_recycle_active&&!s->dialog&&s->file_recovery_partial&&std::string(s->file_path)==bin.string());
  assert(fs::exists(bin/"current.txt")&&!fs::exists(old/"older.txt")&&s->file_count==2);
  s->empty_recycle_request(nullptr);assert(s->recycle_confirmation&&std::string(s->message).find("older locations")!=std::string::npos);
  s->activate(60);assert(fs::exists(bin/"current.txt"));
  s->empty_recycle_request(nullptr);assert(s->recycle_confirmation);s->activate(61);
  for(int i=0;s->file_purge.busy()&&i<10000;++i)s->tick();
  assert(fs::is_empty(bin)&&fs::exists(outside/"untouched.txt")&&fs::exists(console/"live.txt")&&std::string(s->message).find("older locations")!=std::string::npos);s->activate(60);
  assert(chmod(blocked.c_str(),0700)==0&&fs::exists(blocked/".winbrew-trash/13-0/hidden.txt"));
  s->file_browser_action(2105);for(int i=0;s->file_recycle_active&&i<10000;++i)s->tick();
  assert(!s->dialog&&!s->file_recovery_partial&&!fs::exists(blocked/".winbrew-trash/13-0/hidden.txt")&&s->file_count==1);
  assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
  std::cout<<"PASS: inaccessible folders do not block the bin; warning, cancel, visible empty and retry preserve files\n";
#endif
}
