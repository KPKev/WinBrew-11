#include "../src/explorer_fileops.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>
#include <iostream>
#include <algorithm>
using namespace ww::fileops;
namespace fs=std::filesystem;
void put(const fs::path&p,const std::string&s){std::ofstream(p,std::ios::binary)<<s;}
std::string get(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void finish(TreeTransfer&t){for(int i=0;t.busy()&&i<10000;++i)t.step();assert(!t.busy());}
int main(){
 auto root=fs::absolute(fs::path("build")/("tree-ops-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
 fs::create_directories(root/"Source"/"Nested");fs::create_directories(root/"Destination");
 const std::string data(150000,'X');put(root/"Source"/"Nested"/"one.txt",data);put(root/"Source"/"empty.txt","");
 Clipboard clip;assert(clip.copy((root/"Source").string().c_str()));
 TreeTransfer t;assert(t.begin(clip,(root/"Destination").string().c_str()));finish(t);assert(t.state==State::complete);assert(get(root/"Destination"/"Source"/"Nested"/"one.txt")==data);
 assert(t.begin(clip,(root/"Destination").string().c_str()));finish(t);assert(t.state==State::complete);assert(fs::is_directory(root/"Destination"/"Source - Copy"));
 assert(!t.begin(clip,(root/"Source"/"Nested").string().c_str()));
 clip.move=true;assert(t.begin(clip,(root/"Destination").string().c_str()));t.step();t.cancel();assert(fs::exists(root/"Source"));
 assert(clip.copy((root/"Source").string().c_str()));clip.move=true;
 assert(t.begin(clip,(root/"Destination").string().c_str()));put(root/"Source"/"new.txt","changed during copy");finish(t);assert(t.state==State::failed && fs::exists(root/"Source"));
 assert(clip.copy((root/"Source").string().c_str()));clip.move=true;assert(t.begin(clip,(root/"Destination").string().c_str()));finish(t);assert(t.state==State::complete && !fs::exists(root/"Source"));assert(get(fs::path(t.destination)/"Nested"/"one.txt")==data);
 // Replacing a completed output before the move commits retains the source.
 fs::create_directories(root/"Race");put(root/"Race"/"one.txt",data);assert(clip.copy((root/"Race").string().c_str()));clip.move=true;
 assert(t.begin(clip,(root/"Destination").string().c_str()));
 for(int i=0;t.busy()&&t.index<t.entries.size()&&i<1000;++i)t.step();
 assert(t.busy());put(fs::path(t.destination)/"one.txt","modified destination");finish(t);
 assert(t.state==State::failed && fs::exists(root/"Race"));
 auto item=root/"delete.txt";put(item,"recover me");Info info;assert(inspect(item.string().c_str(),info)==0);char recovery[1024];assert(trash(item.string().c_str(),info,recovery,sizeof(recovery)));assert(!fs::exists(item)&&get(recovery)=="recover me");
 put(item,"replacement");assert(!trash(item.string().c_str(),info,recovery,sizeof(recovery)));assert(get(item)=="replacement");
 // An explicitly selected bin collects items from other folders on the same volume.
 fs::create_directories(root/"Desktop");fs::create_directories(root/"Documents");put(root/"Documents"/"note.txt","recover centrally");
 auto central=(root/"Desktop"/".winbrew-trash").string();Info note{};assert(inspect((root/"Documents"/"note.txt").string().c_str(),note)==0);
 assert(trash((root/"Documents"/"note.txt").string().c_str(),note,recovery,sizeof(recovery),central.c_str()));
 assert(get(recovery)=="recover centrally" && std::string(recovery).find(central+"/")==0 && !fs::exists(root/"Documents"/".winbrew-trash"));
 fs::create_directories(root/"Documents"/"Nested"/".winbrew-trash");put(root/"Documents"/"Nested"/".winbrew-trash"/"old.txt","legacy");
 std::vector<std::string> stores;assert(discover_recovery_stores(root.string().c_str(),stores));
 assert(std::find(stores.begin(),stores.end(),central)!=stores.end());
 assert(std::find(stores.begin(),stores.end(),(root/"Documents"/"Nested"/".winbrew-trash").string())!=stores.end());
 std::vector<Clipboard> pending;assert(collect_recovery_items(root.string().c_str(),central.c_str(),pending));
 bool found_old=false;for(const auto &entry:pending)if(fs::path(entry.path).filename()=="old.txt")found_old=true;assert(found_old&&get(root/"Documents"/"Nested"/".winbrew-trash"/"old.txt")=="legacy");
 const size_t old_count=pending.size();assert(!collect_recovery_items((root/"missing").string().c_str(),central.c_str(),pending)&&pending.size()==old_count);
 size_t migrated=0;assert(consolidate_recovery_stores(root.string().c_str(),central.c_str(),migrated));
 assert(migrated>=1 && !fs::exists(root/"Documents"/"Nested"/".winbrew-trash"/"old.txt"));
 bool preserved=false;for(const auto &e:fs::recursive_directory_iterator(central))if(e.path().filename()=="old.txt"&&get(e.path())=="legacy")preserved=true;assert(preserved);
#ifndef _WIN32
 fs::create_directory_symlink(root/"Destination",root/"linked");assert(!clip.copy((root/"linked").string().c_str()));
 fs::create_directory_symlink(root/"Destination",root/"Destination"/"loop");assert(clip.copy((root/"Destination").string().c_str()));assert(!t.begin(clip,root.string().c_str()));assert(fs::exists(item));
#endif
 // Only this freshly created fixture is removed.
 assert(root.parent_path()==fs::absolute("build"));fs::remove_all(root);
 std::cout<<"PASS: recursive verified copy, collision preservation, cut cancellation/change retention, move, recoverable delete and links\n";
}
