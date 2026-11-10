#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
static std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
  const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root=fs::absolute(fs::path("build")/("cross-ui-"+unique));
  const auto external=fs::path("/dev/shm")/("winbrew-cross-ui-"+unique);
  if(!fs::is_directory("/dev/shm")){std::cout<<"SKIP: no separate temporary volume\n";return 0;}
  fs::create_directories(root/"Desktop");fs::create_directory(external);
  {std::ofstream f(external/"from-drive.txt",std::ios::binary);f<<"external contents";}
  ww::fileops::Info src{},dst{};
  assert(ww::fileops::inspect((external/"from-drive.txt").string().c_str(),src)==0);
  assert(ww::fileops::inspect((root/"Desktop").string().c_str(),dst)==0);
  if(src.device==dst.device){fs::remove_all(external);fs::remove_all(root);std::cout<<"SKIP: no separate temporary volume\n";return 0;}
  auto shell=std::make_unique<ww::Shell>();assert(shell->init("assets",nullptr,nullptr,nullptr,(root/"settings.ini").string().c_str()));
  shell->file_open(external.string().c_str());shell->file_selected=-1;
  for(int i=0;i<shell->file_count;++i)if(std::string(shell->files[i].name)=="from-drive.txt")shell->file_selected=i;
  assert(shell->file_selected>=0);shell->file_context_selected();shell->activate(809);
  assert(shell->file_recycle_active&&fs::exists(external/"from-drive.txt"));
  for(int i=0;shell->file_recycle_active&&i<100000;++i)shell->tick();
  assert(!shell->file_recycle_active&&!fs::exists(external/"from-drive.txt"));
  assert(read(root/"Desktop/.winbrew-trash/from-drive.txt")=="external contents");
  shell->activate(60);shell->file_location_open(436);for(int i=0;shell->file_recycle_active&&i<100000;++i)shell->tick();shell->file_selected=-1;
  for(int i=0;i<shell->file_count;++i)if(std::string(shell->files[i].name)=="from-drive.txt")shell->file_selected=i;
  assert(shell->file_selected>=0);shell->file_recycle_selection();assert(shell->recycle_confirmation);shell->activate(61);
  for(int i=0;shell->file_purge.busy()&&i<100000;++i)shell->tick();
  assert(!fs::exists(root/"Desktop/.winbrew-trash/from-drive.txt"));
  shell->activate(60);{std::ofstream f(external/"empty-all.txt",std::ios::binary);f<<"second item";}
  shell->file_open(external.string().c_str());shell->file_selected=-1;
  for(int i=0;i<shell->file_count;++i)if(std::string(shell->files[i].name)=="empty-all.txt")shell->file_selected=i;
  assert(shell->file_selected>=0);shell->file_context_selected();shell->activate(809);
  for(int i=0;shell->file_recycle_active&&i<100000;++i)shell->tick();
  assert(!fs::exists(external/"empty-all.txt")&&read(root/"Desktop/.winbrew-trash/empty-all.txt")=="second item");
  shell->activate(60);shell->empty_recycle_request((root/"Desktop").string().c_str());
  assert(shell->recycle_confirmation);shell->activate(60);assert(fs::exists(root/"Desktop/.winbrew-trash/empty-all.txt"));
  shell->empty_recycle_request((root/"Desktop").string().c_str());assert(shell->recycle_confirmation);shell->activate(61);
  for(int i=0;shell->file_purge.busy()&&i<100000;++i)shell->tick();
  assert(fs::is_empty(root/"Desktop/.winbrew-trash"));
  // A retained pre-profile bin on a different volume must be collected before
  // Empty prompts, without blocking the UI or stranding the old data.
  fs::create_directories(external/"Desktop/.winbrew-trash/123-0");
  {std::ofstream f(external/"Desktop/.winbrew-trash/123-0/old.txt",std::ios::binary);f<<"old external data";}
  auto migrated=std::make_unique<ww::Shell>();assert(migrated->init("assets",nullptr,nullptr,nullptr,(external/"settings.ini").string().c_str()));
  std::snprintf(migrated->user_files_root,sizeof(migrated->user_files_root),"%s",root.string().c_str());
  migrated->empty_recycle_request((root/"Desktop").string().c_str());assert(migrated->file_recycle_active&&!migrated->recycle_confirmation);
  migrated->tick();migrated->activate(807);assert(!migrated->file_recycle_active&&fs::exists(external/"Desktop/.winbrew-trash/123-0/old.txt"));migrated->activate(60);
  migrated->empty_recycle_request((root/"Desktop").string().c_str());assert(migrated->file_recycle_active&&!migrated->recycle_confirmation);
  for(int i=0;migrated->file_recycle_active&&i<100000;++i)migrated->tick();
  assert(migrated->recycle_confirmation&&!fs::exists(external/"Desktop/.winbrew-trash/123-0/old.txt"));
  assert(read(root/"Desktop/.winbrew-trash/old.txt")=="old external data");
  migrated->activate(60);assert(fs::exists(root/"Desktop/.winbrew-trash/old.txt"));
  assert(external.parent_path()==fs::path("/dev/shm")&&root.parent_path()==fs::absolute("build"));
  fs::remove_all(external);fs::remove_all(root);
  std::cout<<"PASS: external-volume delete is visible in the bin and individually removable\n";
}
