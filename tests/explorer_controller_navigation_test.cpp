#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
namespace fs=std::filesystem;
int main(){const auto root=fs::temp_directory_path()/("wb-nav-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"parent/child");auto s=std::make_unique<ww::Shell>();assert(s->init("assets",nullptr,nullptr,nullptr,(root/"prefs").string().c_str()));s->window=1;s->file_games=false;s->file_open((root/"parent").string().c_str());s->file_open((root/"parent/child").string().c_str());
 s->controller_input(true,0x1000,128,128,128,128,0);assert(std::string(s->file_path)==(root/"parent").string()&&s->window==1&&!s->file_games);s->controller_input(true,0,128,128,128,128,0);s->controller_input(true,0x8000,128,128,128,128,0);assert(std::string(s->file_path)==(root/"parent/child").string());
 s->controller_input(true,0,128,128,128,128,0);s->dialog=true;s->controller_input(true,0x1000,128,128,128,128,0);assert(s->dialog&&std::string(s->file_path)==(root/"parent/child").string());assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);std::cout<<"PASS: Triangle opens parent; Square follows back history; dialogs retain focus\n";}
