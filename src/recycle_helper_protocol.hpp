// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <utility>
namespace ww::recycle_helper {
inline constexpr char Store[]="/data/WinBrew/Users/Default/Desktop/.winbrew-trash";
inline constexpr char Profile[]="/data/WinBrew/Users/Default";
inline constexpr char Private[]="/mnt/sandbox/PPSA77382_000/download0";
inline constexpr std::size_t SlotSize=4096;
enum class Action { empty, item, move_private, move_profile, move_nested, restore };
struct Expected {std::uint64_t device=0,inode=0,size=0;std::int64_t modified=0,modified_ns=0,changed=0,changed_ns=0;bool directory=false;};
struct Request {Action action=Action::empty;std::uint16_t port=0;std::string nonce,relative,destination;Expected expected;};
inline bool valid_relative(std::string_view path){if(path.empty()||path.size()>1023||path.front()=='/'||path.back()=='/')return false;std::size_t begin=0;for(std::size_t i=0;i<=path.size();++i){if(i<path.size()&&(path[i]=='\\'||path[i]=='\0'))return false;if(i==path.size()||path[i]=='/'){auto part=path.substr(begin,i-begin);if(part.empty()||part=="."||part=="..")return false;begin=i+1;}}return true;}
inline bool valid_nonce(std::string_view n){if(n.size()!=32)return false;for(char c:n)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline bool in_trash(std::string_view p){return p.find("/.winbrew-trash/")!=p.npos;}
inline bool private_folder(std::string_view p){auto at=p.find('/');if(at==p.npos)return false;auto first=p.substr(0,at);return first=="Desktop"||first=="Downloads"||first=="Documents"||first=="Pictures"||first=="Music"||first=="Videos";}
inline bool valid_request(const Request&r){
 if(!r.port||!valid_nonce(r.nonce))return false;
 if(r.action==Action::empty)return r.relative.empty()&&r.destination.empty();
 if(!valid_relative(r.relative))return false;
 if(r.action==Action::item)return r.destination.empty();
 if(!r.expected.device||!r.expected.inode)return false;
 if(r.action==Action::move_private)return r.destination.empty()&&private_folder(r.relative)&&in_trash(r.relative);
 if(r.action==Action::move_profile)return r.destination.empty()&&in_trash(r.relative)&&r.relative.rfind("Desktop/.winbrew-trash/",0)!=0;
 if(r.action==Action::move_nested)return r.destination.empty()&&r.relative.rfind(".winbrew-trash/",0)==0;
 if(r.action==Action::restore)return valid_relative(r.destination)&&!in_trash("/"+r.destination+"/");
 return false;
}
inline std::string hex_encode(std::string_view p){static constexpr char h[]="0123456789abcdef";std::string o;for(unsigned char c:p){o.push_back(h[c>>4]);o.push_back(h[c&15]);}return o;}
inline bool hex_decode(std::string_view p,std::string&o){if(p.size()%2)return false;auto n=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(std::size_t i=0;i<p.size();i+=2){int a=n(p[i]),b=n(p[i+1]);if(a<0||b<0)return false;o.push_back(char((a<<4)|b));}return true;}
inline char action_code(Action a){switch(a){case Action::empty:return 'E';case Action::item:return 'I';case Action::move_private:return 'L';case Action::move_profile:return 'P';case Action::move_nested:return 'N';case Action::restore:return 'R';}return '?';}
inline std::string encode(const Request&r){if(!valid_request(r))return {};const auto&e=r.expected;std::string out="WBR2\n"+std::to_string(r.port)+"\n"+r.nonce+"\n"+action_code(r.action)+"\n"+hex_encode(r.relative)+"\n"+hex_encode(r.destination)+"\n";
 if(r.action==Action::empty||r.action==Action::item)out+="-\n";else out+=std::to_string(e.device)+","+std::to_string(e.inode)+","+std::to_string(e.size)+","+std::to_string(e.modified)+","+std::to_string(e.modified_ns)+","+std::to_string(e.changed)+","+std::to_string(e.changed_ns)+","+(e.directory?"1":"0")+"\n";return out.size()<SlotSize?out:std::string();}
inline bool decode(std::string_view input,Request&out){std::array<std::string_view,7> f{};for(auto &v:f){auto n=input.find('\n');if(n==input.npos)return false;v=input.substr(0,n);input.remove_prefix(n+1);}if(!input.empty()||f[0]!="WBR2"||f[3].size()!=1)return false;unsigned port=0;auto p=std::from_chars(f[1].data(),f[1].data()+f[1].size(),port);if(p.ec!=std::errc{}||p.ptr!=f[1].data()+f[1].size()||port>65535)return false;Request r;r.port=std::uint16_t(port);r.nonce=std::string(f[2]);switch(f[3][0]){case 'E':r.action=Action::empty;break;case 'I':r.action=Action::item;break;case 'L':r.action=Action::move_private;break;case 'P':r.action=Action::move_profile;break;case 'N':r.action=Action::move_nested;break;case 'R':r.action=Action::restore;break;default:return false;}if(!hex_decode(f[4],r.relative)||!hex_decode(f[5],r.destination))return false;
 if(r.action==Action::empty||r.action==Action::item){if(f[6]!="-")return false;}else{std::array<std::string_view,8> parts{};auto rest=f[6];for(std::size_t i=0;i<parts.size();++i){auto n=rest.find(',');if((i+1==parts.size())!=(n==rest.npos))return false;parts[i]=rest.substr(0,n);if(n!=rest.npos)rest.remove_prefix(n+1);}auto number=[](std::string_view v,auto&x){auto q=std::from_chars(v.data(),v.data()+v.size(),x);return q.ec==std::errc{}&&q.ptr==v.data()+v.size();};unsigned dir=0;auto&e=r.expected;if(!number(parts[0],e.device)||!number(parts[1],e.inode)||!number(parts[2],e.size)||!number(parts[3],e.modified)||!number(parts[4],e.modified_ns)||!number(parts[5],e.changed)||!number(parts[6],e.changed_ns)||!number(parts[7],dir)||dir>1)return false;e.directory=dir==1;}
 if(encode(r)!=std::string("WBR2\n")+std::string(f[1])+"\n"+std::string(f[2])+"\n"+std::string(f[3])+"\n"+std::string(f[4])+"\n"+std::string(f[5])+"\n"+std::string(f[6])+"\n")return false;out=std::move(r);return true;}
}
