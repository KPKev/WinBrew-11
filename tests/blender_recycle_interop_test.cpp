#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <filesystem>
#include <string>
int main(int argc,char**argv){assert(argc==4);std::string container=argv[1],original;ww::fileops::Info info{};std::string name;assert(ww::fileops::recovery_container_item(container.c_str(),name,info));assert(name=="scene.blend");assert(ww::fileops::recovery_original_path(container.c_str(),name.c_str(),original));assert(original==argv[2]);ww::fileops::Clipboard clip;assert(clip.copy((container+"/"+name).c_str()));ww::fileops::CrossVolumeRecycle restore;auto folder=std::filesystem::path(original).parent_path();assert(restore.begin_restore(clip,folder.string().c_str(),(folder/".winbrew-trash").string().c_str(),argv[3]));for(int i=0;restore.busy()&&i<10000;i++)restore.step();assert(restore.state==ww::fileops::State::complete);assert(std::filesystem::exists(original));assert(!std::filesystem::exists(container+"/"+name));}
