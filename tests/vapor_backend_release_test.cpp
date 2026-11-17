// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual release archives through the production updater in a disposable host tree.
#include "../src/steam_integration/backend_updates.hpp"
#include "runtime/artifact_hash.h"
#include "runtime/kit_manifest.h"
#include "runtime/kit_admission.h"
#include "runtime/kit_trust.h"
#include "steam/backend/json.h"
#include "update/file_ops.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace ww::steam_integration;
using ww::BackendUpdatePhase;
void Check(bool ok,const std::string& what){if(!ok)throw std::runtime_error(what);}
std::string Read(const fs::path& path){std::ifstream f(path,std::ios::binary);Check(bool(f),"read "+path.string());return {std::istreambuf_iterator<char>(f),{}};}
std::string Sha(const fs::path& path){auto sha=playden::runtime::HashArtifactFile(path);Check(bool(sha),"hash "+path.string());return *sha;}
void Put(const fs::path& path,const std::string& bytes){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f<<bytes;Check(bool(f),"write fixture");}
void Wait(BackendUpdates& updates,BackendUpdatePhase phase){for(int i=0;i<30000;++i){auto s=updates.Snapshot();if(s.phase==phase)return;if(s.phase==BackendUpdatePhase::failed)throw std::runtime_error(s.message);usleep(20000);}throw std::runtime_error("updater timeout");}
int main(int argc,char** argv){try{
 Check(argc==3,"usage: release-test package-folder fresh-fixture-folder");const fs::path packages=fs::absolute(argv[1]),root=fs::absolute(argv[2]);Check(!fs::exists(root),"fresh fixture required");
 std::string manifest=Read(packages/"vapor-backend-update.json");const std::string pending="candidate-not-console-tested";auto at=manifest.find(pending);Check(at!=std::string::npos,"candidate contract expected");manifest.replace(at,pending.size(),"ps5-13.60-reviewed");Check(ValidateBackendManifest(manifest,nullptr),"contract");
 const std::string base="https://github.com/KPKev/WinBrew-11/releases/download/v1.18.0/",latest="https://api.github.com/repos/KPKev/WinBrew-11/releases/latest";
 std::string release=R"({"tag_name":"v1.18.0","draft":false,"prerelease":false,"assets":[)";
 for(const std::string name:{"vapor-backend-update.json","WinBrew-11-v1.18.0.zip","Vapor-backend-kit-0.28.zip"}){const bool info=name=="vapor-backend-update.json";const auto hash=info?*playden::runtime::HashArtifactBytes(manifest):Sha(packages/name);release+="{\"name\":\""+name+"\",\"size\":"+std::to_string(info?manifest.size():fs::file_size(packages/name))+",\"digest\":\"sha256:"+hash+"\",\"browser_download_url\":\""+base+name+"\"},";}release.back()=']';release+='}';
 playden::update::UpdateConfig config;config.title_id="PPSA77382";config.current={1,17,0};config.latest_release_url=latest;config.releases_page="https://github.com/KPKev/WinBrew-11/releases";config.installed_root=(root/"app").string();config.work_root=(root/"updates").string();config.kits_root=(root/"kits").string();config.appmeta_root=(root/"appmeta").string();config.registered_app_root=(root/"registered").string();
 Put(root/"app/eboot.bin","old fixture executable");Put(root/"app/sce_sys/param.json",R"({"titleId":"PPSA77382","contentVersion":"01.017.000"})");Put(root/"kits/playden-kit/manifest.json","old fixture kit");
 config.as_root=[](const std::function<bool()>& work){return work();};config.hash_file=[](const std::string& path){return playden::runtime::HashArtifactFile(path);};
 config.verify_kit=[](const std::string& path)->std::optional<std::string>{auto k=playden::runtime::VerifyInstalledKit(path,playden::runtime::DevelopmentKitAdmission());return k.status==playden::runtime::KitManifestStatus::kVerified?std::optional<std::string>(k.manifest_sha256):std::nullopt;};
 config.free_bytes=[](const std::string& path)->std::optional<std::uint64_t>{std::error_code e;auto s=fs::space(fs::path(path).parent_path(),e);return e?std::nullopt:std::optional<std::uint64_t>(s.available);};config.wait_before_retry=[](int){throw std::runtime_error("unexpected transport retry");};config.log=[](const std::string& s){std::cout<<s<<std::endl;};
 config.fetch=[&](const playden::steam::HttpRequest& q,const std::atomic<bool>& cancel){playden::steam::HttpResponse r;if(cancel){r.error=playden::steam::HttpError::kCanceled;return r;}std::string small;
 if(q.url==latest)small=release;else if(q.url=="https://api.github.com/repos/leolobato/playden-ps5/releases/latest")small=R"({"tag_name":"v0.11.0","draft":false,"prerelease":false,"assets":[]})";else if(q.url==base+"vapor-backend-update.json")small=manifest;
 if(!small.empty()){r.status_code=200;r.body.assign(small.begin(),small.end());return r;}
 Check(q.url.starts_with(base),"unexpected URL");const std::string name=q.url.substr(base.size());Check(name=="WinBrew-11-v1.18.0.zip"||name=="Vapor-backend-kit-0.28.zip","unexpected asset");std::string range;for(auto& [key,value]:q.headers)if(key=="Range")range=value;Check(range.starts_with("bytes="),"expected range transport");auto dash=range.find('-');auto first=std::stoull(range.substr(6,dash-6)),last=std::stoull(range.substr(dash+1));Check(last>=first&&last<fs::file_size(packages/name),"range bounds");r.body.resize(last-first+1);std::ifstream f(packages/name,std::ios::binary);f.seekg(first);f.read(reinterpret_cast<char*>(r.body.data()),r.body.size());Check(bool(f),"range read");r.status_code=206;r.headers.emplace_back("Content-Range","bytes "+std::to_string(first)+"-"+std::to_string(last)+"/"+std::to_string(fs::file_size(packages/name)));return r;};
 {
 auto updates=std::make_shared<BackendUpdates>(config,"0.8.0");updates->Start();Wait(*updates,BackendUpdatePhase::available);Check(updates->Install(),"install offered");Wait(*updates,BackendUpdatePhase::staged);Check(Read(root/"app/eboot.bin")=="old fixture executable","staging preserved running app");auto apply=updates->RestartAction(false);Check(bool(apply)&&apply(),"apply/restart");
 auto doc=playden::steam::json::Parse(Read(root/"app/manifest.json"));const auto* files=doc?doc->Find("files"):nullptr;Check(files&&files->is_object(),"installed app manifest");std::size_t count=0;for(auto& [name,value]:files->object()){Check(value.is_string()&&Sha(root/"app"/name)==value.text(),"installed app hash "+name);++count;}Check(Sha(root/"app/eboot.bin")=="158936b05422289099d3c4c0123d80f7c7d22913e3cceb00d14ed2f43cfd2e6a","qualified executable");Check(config.verify_kit((root/"kits/playden-kit").string())==std::optional<std::string>("ef0af8cb8a39ff5c6327d795873705ac79be4fc4109855181839bbbec06a3432"),"qualified kit");std::cout<<"RELEASE_TEST installed files="<<count<<std::endl;
 }
 config.current={1,18,0};{auto updates=std::make_shared<BackendUpdates>(config,"0.11.0");updates->Start();Wait(*updates,BackendUpdatePhase::current);Check(updates->Snapshot().rollback_available,"rollback offered");auto restore=updates->RestartAction(true);Check(bool(restore)&&restore(),"rollback");Check(Read(root/"app/eboot.bin")=="old fixture executable","old app restored");Check(Read(root/"kits/playden-kit/manifest.json")=="old fixture kit","old kit restored");}
 std::cout<<"RELEASE_TEST passed: actual archives download-stage-apply-confirm-rollback"<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"RELEASE_TEST failed: "<<e.what()<<std::endl;return 1;}}
