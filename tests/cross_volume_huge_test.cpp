#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
extern "C" int __wrap_fsync(int){return 0;}
static volatile bool fail_reallocation=false;
extern "C" void *__real_realloc(void *,size_t);
extern "C" void *__wrap_realloc(void *p,size_t n){return fail_reallocation?nullptr:__real_realloc(p,n);}
int main(){
  ww::fileops::CheckedTransferEntries checked;ww::fileops::Info info{};
  for(int i=0;i<64;++i)assert(checked.append("kept",info));
  fail_reallocation=true;const bool grew=checked.append("not added",info);fail_reallocation=false;
  assert(!grew&&checked.size()==64&&!std::strcmp(checked[0].relative,"kept"));
  const auto root=fs::temp_directory_path()/("winbrew-cross-huge-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto source=root/"source", bin=root/"target/.winbrew-trash";
  fs::create_directories(source);fs::create_directories(bin);
  constexpr int count=66000;
  for(int i=0;i<count;++i){std::ofstream f(source/("item-"+std::to_string(i)));assert(f.good());}
  ww::fileops::Clipboard clip;assert(clip.copy(source.string().c_str()));
  ww::fileops::CrossVolumeRecycle refused;
  fail_reallocation=true;const bool accepted=refused.begin(clip,bin.string().c_str());fail_reallocation=false;
  assert(!accepted&&fs::exists(source/"item-0")&&fs::is_empty(bin));
  ww::fileops::CrossVolumeRecycle op;assert(op.begin(clip,bin.string().c_str()));
  assert(op.transfer.entries.size()==count+1);
  for(size_t i=0;op.busy()&&i<1000000;++i)op.step();
  assert(op.state==ww::fileops::State::complete&&!fs::exists(source));
  assert(fs::exists(bin/"source/item-0")&&fs::exists(bin/"source/item-65999"));
  assert(std::distance(fs::directory_iterator(bin/"source"),fs::directory_iterator{})==count);
  assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);
  std::cout<<"PASS: verified recovery transfer of a folder containing 66000 files\n";
}
