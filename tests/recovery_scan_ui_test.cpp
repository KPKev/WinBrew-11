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
  const auto console=root/"console",profile=console/"Users/Default",old=profile/"Other/Deep/.winbrew-trash/123-0";
  fs::create_directories(profile/"Desktop");fs::create_directories(old);fs::create_directories(root/"private");fs::create_directories(console/"Bulk");
  for(int i=0;i<100;++i)fs::create_directory(console/"Bulk"/("folder-"+std::to_string(i)));
  const auto nested=profile/"Desktop/.winbrew-trash/.winbrew-trash";fs::create_directories(nested/"800-0/900-0");{std::ofstream f(nested/"800-0/900-0/nested.txt");f<<"nested";}
  const auto deep=nested/"801-0/901-0/.winbrew-trash/902-0/903-0";fs::create_directories(deep);{std::ofstream f(deep/"deep.txt");f<<"deep";}
  {std::ofstream f(old/"old.txt");f<<"old item";}{std::ofstream f(console/"live.txt");f<<"keep live";}
  const auto recorded=profile/"recorded.txt",legacy_store=profile/"Documents/.winbrew-trash";fs::create_directories(legacy_store);{std::ofstream f(recorded);f<<"recorded";}
  ww::fileops::Info recorded_info{};char recorded_path[1024]{};assert(!ww::fileops::inspect(recorded.string().c_str(),recorded_info));assert(ww::fileops::trash(recorded.string().c_str(),recorded_info,recorded_path,sizeof(recorded_path),legacy_store.string().c_str()));
  const auto nested_recorded=profile/"nested-recorded.txt";{std::ofstream f(nested_recorded);f<<"recorded nested";}ww::fileops::Info nested_recorded_info{};char nested_recorded_path[1024]{};assert(!ww::fileops::inspect(nested_recorded.string().c_str(),nested_recorded_info));assert(ww::fileops::trash(nested_recorded.string().c_str(),nested_recorded_info,nested_recorded_path,sizeof(nested_recorded_path),legacy_store.string().c_str()));const auto nested_wrapper=fs::path(nested_recorded_path).parent_path();fs::rename(nested_wrapper,nested/"804-0");
  for(int i=0;i<260;++i){std::ofstream f(old/("batch-"+std::to_string(i)));f<<i;}
  auto shell=std::make_unique<ww::Shell>();assert(shell->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
  std::snprintf(shell->console_storage_root,sizeof(shell->console_storage_root),"%s",console.string().c_str());
  std::snprintf(shell->user_files_root,sizeof(shell->user_files_root),"%s",profile.string().c_str());
  shell->file_location_open(436);assert(shell->file_recycle_active&&shell->file_recovery_walker);
  shell->tick();shell->activate(807);assert(!shell->file_recycle_active&&fs::exists(old/"old.txt")&&read(console/"live.txt")=="keep live");shell->activate(60);
  shell->file_location_open(436);assert(shell->file_recycle_active);
  for(int i=0;shell->file_recycle_active&&i<100000;++i)shell->tick();
  assert(!shell->file_recycle_active&&!shell->dialog&&!fs::exists(old/"old.txt"));
  assert(!fs::exists(old));
  assert(std::string(shell->file_path)==(profile/"Desktop/.winbrew-trash").string());
  bool visible=false;for(const auto &e:fs::directory_iterator(profile/"Desktop/.winbrew-trash"))if(fs::exists(e.path()/"old.txt")&&read(e.path()/"old.txt")=="old item")visible=true;
  size_t recovered=0;for(const auto &e:fs::directory_iterator(profile/"Desktop/.winbrew-trash")){(void)e;++recovered;}
  assert(visible&&recovered==265&&read(console/"live.txt")=="keep live"&&!fs::exists(recorded_path)&&!fs::exists(nested));
  bool kept_origin=false;for(const auto &e:fs::directory_iterator(profile/"Desktop/.winbrew-trash")){std::string item,origin;ww::fileops::Info info{};if(ww::fileops::recovery_container_item(e.path().string().c_str(),item,info)&&item=="recorded.txt"&&ww::fileops::recovery_original_path(e.path().string().c_str(),item.c_str(),origin)&&origin==recorded.string())kept_origin=true;}assert(kept_origin);
  shell->empty_recycle_request((profile/"Desktop").string().c_str());
  assert(!shell->file_recycle_active&&shell->recycle_confirmation);shell->activate(60);
  assert(!fs::is_empty(profile/"Desktop/.winbrew-trash"));
  // If an old location cannot be interpreted, show the known bin and warn;
  // never claim that everything has been checked or permanently delete it.
  fs::create_directories(profile/"Bad");{std::ofstream f(profile/"Bad/.winbrew-trash");f<<"not a store";}
  auto incomplete=std::make_unique<ww::Shell>();assert(incomplete->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
  std::snprintf(incomplete->console_storage_root,sizeof(incomplete->console_storage_root),"%s",console.string().c_str());
  std::snprintf(incomplete->user_files_root,sizeof(incomplete->user_files_root),"%s",profile.string().c_str());
  incomplete->file_location_open(436);for(int i=0;incomplete->file_recycle_active&&i<100000;++i)incomplete->tick();
  assert(!incomplete->dialog&&!incomplete->recycle_confirmation&&incomplete->file_recovery_partial&&std::string(incomplete->file_path)==(profile/"Desktop/.winbrew-trash").string());
  assert(!fs::is_empty(profile/"Desktop/.winbrew-trash")&&read(profile/"Bad/.winbrew-trash")=="not a store");
  incomplete->empty_recycle_request(nullptr);for(int i=0;incomplete->file_recycle_active&&i<100000;++i)incomplete->tick();
  assert(incomplete->recycle_confirmation&&std::string(incomplete->message).find("older locations")!=std::string::npos);
  incomplete->activate(60);assert(!fs::is_empty(profile/"Desktop/.winbrew-trash")&&read(profile/"Bad/.winbrew-trash")=="not a store");
  assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);
  std::cout<<"PASS: cancellable user-folder discovery, migration, visible items and cached empty confirmation\n";
}
