#include "../src/steam_integration/blender_shared_enrollment.hpp"
#include <cassert>
extern "C" {
#include "../runtime-patches/blender/winbrew-bridge-helper/source/title_rule.h"
}
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
using ww::steam_integration::WriteBlenderSharedEnrollment;
static std::string read(const std::filesystem::path&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(){char tmp[]="/tmp/winbrew-enrollment-XXXXXX";assert(mkdtemp(tmp));auto root=std::filesystem::path(tmp)/"data";std::filesystem::create_directories(root/"wine11/title-rule");chmod(root.c_str(),0755);chmod((root/"wine11").c_str(),0755);chmod((root/"wine11/title-rule").c_str(),0700);
assert(WriteBlenderSharedEnrollment(root,"FAKE78042"));auto list=root/"wine11/title-rule/blender-shared/allow-list";assert(read(list)=="FAKE78042\n");assert(lapy_title_rule_check("FAKE78042","FAKE78",list.c_str(),geteuid())==LAPY_TITLE_RULE_ALLOWED);assert(lapy_title_rule_check("FAKE78013","FAKE78",list.c_str(),geteuid())==LAPY_TITLE_RULE_NOT_LISTED);assert(WriteBlenderSharedEnrollment(root,"FAKE78042"));assert(!WriteBlenderSharedEnrollment(root,"PPSA77382"));assert(!WriteBlenderSharedEnrollment(root,"FAKE78042\nFAKE78013"));assert(read(list)=="FAKE78042\n");assert(WriteBlenderSharedEnrollment(root,"FAKE78999"));assert(read(list)=="FAKE78999\n");assert(WriteBlenderSharedEnrollment(root,""));assert(read(list).empty());
std::filesystem::remove(list);std::filesystem::create_symlink(root/"outside",list);assert(!WriteBlenderSharedEnrollment(root,"FAKE78042"));assert(!std::filesystem::exists(root/"outside"));std::filesystem::remove_all(tmp);}
