#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <cstdarg>
#include <iostream>
static bool fail_bin_openat=false;
extern "C" int __real_openat(int,const char *,int,...);
extern "C" int __wrap_openat(int fd,const char *path,int flags,...){if(fail_bin_openat && !std::strcmp(path,".winbrew-trash")){errno=EINVAL;return -1;}if(flags&O_CREAT){va_list ap;va_start(ap,flags);const int mode=va_arg(ap,int);va_end(ap);return __real_openat(fd,path,flags,mode);}return __real_openat(fd,path,flags);}
extern "C" int __real_open(const char *,int,...);
extern "C" int __wrap_open(const char *path,int flags,...){
 if(!std::strcmp(path,"/")){errno=EINVAL;return -1;}
 if(flags&O_CREAT){va_list ap;va_start(ap,flags);const int mode=va_arg(ap,int);va_end(ap);return __real_open(path,flags,mode);}return __real_open(path,flags);
}
int main(){namespace fs=std::filesystem;const auto base=fs::temp_directory_path()/("wb-anchor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));const auto bin=base/".winbrew-trash";fs::create_directories(bin);std::ofstream(bin/"item.txt")<<"keep";
 ww::fileops::Purge p;assert(p.prepare(bin.string().c_str())&&p.ready);p.cancel();assert(fs::exists(bin/"item.txt"));std::ofstream(base/"source.txt")<<"keep";ww::fileops::Clipboard clip;assert(clip.copy((base/"source.txt").string().c_str()));ww::fileops::CrossVolumeRecycle x;fail_bin_openat=true;assert(!x.begin(clip,bin.string().c_str()));fail_bin_openat=false;assert(std::string(x.transfer.stage).find("at .winbrew-")==0 && std::string(x.transfer.stage).find("d0 22")!=std::string::npos);assert(fs::exists(base/"source.txt"));fs::create_directory_symlink(base,base.parent_path()/(base.filename().string()+"-link"));ww::fileops::Purge blocked;const auto link=base.parent_path()/(base.filename().string()+"-link");assert(!blocked.prepare((link/".winbrew-trash").string().c_str()));assert(fs::exists(bin/"item.txt"));fs::remove(link);assert(base.parent_path()==fs::temp_directory_path());fs::remove_all(base);std::cout<<"PASS: protected bin opens when opening filesystem root is refused; cancellation keeps contents\n";}
