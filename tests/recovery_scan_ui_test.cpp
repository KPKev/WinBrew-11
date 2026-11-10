#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
  const auto root=fs::absolute(fs::path("build")/("scan-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
  const auto console=root/"console",profile=console/"Users/Default",old=console/"Other/Deep/.winbrew-trash/123-0";
  fs::create_directories(profile/"Desktop");fs::create_directories(old);fs::create_directories(root/"private");fs::create_directories(console/"Bulk");
  for(int i=0;i<100;++i)fs::create_directory(console/"Bulk"/("folder-"+std::to_string(i)));
  {std::ofstream f(old/"old.txt");f<<"old item";}{std::ofstream f(console/"live.txt");f<<"keep live";}
  for(int i=0;i<260;++i){std::ofstream f(old/("batch-"+std::to_string(i)));f<<i;}
  auto shell=std::make_unique<ww::Shell>();assert(shell->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
  std::snprintf(shell->console_storage_root,sizeof(shell->console_storage_root),"%s",console.string().c_str());
  std::snprintf(shell->user_files_root,sizeof(shell->user_files_root),"%s",profile.string().c_str());
  shell->file_location_open(436);assert(shell->file_recycle_active&&shell->file_recovery_walker);
  shell->tick();shell->activate(807);assert(!shell->file_recycle_active&&fs::exists(old/"old.txt")&&read(console/"live.txt")=="keep live");shell->activate(60);
  shell->file_location_open(436);assert(shell->file_recycle_active);
  for(int i=0;shell->file_recycle_active&&i<100000;++i)shell->tick();
  assert(!shell->file_recycle_active&&!shell->dialog&&!fs::exists(old/"old.txt"));
  assert(fs::is_empty(old));
  assert(std::string(shell->file_path)==(profile/"Desktop/.winbrew-trash").string());
  bool visible=false;for(const auto &e:fs::directory_iterator(profile/"Desktop/.winbrew-trash"))if(fs::exists(e.path()/"old.txt")&&read(e.path()/"old.txt")=="old item")visible=true;
  size_t recovered=0;for(const auto &e:fs::directory_iterator(profile/"Desktop/.winbrew-trash")){(void)e;++recovered;}
  assert(visible&&recovered==261&&read(console/"live.txt")=="keep live");
  shell->empty_recycle_request((profile/"Desktop").string().c_str());
  assert(!shell->file_recycle_active&&shell->recycle_confirmation);shell->activate(60);
  assert(!fs::is_empty(profile/"Desktop/.winbrew-trash"));
  // If an old location cannot be interpreted, show the known bin and warn;
  // never claim that everything has been checked or permanently delete it.
  fs::create_directories(console/"Bad");{std::ofstream f(console/"Bad/.winbrew-trash");f<<"not a store";}
  auto incomplete=std::make_unique<ww::Shell>();assert(incomplete->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
  std::snprintf(incomplete->console_storage_root,sizeof(incomplete->console_storage_root),"%s",console.string().c_str());
  std::snprintf(incomplete->user_files_root,sizeof(incomplete->user_files_root),"%s",profile.string().c_str());
  incomplete->file_location_open(436);for(int i=0;incomplete->file_recycle_active&&i<100000;++i)incomplete->tick();
  assert(incomplete->dialog&&!incomplete->recycle_confirmation&&std::string(incomplete->file_path)==(profile/"Desktop/.winbrew-trash").string());
  assert(!fs::is_empty(profile/"Desktop/.winbrew-trash")&&read(console/"Bad/.winbrew-trash")=="not a store");
  assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);
  std::cout<<"PASS: cancellable full-storage discovery, migration, visible items and cached empty confirmation\n";
}
