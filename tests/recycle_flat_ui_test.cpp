#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(){
 const auto root=fs::temp_directory_path()/("wb-flat-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto profile=root/"profile",desktop=profile/"Desktop",bin=desktop/".winbrew-trash",live=desktop/"example.txt",system=root/"system.txt";
 fs::create_directories(bin);fs::create_directories(root/"private");std::ofstream(live)<<"user document";std::ofstream(system)<<"system fixture";
 ww::fileops::Info before{};assert(ww::fileops::inspect(live.string().c_str(),before)==0);char path[1024];assert(ww::fileops::trash(live.string().c_str(),before,path,sizeof(path),bin.string().c_str()));assert(!fs::exists(live));std::string origin;assert(ww::fileops::recovery_original_path(fs::path(path).parent_path().string().c_str(),"example.txt",origin) && origin==live.string());
 auto shell=std::make_unique<ww::Shell>();assert(shell->init("assets",nullptr,nullptr,nullptr,(root/"private/settings.ini").string().c_str()));
 std::snprintf(shell->user_files_root,sizeof(shell->user_files_root),"%s",profile.string().c_str());
 shell->file_location_open(436);for(int i=0;shell->file_recycle_active&&i<10000;++i)shell->tick();
 assert(!shell->dialog && shell->file_count==1);auto &entry=shell->files[0];assert(entry.display_name=="example.txt" && !entry.directory);assert(fs::exists(shell->file_entry_path(entry)));
 shell->file_select_row(0);shell->file_context_selected();shell->file_action(823);assert(shell->file_restore_active);for(int i=0;shell->file_restore_active&&i<10000;++i)shell->tick();assert(fs::exists(live)&&fs::is_empty(bin));assert(std::string(shell->message).find("restored")!=std::string::npos);shell->activate(60);
 assert(!ww::fileops::inspect(live.string().c_str(),before));assert(ww::fileops::trash(live.string().c_str(),before,path,sizeof(path),bin.string().c_str()));shell->file_refresh();assert(shell->file_count==1);
 shell->file_select_row(0);shell->file_context_selected();shell->file_action(809);assert(shell->recycle_confirmation);shell->activate(60);assert(fs::exists(shell->file_entry_path(shell->files[0])));
 shell->file_select_row(0);shell->file_context_selected();shell->file_action(809);assert(shell->recycle_confirmation);shell->activate(61);for(int i=0;shell->file_purge.busy()&&i<10000;++i)shell->tick();
 assert(fs::is_empty(bin)&&fs::exists(system));shell->activate(60);
 const std::string long_name=std::string(220,'x')+".txt";const auto long_file=desktop/long_name;std::ofstream(long_file)<<"long name";assert(!ww::fileops::inspect(long_file.string().c_str(),before));assert(ww::fileops::trash(long_file.string().c_str(),before,path,sizeof(path),bin.string().c_str()));shell->file_refresh();assert(shell->file_count==1&&shell->files[0].display_name==long_name&&fs::exists(shell->file_entry_path(shell->files[0])));shell->file_select_row(0);shell->file_context_selected();shell->file_action(823);for(int i=0;shell->file_restore_active&&i<10000;++i)shell->tick();assert(fs::exists(long_file)&&fs::is_empty(bin));
 shell->activate(60);fs::create_directories(bin/"12345-0");std::ofstream(bin/"12345-0/old.txt")<<"old";shell->file_refresh();assert(shell->file_count==1&&shell->files[0].display_name=="old.txt"&&!shell->files[0].directory);shell->file_select_row(0);shell->file_context_selected();shell->file_action(823);for(int i=0;shell->file_restore_active&&i<10000;++i)shell->tick();assert(fs::exists(desktop/"old.txt")&&fs::is_empty(bin));
 assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);std::cout<<"PASS: file appears directly; restore, cancel, and confirm affect only selected item and wrapper\n";
}
