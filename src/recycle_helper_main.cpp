// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated payload: its only removable root is the compile-time Recycle Bin.
#include "recycle_helper_protocol.hpp"
#include "explorer_fileops.hpp"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
extern "C" { volatile char winbrew_recycle_request_slot[ww::recycle_helper::SlotSize]="WB_RECYCLE_HELPER_REQUEST_SLOT_1"; }
namespace {
bool send_line(int fd,const std::string &s){std::size_t n=0;while(n<s.size()){const auto w=send(fd,s.data()+n,s.size()-n,MSG_NOSIGNAL);if(w<=0)return false;n+=std::size_t(w);}return true;}
bool receive_choice(int fd,char &choice){char data[2]{};std::size_t n=0;while(n<2){const auto got=recv(fd,data+n,2-n,0);if(got<=0)return false;n+=std::size_t(got);}if(data[1]!='\n' || (data[0]!='Y'&&data[0]!='N'))return false;choice=data[0];return true;}
}
int main(){
 using namespace ww;using namespace ww::recycle_helper;
 std::string encoded;for(std::size_t i=0;i<SlotSize&&winbrew_recycle_request_slot[i];++i)encoded.push_back(winbrew_recycle_request_slot[i]);
 Request request;if(!decode(encoded,request))return 1;
 if(seteuid(0)!=0)return 1;
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 1;timeval limit{300,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&limit,sizeof(limit));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&limit,sizeof(limit));
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(request.port);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){close(fd);return 1;}
#ifdef RECYCLE_HELPER_TEST_ROOT
 const std::string root=RECYCLE_HELPER_TEST_ROOT;
#else
 const std::string root=Store;
#endif
 if(request.action!=Action::empty && request.action!=Action::item){
#ifdef RECYCLE_HELPER_TEST_ROOT
  const std::string store=root,profile=root.substr(0,root.size()-15),private_root=profile+"/private";
#else
  const std::string profile=Profile,store=Store,private_root=Private;
#endif
  std::string source,destination;
  if(request.action==Action::move_private)source=private_root+"/"+request.relative;
  else if(request.action==Action::move_profile)source=profile+"/"+request.relative;
  else source=store+"/"+request.relative;
  if(request.action==Action::restore)destination=profile+"/"+request.destination;
  fileops::Info expected{};const auto &e=request.expected;expected.device=e.device;expected.inode=e.inode;expected.size=e.size;expected.modified=e.modified;expected.modified_ns=long(e.modified_ns);expected.changed=e.changed;expected.changed_ns=long(e.changed_ns);expected.directory=e.directory;expected.regular=!e.directory;
  fileops::Clipboard clip;if(!clip.copy(source.c_str()) || !fileops::unchanged(clip.info,expected)){send_line(fd,"ERROR "+request.nonce+" changed\n");close(fd);return 1;}
  std::string wrapper,marker;fileops::Info wrapper_info{},marker_info{};
  if(request.action==Action::restore){const auto split=source.find_last_of('/');const auto parent=source.substr(0,split),name=source.substr(split+1);std::string child,original;fileops::Info info{};
    if(parent!=store && parent.compare(0,store.size()+1,store+"/")==0 && parent.find('/',store.size()+1)==std::string::npos && fileops::recovery_container_item(parent.c_str(),child,info) && child==name && fileops::unchanged(info,clip.info)){
      wrapper=parent;if(fileops::inspect(wrapper.c_str(),wrapper_info)<0){send_line(fd,"ERROR "+request.nonce+" wrapper\n");close(fd);return 1;}
      if(fileops::recovery_original_path(parent.c_str(),name.c_str(),original)){marker=parent+(name==".winbrew-original-location"?"/.winbrew-original-location-1":"/.winbrew-original-location");if(fileops::inspect(marker.c_str(),marker_info)<0 || !marker_info.regular){send_line(fd,"ERROR "+request.nonce+" marker\n");close(fd);return 1;}}
    }
  }
  fileops::CrossVolumeRecycle transfer;const bool started=request.action==Action::restore?transfer.begin_restore(clip,destination.c_str(),store.c_str(),profile.c_str()):request.action==Action::move_nested?transfer.begin_nested_recovery(clip,store.c_str()):transfer.begin(clip,store.c_str());
  if(!started){send_line(fd,"ERROR "+request.nonce+" start\n");close(fd);return 1;}
  if(!send_line(fd,"READY "+request.nonce+" 1\n")){transfer.cancel();close(fd);return 1;}
  char choice=0;if(!receive_choice(fd,choice)||choice!='Y'){transfer.cancel();close(fd);return 0;}
  while(transfer.busy()){transfer.step();if(!send_line(fd,"PROGRESS "+request.nonce+" 0\n")){transfer.cancel();close(fd);return 1;}}
  if(transfer.state!=fileops::State::complete){send_line(fd,"ERROR "+request.nonce+" transfer\n");close(fd);return 1;}
  if(request.action==Action::restore && !wrapper.empty()){
    fileops::Purge cleanup;if(!cleanup.prepare_item(store.c_str(),wrapper.c_str())){send_line(fd,"ERROR "+request.nonce+" cleanup\n");close(fd);return 1;}
    const auto relative=wrapper.substr(store.size()+1);std::size_t checked=0;bool safe=true;
    for(auto *entry=cleanup.entries.last();entry;entry=entry->previous){const std::string path=entry->path;++checked;if(path==relative){if(!fileops::identity(entry->info,wrapper_info))safe=false;}else if(!marker.empty()&&path==marker.substr(store.size()+1)){if(!fileops::unchanged(entry->info,marker_info))safe=false;}else safe=false;}
    if(!safe || checked!=(marker.empty()?1u:2u) || !cleanup.begin()){cleanup.cancel();send_line(fd,"ERROR "+request.nonce+" cleanup\n");close(fd);return 1;}
    while(cleanup.busy())cleanup.step();if(cleanup.state!=fileops::State::complete){send_line(fd,"ERROR "+request.nonce+" cleanup\n");close(fd);return 1;}
  }else if(request.action!=Action::restore){
    const std::string oldstore=request.action==Action::move_nested?store+"/.winbrew-trash":source.substr(0,source.find("/.winbrew-trash/")+15);
    if(!fileops::remove_empty_legacy_wrapper(oldstore.c_str(),source.c_str())){send_line(fd,"ERROR "+request.nonce+" cleanup\n");close(fd);return 1;}
    if(request.action==Action::move_nested)fileops::remove_empty_nested_store(store.c_str());
  }
  send_line(fd,"DONE "+request.nonce+"\n");close(fd);return 0;
 }
 fileops::Purge purge;const bool prepared=request.action==Action::empty?purge.prepare(root.c_str()):purge.prepare_item(root.c_str(),(root+"/"+request.relative).c_str());
 if(!prepared){send_line(fd,"ERROR "+request.nonce+" prepare\n");close(fd);return 1;}
 if(!purge.ready){send_line(fd,"EMPTY "+request.nonce+"\n");close(fd);return 0;}
 if(!send_line(fd,"READY "+request.nonce+" "+std::to_string(purge.remaining)+"\n")){purge.cancel();close(fd);return 1;}
 char choice=0;if(!receive_choice(fd,choice)||choice!='Y'){purge.cancel();close(fd);return 0;}
 if(!purge.begin()){send_line(fd,"ERROR "+request.nonce+" changed\n");close(fd);return 1;}
 while(purge.busy()){purge.step();if(!send_line(fd,"PROGRESS "+request.nonce+" "+std::to_string(purge.removed)+"\n")){purge.cancel();close(fd);return 1;}}
 const bool ok=purge.state==fileops::State::complete;
 if(ok && request.action==Action::empty){fileops::Purge remaining;if(!remaining.prepare(root.c_str()) || remaining.ready){send_line(fd,"REMAINING "+request.nonce+"\n");close(fd);return 1;}}
 send_line(fd,std::string(ok?"DONE ":"ERROR ")+request.nonce+(ok?"\n":" stopped\n"));close(fd);return ok?0:1;
}
