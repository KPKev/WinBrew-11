#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(){
 const auto root=fs::temp_directory_path()/("winbrew-large-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto bin=root/"Desktop/.winbrew-trash";fs::create_directories(bin);
 for(int i=0;i<66000;++i){std::ofstream file(bin/("item-"+std::to_string(i)));}
 auto shell=std::make_unique<ww::Shell>();assert(shell->init("assets",nullptr,nullptr,nullptr,(root/"settings.ini").string().c_str()));
 shell->file_open(bin.string().c_str());shell->file_update_matches();
 assert(shell->file_location==436&&shell->file_count==65535&&shell->file_truncated&&shell->file_matches.size()==65535);
 shell->file_browser_action(2180);
 assert(shell->file_recycle_batch_offset==65535&&shell->file_count==465&&!shell->file_truncated);
 const std::string selected=shell->files[0].name;
 shell->file_browser_action(2179);assert(shell->file_recycle_batch_offset==0&&shell->file_count==65535);
 shell->file_browser_action(2180);assert(shell->file_recycle_batch_offset==65535&&shell->file_count==465);
 shell->file_selected=0;
 assert(shell->file_selected>=0);shell->file_recycle_selection();assert(shell->recycle_confirmation);shell->activate(61);
 for(int i=0;shell->file_purge.busy()&&i<100;++i)shell->tick();
 assert(!fs::exists(bin/selected));
 assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
 std::cout<<"PASS: a bin with 66000 items supports paging and individual deletion\n";
}
