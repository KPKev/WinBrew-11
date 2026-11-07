// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam/backend/depot_installer.h"
#include "steam/backend/depot_chunk.h"
#include "../src/steam_integration/download_tuning_store.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
using namespace playden;
using namespace playden::steam;
std::atomic<unsigned long long> simulated_tuner_ms{0}, observed_tuner_ms{0}, observed_tuner_bytes{0}, applied_tuner_limit{0};
namespace playden::steam {
ChunkError ProcessDepotChunk(const std::vector<std::uint8_t>&, const std::array<std::uint8_t,32>&,
                             std::size_t size, std::uint32_t, const std::array<std::uint8_t,20>&,
                             std::vector<std::uint8_t>* out) { out->assign(size, 0x41); return ChunkError::kNone; }
}
int main() {
  char base[]="/tmp/wine11-slots-XXXXXX"; assert(mkdtemp(base));
  const std::string root=std::string(base)+"/game"; assert(std::filesystem::create_directory(root));
  std::mutex mutex; std::condition_variable cv;
  int entered=0, active=0, allowed=0; std::atomic<int> completed{0};
  bool promoted=false, selected=false;
  const std::string setting=std::string(base)+"/slots", log=std::string(base)+"/tuning.log";
  assert(ww::steam_integration::ReadTunedSlots(setting)==20);
  DepotManifest manifest; manifest.depot_id=441; manifest.gid=11;
  ManifestFile file; file.path="game.bin"; file.size=60;
  for (unsigned i=0;i<60;++i) { ManifestChunk c; c.offset=i; c.original_size=1; c.compressed_size=1024*1024; c.sha[0]=i; file.chunks.push_back(c); }
  manifest.files.push_back(file);
  InstallerHooks h;
  h.access=[](const StoredSession&,std::uint32_t,const std::vector<DepotRequest>& requests,const std::atomic<bool>&) {
    DepotAccessResult r; r.error=CmError::kNone; r.grants.push_back({requests[0].depot_id,requests[0].manifest_gid,{},77}); return r; };
  h.servers=[](std::uint32_t,const std::atomic<bool>&,std::vector<ContentServer>* out) { *out={{"a.example.invalid","a.example.invalid",1}}; return CdnError::kNone; };
  h.manifest=[&](const ContentServer&,const DepotGrant&,const std::atomic<bool>&,DepotManifest* out) { *out=manifest; return InstallError::kNone; };
  h.free_bytes=[](const std::string&) { return std::uint64_t{1}<<40; };
  h.chunk=[&](const ContentServer&,std::uint32_t,const std::string&,std::size_t,const std::atomic<bool>& stop,std::vector<std::uint8_t>* out) {
    std::unique_lock<std::mutex> lock(mutex); const int ordinal=++entered; ++active; cv.notify_all();
    while (!stop && ordinal>allowed) cv.wait_for(lock,std::chrono::milliseconds(10));
    --active; cv.notify_all();
    if(stop) return CdnError::kCanceled;
    lock.unlock(); out->assign(1024*1024, 0); return CdnError::kNone;
  };
  h.trace=[&](const char* text) { ww::steam_integration::RecordTuning(setting,log,440,"trace",text);
    int tested,next,done; unsigned long long bytes,ms;
    if(std::sscanf(text,"adaptive tested=%d bytes=%llu ms=%llu next=%d done=%d",&tested,&bytes,&ms,&next,&done)==5) {
      std::lock_guard<std::mutex> lock(mutex); if(tested==5 && next==6 && !done) promoted=true;
      if(tested==6 && next==5 && done) selected=true; cv.notify_all(); }
  };
  DepotInfo depot; depot.id=441; depot.manifest_gid=11;
  InstallTarget target; target.app_id=440; target.install_root=root; target.depots.push_back(depot);
  DepotInstaller installer(h); installer.SetTuning({20,256}); std::atomic<bool> cancel{false}; InstallOutcome result;
  std::thread runner([&] { result=installer.Run({},target,cancel,[&](const InstallProgress& p) {completed=int(p.download_done/(1024*1024)); cv.notify_all();}); });
  auto wait=[&](auto condition) { std::unique_lock<std::mutex> lock(mutex); if (!cv.wait_for(lock,std::chrono::seconds(10),condition)) { std::fprintf(stderr,"timeout entered=%d active=%d completed=%d promoted=%d selected=%d now=%llu\n", entered,active,completed.load(),promoted,selected,simulated_tuner_ms.load()); std::abort(); } };
  auto tick=[&](unsigned long long now) {
    simulated_tuner_ms=now;
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(observed_tuner_ms.load()!=now+1 && std::chrono::steady_clock::now()<limit)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(observed_tuner_ms.load()==now+1);
  };
  wait([&]{return entered==5 && active==5;});
  tick(0); tick(15000);
  {std::lock_guard<std::mutex> lock(mutex); allowed=10;} cv.notify_all();
  wait([&]{return completed>=10 && active==5;});
  tick(30000); wait([&]{return promoted && active==6;});
  assert(!std::filesystem::exists(setting));
  {std::lock_guard<std::mutex> lock(mutex); allowed=14;} cv.notify_all();
  wait([&]{return completed>=14 && active==6;});
  const auto first_sample_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  while(observed_tuner_bytes.load()<14ull*1024*1024 && std::chrono::steady_clock::now()<first_sample_deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(observed_tuner_bytes.load()>=14ull*1024*1024);
  tick(45000);
  {std::lock_guard<std::mutex> lock(mutex); allowed=19;} cv.notify_all();
  wait([&]{return completed>=19 && active==6;});
  const auto sample_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  while(observed_tuner_bytes.load()<19ull*1024*1024 && std::chrono::steady_clock::now()<sample_deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(observed_tuner_bytes.load()>=19ull*1024*1024);
  tick(60000); wait([&]{return selected;});
  const auto applied_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  while(applied_tuner_limit.load()!=5 && std::chrono::steady_clock::now()<applied_deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(applied_tuner_limit.load()==5);
  // Existing in-flight work is not interrupted, but no new worker may start
  // until occupancy falls below the newly selected limit.
  {std::lock_guard<std::mutex> lock(mutex); allowed=20;} cv.notify_all();
  wait([&]{return completed>=20 && active==5;});
  {std::lock_guard<std::mutex> lock(mutex); assert(entered==25); allowed=21;} cv.notify_all();
  wait([&]{return entered==26 && active==5;});
  assert(ww::steam_integration::ReadTunedSlots(setting)==5);
  cancel=true; cv.notify_all(); runner.join(); assert(result.error==InstallError::kCanceled);
  // Recreate the install while keeping its saved setting and verify the next
  // real downloader uses the persisted fixed worker count.
  auto verify_saved = [&](int expected) {
    std::filesystem::remove_all(root); assert(std::filesystem::create_directory(root));
    { std::lock_guard<std::mutex> lock(mutex); entered=active=allowed=0; promoted=selected=false; }
    completed=0; cancel=false;
    DepotInstaller restored(h); restored.SetTuning({ww::steam_integration::ReadTunedSlots(setting),256});
    std::thread again([&] { result=restored.Run({},target,cancel); });
    wait([&]{return entered==expected && active==expected;});
    { std::lock_guard<std::mutex> lock(mutex); assert(!promoted && !selected); }
    cancel=true; cv.notify_all(); again.join(); assert(result.error==InstallError::kCanceled);
  };
  verify_saved(5);
  ww::steam_integration::RecordTuning(setting,log,440,"trace","adaptive tested=20 bytes=10000000 ms=30000 next=20 done=1");
  assert(ww::steam_integration::ReadTunedSlots(setting)==21); // fixed 20, not automatic
  verify_saved(20);
  std::filesystem::remove_all(base);
}
