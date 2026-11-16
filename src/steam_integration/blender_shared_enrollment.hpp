// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cerrno>
#include <string>
#include <unistd.h>
namespace ww::steam_integration {
inline bool BlenderSharedTitleId(const std::string& title) {
  return title.size()==9 && title.rfind("FAKE78",0)==0 &&
         title.find_first_not_of("0123456789",6)==std::string::npos;
}
// Caller holds the application's effective-UID gate and executes as the owner.
// A separate list grants shared-folder access only to the prepared Blender title.
inline bool WriteBlenderSharedEnrollment(const std::filesystem::path& data,const std::string& title) {
  using namespace playden::platform;
  if(!title.empty()&&!BlenderSharedTitleId(title))return false;
  const std::string wanted=title.empty()?std::string{}:title+"\n";
  auto root=ManagedDirectory::Open(data,DirectoryPolicy::kApprovedTree);
  if(!root)return false;
  auto wine=root->OpenChild("wine11",DirectoryPolicy::kApprovedTree,false);
  if(!wine)return false;
  auto rules=wine->OpenChild("title-rule",DirectoryPolicy::kPrivate,false);
  if(!rules)return false;
  auto shared=rules->OpenChild("blender-shared",DirectoryPolicy::kPrivate,true);
  if(!shared||!shared->Sync()||!rules->Sync())return false;
  int fd=shared->CreateExclusiveFile("enrollment.lock",0600);
  if(fd>=0){bool ok=!fsync(fd);if(close(fd))ok=false;if(!ok||!shared->Sync())return false;}
  else if(errno!=EEXIST)return false;
  auto lease=ManagedFileLock::Acquire(*shared,"enrollment.lock",LockMode::kPublisher);
  if(!lease)return false;
  const auto read=[&](const char* name,std::string& bytes) {
    int f=shared->OpenRegularFile(name,true,0600);if(f<0)return false;
    struct stat st{};char b[12];ssize_t n=::read(f,b,sizeof b);
    bool ok=!fstat(f,&st)&&st.st_nlink==1&&n>=0&&n<sizeof b&&st.st_size==n;
    if(close(f))ok=false;if(ok)bytes.assign(b,n);else errno=EIO;return ok;
  };
  std::string existing;
  if(read("allow-list",existing)){if(existing==wanted)return true;}
  else if(errno!=ENOENT)return false;
  struct stat stale{};
  if(shared->InspectChild(".allow-list.new",&stale)){
    std::string ignored;if(!read(".allow-list.new",ignored)||!shared->RemoveRegularFile(".allow-list.new"))return false;
  }else if(errno!=ENOENT)return false;
  fd=shared->CreateExclusiveFile(".allow-list.new",0600);if(fd<0)return false;
  bool ok=true;size_t at=0;
  while(at<wanted.size()){ssize_t n=write(fd,wanted.data()+at,wanted.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=n;}
  if(fsync(fd))ok=false;
  if(close(fd))ok=false;
  if(!ok||!lease->Validate(*shared,"enrollment.lock")||!shared->RenameChild(".allow-list.new",*shared,"allow-list",true)||!shared->Sync())return false;
  return read("allow-list",existing)&&existing==wanted;
}
}
