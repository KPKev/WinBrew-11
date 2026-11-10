#include "../src/explorer_fileops.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs=std::filesystem;
using namespace ww::fileops;

int main(){
  const auto root=fs::temp_directory_path()/("winbrew-walk-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto known=root/"Users/Default/Desktop/.winbrew-trash";
  const auto other=root/"Elsewhere/Deep/.winbrew-trash";
  fs::create_directories(known);fs::create_directories(other/"box");
  {std::ofstream f(other/"box/old.txt");f<<"preserve";}
  fs::create_directories(root/"Large");
  for(int i=0;i<4100;++i){std::ofstream f(root/"Large"/("file-"+std::to_string(i)));f<<'x';}
  RecoveryWalker fast;assert(fast.begin({(root/"Large").string()},known.string().c_str()));
  fast.step(1);assert(fast.names.empty()&&fast.directories==1);fast.step(1);assert(fast.complete&&!fast.failed);
#ifndef _WIN32
  fs::create_directory_symlink(root,root/"Elsewhere/link-to-root");
  {std::ofstream f(root/"Elsewhere/private.dat");f<<"private";}
  assert(chmod((root/"Elsewhere/private.dat").c_str(),0)==0);
  assert(mkfifo((root/"Elsewhere/pipe").c_str(),0600)==0);
#endif
  RecoveryWalker walker;assert(walker.begin({root.string(),(root/"Users").string()},known.string().c_str()));
  walker.step(1);assert(walker.active&&!walker.complete);
  for(int i=0;walker.active&&i<20000;++i)walker.step(16);
  assert(walker.complete&&!walker.failed);
  assert(walker.stores.size()==1&&walker.stores.front()==other.string());
  assert(fs::exists(other/"box/old.txt")&&fs::exists(known));
  assert(walker.begin({root.string()},known.string().c_str()));walker.step(1);walker.cancel();
  assert(!walker.active&&!walker.complete&&walker.stores.empty()&&fs::exists(other/"box/old.txt"));
  assert(walker.begin({root.string()},known.string().c_str()));walker.step(1);
  {std::ofstream f(root/"changed-during-scan");f<<"new";}
  for(int i=0;walker.active&&i<20000;++i)walker.step(64);
  assert(walker.failed&&!walker.complete&&fs::exists(other/"box/old.txt"));
  fs::remove(root/"changed-during-scan");
  {std::ofstream f(root/"Large/.winbrew-trash");f<<"unexpected";}
  assert(walker.begin({root.string()},known.string().c_str()));
  for(int i=0;walker.active&&i<20000;++i)walker.step(64);
  assert(walker.failed&&!walker.complete&&walker.stores.empty()&&fs::exists(other/"box/old.txt"));
  assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
  std::cout<<"PASS: incremental legacy discovery, large directories, cancellation and unsafe-store refusal\n";
}
