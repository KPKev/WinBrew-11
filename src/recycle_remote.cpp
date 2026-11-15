// SPDX-License-Identifier: GPL-3.0-or-later
#include "recycle_remote.hpp"
#if defined(__PROSPERO__) && !defined(RECYCLE_HELPER_PROCESS)
#include "recycle_helper_protocol.hpp"
#include "explorer_fileops.hpp"
#include "runtime/artifact_hash.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
extern "C" const unsigned char winbrew_recycle_helper[];
extern "C" const std::size_t winbrew_recycle_helper_size;
extern "C" const char winbrew_recycle_helper_sha256[];
namespace ww::fileops {
namespace {
struct Session {void *owner=nullptr;int peer=-1;std::string nonce,buffer;std::size_t total=0,progress=0;bool confirmed=false;};
Session &session(){static Session *value=new Session;return *value;}
void close_fd(int &fd){if(fd>=0)::close(fd);fd=-1;}
void close_session(void *owner){auto&s=session();if(s.owner!=owner)return;close_fd(s.peer);s.owner=nullptr;s.nonce.clear();s.buffer.clear();s.total=s.progress=0;s.confirmed=false;}
bool send_all(int fd,const char *data,std::size_t count){std::size_t sent=0;while(sent<count){const auto n=send(fd,data+sent,count-sent,MSG_NOSIGNAL);if(n<0&&errno==EINTR)continue;if(n<=0)return false;sent+=std::size_t(n);}return true;}
bool nonce(std::string &value){unsigned char bytes[16];int fd=::open("/dev/urandom",O_RDONLY);if(fd<0)return false;std::size_t done=0;while(done<sizeof(bytes)){const auto n=::read(fd,bytes+done,sizeof(bytes)-done);if(n<=0){::close(fd);return false;}done+=std::size_t(n);}::close(fd);static const char h[]="0123456789abcdef";for(auto b:bytes){value.push_back(h[b>>4]);value.push_back(h[b&15]);}return true;}
bool readable(int fd,int seconds){if(fd<0||fd>=FD_SETSIZE)return false;fd_set set;FD_ZERO(&set);FD_SET(fd,&set);timeval tv{seconds,0};return select(fd+1,&set,nullptr,nullptr,&tv)==1;}
bool line_read(int fd,std::string &value){while(value.size()<512){if(!readable(fd,30))return false;char c=0;const auto n=recv(fd,&c,1,0);if(n<=0)return false;if(c=='\n')return true;value.push_back(c);}return false;}
bool launch(void *owner,recycle_helper::Request &request,bool &empty){auto&s=session();empty=false;if(s.owner)return false;
 int listener=socket(AF_INET,SOCK_STREAM,0),loader=-1,peer=-1;if(listener<0)return false;
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=0;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||listen(listener,1)!=0){close_fd(listener);return false;}
 socklen_t length=sizeof(address);if(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)!=0){close_fd(listener);return false;}
 request.port=ntohs(address.sin_port);if(!nonce(request.nonce)){close_fd(listener);return false;}const auto encoded=recycle_helper::encode(request);
 std::string elf(reinterpret_cast<const char*>(winbrew_recycle_helper),winbrew_recycle_helper_size);const auto hash=playden::runtime::HashArtifactBytes(elf);const std::string marker="WB_RECYCLE_HELPER_REQUEST_SLOT_1";const auto pos=elf.find(marker);
 if(encoded.empty()||!hash||*hash!=winbrew_recycle_helper_sha256||pos==std::string::npos||elf.find(marker,pos+1)!=std::string::npos||pos+recycle_helper::SlotSize>elf.size()||elf.compare(pos+marker.size(),recycle_helper::SlotSize-marker.size(),std::string(recycle_helper::SlotSize-marker.size(),'\0'))!=0){close_fd(listener);return false;}
 elf.replace(pos,recycle_helper::SlotSize,encoded+std::string(recycle_helper::SlotSize-encoded.size(),'\0'));loader=socket(AF_INET,SOCK_STREAM,0);timeval timeout{10,0};if(loader>=0)setsockopt(loader,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(9021);target.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(loader<0||connect(loader,reinterpret_cast<sockaddr*>(&target),sizeof(target))!=0||!send_all(loader,elf.data(),elf.size())){close_fd(loader);close_fd(listener);return false;}
 shutdown(loader,SHUT_WR);close_fd(loader);if(!readable(listener,30)){close_fd(listener);return false;}peer=accept(listener,nullptr,nullptr);close_fd(listener);if(peer<0)return false;
 std::string line;if(!line_read(peer,line)){close_fd(peer);return false;}const std::string ready="READY "+request.nonce+" ",none="EMPTY "+request.nonce;if(line==none){close_fd(peer);empty=true;return true;}
 if(line.compare(0,ready.size(),ready)!=0){close_fd(peer);return false;}std::size_t total=0;const auto count=line.substr(ready.size());const auto parsed=std::from_chars(count.data(),count.data()+count.size(),total);if(parsed.ec!=std::errc{}||parsed.ptr!=count.data()+count.size()||!total){close_fd(peer);return false;}
 s.owner=owner;s.peer=peer;s.nonce=request.nonce;s.total=total;s.confirmed=false;return true;
}
bool confirm(void *owner){auto&s=session();if(s.owner!=owner||s.peer<0||!send_all(s.peer,"Y\n",2)){close_session(owner);return false;}s.confirmed=true;return true;}
// 0: waiting, 1: done, -1: stopped or invalid response.
int poll(void *owner){auto&s=session();if(s.owner!=owner||s.peer<0||!s.confirmed)return -1;char bytes[512];const auto n=recv(s.peer,bytes,sizeof(bytes),MSG_DONTWAIT);if(n<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1;if(n==0)return -1;s.buffer.append(bytes,std::size_t(n));if(s.buffer.size()>8192)return -1;
 for(;;){const auto at=s.buffer.find('\n');if(at==std::string::npos)break;const auto line=s.buffer.substr(0,at);s.buffer.erase(0,at+1);const auto progress="PROGRESS "+s.nonce+" ";if(line.compare(0,progress.size(),progress)==0){const auto number=line.substr(progress.size());std::size_t value=0;const auto r=std::from_chars(number.data(),number.data()+number.size(),value);if(r.ec!=std::errc{}||r.ptr!=number.data()+number.size()||value<s.progress||value>s.total)return -1;s.progress=value;}else if(line=="DONE "+s.nonce)return 1;else return -1;}return 0;
}
void cancel(void *owner){auto&s=session();if(s.owner==owner&&s.peer>=0&&!s.confirmed)send_all(s.peer,"N\n",2);close_session(owner);}
bool prefix(const std::string &path,const std::string &root,std::string &relative){const auto p=root+"/";if(path.compare(0,p.size(),p)!=0)return false;relative=path.substr(p.size());return true;}
}
void remote_purge_close(Purge&p){close_session(&p);}
void remote_purge_cancel(Purge&p){cancel(&p);p.ready=false;p.state=State::cancelled;}
bool remote_purge_prepare(Purge&p,const char*path,const char*relative){remote_purge_close(p);p.entries.clear();p.root=path?path:"";p.selected=relative?relative:"";p.remaining=p.removed=0;p.bytes=0;p.error=0;p.ready=false;p.state=State::idle;
 if(p.root!=recycle_helper::Store || (relative&&!recycle_helper::valid_relative(relative))){p.error=EINVAL;p.state=State::failed;return false;}recycle_helper::Request req;req.action=relative?recycle_helper::Action::item:recycle_helper::Action::empty;if(relative)req.relative=relative;bool empty=false;if(!launch(&p,req,empty)){p.error=EIO;p.state=State::failed;return false;}if(empty){p.state=State::complete;return true;}p.remaining=session().total;p.ready=true;return true;}
bool remote_purge_begin(Purge&p){if(!p.ready||!confirm(&p)){p.error=EIO;p.state=State::failed;return false;}p.ready=false;p.state=State::copying;return true;}
void remote_purge_step(Purge&p){const int result=poll(&p);if(result==0){p.removed=session().progress;p.remaining=session().total-p.removed;return;}close_session(&p);if(result>0){p.remaining=0;p.state=State::complete;}else{p.error=EAGAIN;p.state=State::failed;}}
void remote_transfer_close(CrossVolumeRecycle&x){close_session(&x);}
void remote_transfer_cancel(CrossVolumeRecycle&x){cancel(&x);x.state=State::cancelled;}
bool remote_transfer_begin(CrossVolumeRecycle&x,const Clipboard&clip,const char*destination,bool restoring,bool nested){if(x.busy()||!clip.ready||!destination)return false;remote_transfer_close(x);x.state=State::idle;x.error=0;
 recycle_helper::Request req;const std::string source=clip.path,profile=recycle_helper::Profile,store=recycle_helper::Store,private_root=recycle_helper::Private;
 if(restoring){req.action=recycle_helper::Action::restore;if(std::string(destination).compare(0,profile.size()+1,profile+"/")!=0||!prefix(source,store,req.relative)||!prefix(destination,profile,req.destination))return x.fail(EINVAL);}
 else if(nested || source.compare(0,store.size()+16,store+"/.winbrew-trash/")==0){req.action=recycle_helper::Action::move_nested;if(!prefix(source,store,req.relative)||std::string(destination)!=store)return x.fail(EINVAL);}
 else if(prefix(source,private_root,req.relative)){req.action=recycle_helper::Action::move_private;if(std::string(destination)!=store)return x.fail(EINVAL);}
 else if(prefix(source,"/download0",req.relative)){req.action=recycle_helper::Action::move_private;if(std::string(destination)!=store)return x.fail(EINVAL);}
 else if(prefix(source,profile,req.relative)){req.action=recycle_helper::Action::move_profile;if(std::string(destination)!=store)return x.fail(EINVAL);}else return x.fail(EINVAL);
 const auto&i=clip.info;req.expected={i.device,i.inode,i.size,i.modified,i.modified_ns,i.changed,i.changed_ns,i.directory};bool empty=false;
 if(!launch(&x,req,empty)||empty||!confirm(&x)){close_session(&x);std::snprintf(x.transfer.stage,sizeof(x.transfer.stage),"Recycle helper");return x.fail(EIO);}x.state=State::copying;return true;}
void remote_transfer_step(CrossVolumeRecycle&x){const int result=poll(&x);if(!result)return;close_session(&x);if(result>0)x.state=State::complete;else{x.error=EIO;x.state=State::failed;}}
}
#endif
