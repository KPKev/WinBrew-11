#include "../src/explorer_fileops.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(){
 const auto root=fs::temp_directory_path()/("wb-restore-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),profile=root/"profile",desktop=profile/"Desktop",bin=desktop/".winbrew-trash",file=desktop/"note.txt",outside=root/"outside";
 fs::create_directories(bin);fs::create_directories(outside);std::ofstream(file)<<"restored content";
 ww::fileops::Info info{};assert(!ww::fileops::inspect(file.string().c_str(),info));char source[1024]{};assert(ww::fileops::trash(file.string().c_str(),info,source,sizeof(source),bin.string().c_str()));std::string original;assert(ww::fileops::recovery_original_path(fs::path(source).parent_path().string().c_str(),"note.txt",original)&&original==file.string());
 ww::fileops::Clipboard clip;assert(clip.copy(source));ww::fileops::CrossVolumeRecycle denied;assert(!denied.begin_restore(clip,outside.string().c_str(),bin.string().c_str(),profile.string().c_str()));assert(fs::exists(source));
 std::ofstream(file)<<"existing";ww::fileops::CrossVolumeRecycle x;assert(x.begin_restore(clip,desktop.string().c_str(),bin.string().c_str(),profile.string().c_str()));for(int i=0;x.busy()&&i<10000;++i)x.step();assert(x.state==ww::fileops::State::complete);assert(!fs::exists(source));assert(fs::exists(x.transfer.destination));std::ifstream existing(file),restored(x.transfer.destination);std::string e,r;std::getline(existing,e);std::getline(restored,r);assert(e=="existing"&&r=="restored content");
 const auto cancel_src=profile/"cancel-folder";fs::create_directories(cancel_src);std::ofstream(cancel_src/"item.txt")<<"keep";ww::fileops::Clipboard cancel_clip;assert(cancel_clip.copy(cancel_src.string().c_str()));ww::fileops::CrossVolumeRecycle cancel_empty;assert(cancel_empty.begin(cancel_clip,bin.string().c_str()));const std::string empty_output=cancel_empty.transfer.destination;assert(fs::exists(empty_output));cancel_empty.cancel();assert(!fs::exists(empty_output)&&fs::exists(cancel_src/"item.txt"));
 ww::fileops::CrossVolumeRecycle cancel_nonempty;assert(cancel_nonempty.begin(cancel_clip,bin.string().c_str()));const std::string retained_output=cancel_nonempty.transfer.destination;std::ofstream(fs::path(retained_output)/"other.txt")<<"other";cancel_nonempty.cancel();assert(fs::exists(fs::path(retained_output)/"other.txt")&&fs::exists(cancel_src/"item.txt"));
 const auto dir=desktop/"My Folder";fs::create_directories(dir/"nested");std::ofstream(dir/"nested/item.txt")<<"folder content";assert(!ww::fileops::inspect(dir.string().c_str(),info));assert(ww::fileops::trash(dir.string().c_str(),info,source,sizeof(source),bin.string().c_str()));ww::fileops::Clipboard folder;assert(folder.copy(source));ww::fileops::CrossVolumeRecycle y;assert(y.begin_restore(folder,desktop.string().c_str(),bin.string().c_str(),profile.string().c_str()));for(int i=0;y.busy()&&i<10000;++i)y.step();assert(y.state==ww::fileops::State::complete&&fs::exists(dir/"nested/item.txt")&&!fs::exists(source));
 assert(root.parent_path()==fs::temp_directory_path());fs::remove_all(root);std::cout<<"PASS: restore verifies copy, preserves conflicts, confines destination to profile\n";
}
