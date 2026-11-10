#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
// Keep the 66,000-item fixture fast; other purge tests exercise real fsync.
extern "C" int __wrap_fsync(int){return 0;}
int main(){
 const auto root=fs::temp_directory_path()/("winbrew-huge-purge-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto bin=root/".winbrew-trash";fs::create_directories(bin);
 {std::ofstream f(root/"outside.txt");f<<"keep";}
 constexpr int count=66000;
 for(int i=0;i<count;++i){std::ofstream f(bin/("item-"+std::to_string(i)));assert(f.good());}
 ww::fileops::Purge job;assert(job.prepare(bin.string().c_str())&&job.ready&&job.remaining==count);
 job.cancel();assert(fs::exists(bin/"item-0")&&fs::exists(bin/"item-65999"));
 assert(std::distance(fs::directory_iterator(bin),fs::directory_iterator{})==count);
 assert(job.prepare_item(bin.string().c_str(),(bin/"item-65999").string().c_str())&&job.begin());
 while(job.busy())job.step();assert(!fs::exists(bin/"item-65999")&&fs::exists(bin/"item-0"));
 assert(job.prepare(bin.string().c_str())&&job.remaining==count-1&&job.begin());
 for(int i=0;job.busy()&&i<count+1;++i)job.step();
 assert(job.state==ww::fileops::State::complete&&job.removed==count-1&&fs::is_empty(bin)&&fs::exists(root/"outside.txt"));
 assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
 std::cout<<"PASS: confirmed emptying beyond 65536 items without deleting outside files\n";
}
