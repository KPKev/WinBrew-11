#include "../src/recovery_snapshot.hpp"
#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace ww::fileops;
static size_t remaining=0;
static void *limited(size_t size){if(!remaining)return nullptr;--remaining;return std::malloc(size);}
int main(){
  RecoverySnapshot<Info> snapshot;Info info{};info.regular=true;
  for(size_t i=0;i<70000;++i){char name[64];std::snprintf(name,sizeof(name),"folder/item-%zu",i);info.inode=i+1;assert(snapshot.append(name,info));}
  assert(snapshot.size()==70000);auto *entry=snapshot.last();
  for(size_t i=70000;i>0;--i){char expected[64];std::snprintf(expected,sizeof(expected),"folder/item-%zu",i-1);assert(entry&&entry->info.inode==i&&!std::strcmp(entry->path,expected));entry=entry->previous;}
  assert(!entry);assert(!snapshot.append("../outside",info));assert(!snapshot.append("dir/../outside",info));
  assert(!snapshot.append("/absolute",info));assert(!snapshot.append("dir//item",info));assert(!snapshot.append("dir\\item",info));
  assert(snapshot.size()==70000);snapshot.clear();assert(!snapshot.size()&&!snapshot.last());
  remaining=1;RecoverySnapshot<Info> bounded(limited);assert(bounded.append("first",info));assert(!bounded.append("second",info));
  assert(bounded.size()==1&&!std::strcmp(bounded.last()->path,"first"));
  std::cout<<"PASS: checked reverse snapshot beyond 65536 entries, safe paths and allocation failure\n";
}
