#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
using namespace ww::fileops;

int main(){
  const auto root=fs::temp_directory_path()/("winbrew-recovery-batches-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto wide=root/"wide", dest=root/"Desktop/.winbrew-trash";
  fs::create_directories(wide);fs::create_directories(dest);
  for(int i=0;i<66000;++i)fs::create_directory(wide/("box-"+std::to_string(i)));
  fs::create_directory(wide/".winbrew-trash");
  RecoveryWalker walker;assert(walker.begin({wide.string()},dest.string().c_str()));
  while(walker.active)walker.step(10000);
  assert(walker.complete&&!walker.failed&&walker.stores.size()==1);
  fs::remove(wide/".winbrew-trash");
  const auto store=root/"old/.winbrew-trash";fs::create_directories(store.parent_path());fs::rename(wide,store);
  for(int i=0;i<260;++i){std::ofstream f(store/"box-0"/("item-"+std::to_string(i)));f<<i;}
  {std::ofstream f(store/"box-65999"/"last");f<<"last";}
  size_t moved=0;bool more=true;
  while(more){
    std::vector<Clipboard> batch;
    assert(collect_recovery_batch({store.string()},dest.string().c_str(),batch,more));
    assert(batch.size()<=256);
    for(const auto &item:batch){char target[1024];assert(trash(item.path,item.info,target,sizeof(target),dest.string().c_str()));++moved;}
  }
  assert(moved==261&&fs::is_empty(store/"box-0")&&fs::is_empty(store/"box-65999"));
  size_t count=0;for(const auto &entry:fs::directory_iterator(dest)){(void)entry;++count;}assert(count==moved);
  assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
  std::cout<<"PASS: discovery and batched legacy recovery beyond 65536 directories\n";
}
