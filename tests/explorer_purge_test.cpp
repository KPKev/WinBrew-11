#include "../src/explorer_fileops.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ww::fileops;namespace fs=std::filesystem;
void put(const fs::path&p,const std::string&s){std::ofstream(p)<<s;}
void finish(Purge&p){for(int i=0;p.busy()&&i<10000;++i)p.step();assert(!p.busy());}
int main(){
 auto root=fs::absolute(fs::path("build")/("purge-fixture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
 fs::create_directories(root/"Folder");auto bin=root/"Folder"/".winbrew-trash";Purge p;
 assert(p.prepare(bin.string().c_str())&&!p.ready&&p.state==State::complete);
 put(root/"Folder"/"outside.txt","Keep this live file");assert(!p.prepare((root/"Folder").string().c_str()));
 fs::create_directories(bin/"container"/"nested"/".winbrew-trash"/"old");put(bin/"container"/"nested"/"one.txt","12345");put(bin/"container"/"nested"/".winbrew-trash"/"old"/"two.txt","678");
 assert(p.prepare(bin.string().c_str())&&p.ready&&p.bytes==8&&p.remaining==6);
 p.cancel();assert(fs::exists(bin/"container"/"nested"/"one.txt"));
 assert(p.prepare(bin.string().c_str()));put(bin/"late.txt","new item");assert(!p.begin());assert(fs::exists(bin/"late.txt"));
 assert(p.prepare(bin.string().c_str())&&p.begin());p.step();p.cancel();assert(p.state==State::cancelled&&p.removed==1&&fs::exists(root/"Folder"/"outside.txt"));
 assert(p.prepare(bin.string().c_str())&&p.begin());finish(p);assert(p.state==State::complete&&fs::is_empty(bin)&&fs::exists(root/"Folder"/"outside.txt"));
 fs::create_directories(bin/"new");put(bin/"new"/"file.txt","original");assert(p.prepare(bin.string().c_str())&&p.begin());put(bin/"new"/"file.txt","changed after confirmation");p.step();assert(p.state==State::failed&&fs::exists(bin/"new"/"file.txt"));
#ifndef _WIN32
 fs::create_directory_symlink(root/"Folder",bin/"escape");assert(!p.prepare(bin.string().c_str()));assert(fs::exists(root/"Folder"/"outside.txt"));fs::remove(bin/"escape");
 fs::create_directory_symlink(root/"Folder",root/"alias");assert(!p.prepare((root/"alias"/".winbrew-trash").string().c_str()));
#endif
 char location[1024];assert(recovery_location((bin/"new").string().c_str(),location,sizeof(location))&&fs::path(location)==bin);
 assert(recovery_location((root/"Folder").string().c_str(),location,sizeof(location))&&fs::path(location)==bin);
 assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);
 std::cout<<"PASS: permanent emptying confined to the selected bin; snapshot confirmation, recursive nested recovery, cancellation, changed files and link rejection\n";
}
