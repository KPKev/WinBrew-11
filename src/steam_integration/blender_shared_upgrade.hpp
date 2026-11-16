// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "blender_shared_release_policy.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace ww::steam_integration {
template<class Artifact>bool CanMigrateBlenderSharedRelease(std::uint32_t app,const std::vector<Artifact>&old,const std::vector<Artifact>&wanted){
 if(app!=365670 || (old.size()!=2&&old.size()!=3) || wanted.size()!=3)return false;
 const auto one=[](const auto& v,const char* id)->const Artifact*{const Artifact*out=nullptr;for(const auto&a:v)if(a.artifact_id==id){if(out)return nullptr;out=&a;}return out;};
 const auto *orun=one(old,"runtime"),*orecipe=one(old,"recipe"),*nrun=one(wanted,"runtime"),*nrecipe=one(wanted,"recipe"),*nr=one(wanted,"compatibility-title");
 if(!orun||!orecipe||!nrun||!nrecipe||!nr||nrun->identity!="wine-wow64-blender-019.3"||nr->identity!="blender-019.3-title"||nr->sha256!=kBlenderSharedReleasePin||nrun->sha256.size()!=64||orecipe->identity!=nrecipe->identity||orecipe->sha256!=nrecipe->sha256)return false;
 if(old.size()==2)return orun->identity=="wine-wow64-031302d-kit-0.13"&&orun->sha256=="6f5d2365ad5553819a21a390edb57c739d3af99b2392b16141b7fc8c140a5adb";
 const auto*r=one(old,"compatibility-title");if(!r||orun->sha256!=nrun->sha256)return false;
 return (orun->identity=="wine-wow64-blender-019.3"&&r->identity=="blender-019.3-title"&&r->sha256=="d0bcb55c66fbfcc84531b99ebceb1f2847532c0b5580b7e9c5ad08440777406e")||
 (orun->identity=="wine-wow64-blender-019.1"&&r->identity=="blender-019.1-title"&&r->sha256=="2191c6d66d0964fb3916cc797d992204729e3bcf3ac0db73f57a4630ef1f2644")||
 (orun->identity=="wine-wow64-blender-019.2"&&r->identity=="blender-019.2-title"&&r->sha256=="b01d6c5dd932d51f5990a5b7dcafbcf954cca03a5d4e0414cc6976594315f712")||
 (orun->identity=="wine-wow64-blender-019.3-owner"&&r->identity=="blender-019.3-owner-title"&&r->sha256=="04ed27785c1c726362652d038db71de4482626b0e2a2cb349e241d0f9473a97e");
}
}
