// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam/backend/depot_installer.h"
#include "steam/backend/depot_chunk.h"
#include "steam/install_volumes.h"
#include "../src/steam_integration/download_space_status.hpp"
#include <cassert>
#include <filesystem>
#include <cstdlib>
#include <limits>
using namespace playden;
using namespace playden::steam;
static bool mock_supported = true;
static std::optional<VolumeSpace> mock_space;
namespace playden::steam {
bool VolumeSpaceSupported() { return mock_supported; }
std::optional<VolumeSpace> QueryVolumeSpace(const std::string&, std::uint64_t) { return mock_space; }
ChunkError ProcessDepotChunk(const std::vector<std::uint8_t>&, const std::array<std::uint8_t,32>&,
                             std::size_t, std::uint32_t, const std::array<std::uint8_t,20>&,
                             std::vector<std::uint8_t>*) { return ChunkError::kNone; }
}
int main() {
  char root[]="/tmp/wine11-space-query-XXXXXX"; assert(mkdtemp(root));
  DepotManifest manifest; manifest.depot_id=441; manifest.gid=11;
  ManifestFile file; file.path="game.bin"; file.size=1;
  ManifestChunk chunk; chunk.offset=0; chunk.original_size=1; chunk.compressed_size=1; file.chunks.push_back(chunk);
  manifest.files.push_back(file);
  InstallerHooks h;
  h.access=[](const StoredSession&,std::uint32_t,const std::vector<DepotRequest>& requests,const std::atomic<bool>&) {
    DepotAccessResult r; r.error=CmError::kNone; r.grants.push_back({requests[0].depot_id,requests[0].manifest_gid,{},77}); return r; };
  h.servers=[](std::uint32_t,const std::atomic<bool>&,std::vector<ContentServer>* out) { *out={{"a.example.invalid","a.example.invalid",1}}; return CdnError::kNone; };
  h.manifest=[&](const ContentServer&,const DepotGrant&,const std::atomic<bool>&,DepotManifest* out) { *out=manifest; return InstallError::kNone; };
  h.chunk=[](const ContentServer&,std::uint32_t,const std::string&,std::size_t,const std::atomic<bool>&,std::vector<std::uint8_t>*) { return CdnError::kCanceled; };
  DepotInfo depot; depot.id=441; depot.manifest_gid=11;
  InstallTarget target; target.app_id=440; target.install_root=root; target.depots.push_back(depot);
  DepotInstaller installer(h); std::atomic<bool> cancel{false};
  mock_space.reset(); auto unavailable=installer.Run({},target,cancel);
  assert(ww::steam_integration::IsSpaceQueryUnavailable(unavailable.error, unavailable.detail));
  assert(std::string(ww::steam_integration::kSpaceQueryMessage).find("unavailable") != std::string::npos);
  mock_space=VolumeSpace{1024, 0}; auto low=installer.Run({},target,cancel);
  assert(low.error==InstallError::kNoSpace && !ww::steam_integration::IsSpaceQueryUnavailable(low.error, low.detail));
  // A platform without any query retains upstream's existing unchecked path.
  mock_supported=false; mock_space.reset(); auto unsupported=installer.Run({},target,cancel);
  assert(unsupported.error==InstallError::kCanceled);
  std::filesystem::remove_all(root);
}
