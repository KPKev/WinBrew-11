#include "steam/backend/app_info.h"
#include <cassert>
#include <set>
#include <vector>
using namespace playden::steam;
int main(){AppInfo app;app.app_id=365670;DepotInfo x64; x64.id=365671;x64.os_list="windows";x64.os_arch="64";x64.manifest_gid=1;DepotInfo x86=x64;x86.id=365672;x86.os_arch="32";x86.manifest_gid=2;app.depots={x64,x86};std::vector<DepotInfo> out;std::string error;assert(SelectDepots(app,{365670},std::nullopt,&out,&error));assert(out.size()==1&&out[0].id==365671);assert(SelectDepots(app,{365670},std::set<std::uint32_t>{365672},&out,&error));assert(out.size()==1&&out[0].id==365672);app.app_id=42;assert(SelectDepots(app,{42},std::nullopt,&out,&error));assert(out.size()==2);x64.manifest_gid.reset();app.app_id=365670;app.depots={x64,x86};assert(SelectDepots(app,{365670},std::nullopt,&out,&error));assert(out.size()==1&&out[0].id==365672);}
