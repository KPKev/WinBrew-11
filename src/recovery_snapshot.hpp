// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace ww::fileops {
// A checked, reverse-traversable snapshot. It does not create a temporary
// file and can report allocation failure before any user data is removed.
template<class Metadata> class RecoverySnapshot {
  static_assert(std::is_trivially_copyable_v<Metadata>);
public:
  struct Entry { Entry *previous; Metadata info; size_t length; char *path; };
  using Allocate = void *(*)(size_t);
  // A supplied allocator must return memory compatible with std::free.
  explicit RecoverySnapshot(Allocate allocator=std::malloc):allocate_(allocator) {}
  RecoverySnapshot(const RecoverySnapshot &)=delete;
  RecoverySnapshot &operator=(const RecoverySnapshot &)=delete;
  ~RecoverySnapshot(){clear();}
  bool append(const char *relative,const Metadata &info) noexcept {
    if(!relative || !*relative || *relative=='/' || !allocate_)return false;
    const size_t length=std::strlen(relative);
    if(length>=1024 || relative[length-1]=='/' || length>std::numeric_limits<size_t>::max()-sizeof(Entry)-1)return false;
    size_t begin=0;
    for(size_t pos=0;pos<=length;++pos){
      if(pos<length && relative[pos]=='\\')return false;
      if(pos==length || relative[pos]=='/'){
        const size_t part=pos-begin;
        if(!part || (part==1 && relative[begin]=='.') || (part==2 && relative[begin]=='.' && relative[begin+1]=='.'))return false;
        begin=pos+1;
      }
    }
    void *memory=allocate_(sizeof(Entry)+length+1);
    if(!memory)return false;
    auto *entry=::new(memory) Entry{};
    entry->previous=tail_;entry->info=info;entry->length=length;entry->path=reinterpret_cast<char *>(entry+1);
    std::memcpy(entry->path,relative,length+1);tail_=entry;++count_;return true;
  }
  const Entry *last() const noexcept{return tail_;}
  size_t size() const noexcept{return count_;}
  void pop_back() noexcept {
    if(!tail_)return;auto *previous=tail_->previous;tail_->~Entry();std::free(tail_);tail_=previous;--count_;
  }
  void clear() noexcept {
    while(tail_){auto *previous=tail_->previous;tail_->~Entry();std::free(tail_);tail_=previous;}
    count_=0;
  }
private:
  Allocate allocate_;
  Entry *tail_=nullptr;
  size_t count_=0;
};
} // namespace ww::fileops
