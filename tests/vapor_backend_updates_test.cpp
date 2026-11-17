// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse the upstream archive/server fixtures and run its transaction regressions.
#define main upstream_self_update_main
#include "../vendor/playden-source/tests/self_update_service_test.cpp"
#undef main
#include "../src/steam_integration/backend_updates.hpp"
#include <condition_variable>
using namespace ww::steam_integration;
using ww::BackendUpdatePhase;
namespace {
const std::string VaporLatest = "https://api.github.com/repos/KPKev/WinBrew-11/releases/latest";
const std::string UpstreamLatest = "https://api.github.com/repos/leolobato/playden-ps5/releases/latest";
const std::string Downloads = "https://github.com/KPKev/WinBrew-11/releases/download/v0.18.1/";
std::string Manifest() {
  return R"({"format":1,"app":{"version":"0.18.1","title_id":"PPSA77382","min_from":"0.18.0","asset":"vapor.zip","prefix":"PPSA77382/"},"kit":{"kit_id":"playden-kit","revision":"0.28","manifest_sha256":")" + kKitSha +
      R"(","asset":"kit.zip","prefix":"playden-kit-0.28/playden-kit/"},"notes":"Backend update","vapor_backend":{"schema":1,"frontend_abi":1,"source_revision":"7d30aaa7dc3011561eae728badc133a6bcc7f73b","upstream_version":"0.11.0","runtime_revision":"0.28","package_kind":"complete-winbrew","qualification":"ps5-13.60-reviewed"}})";
}
void WaitFor(BackendUpdates& updates, BackendUpdatePhase phase) {
  for (int i = 0; i < 2000; ++i) {
    if (updates.Snapshot().phase == phase) return;
    usleep(5000);
  }
  std::fprintf(stderr, "Unexpected update state %d: %s\n", int(updates.Snapshot().phase), updates.Snapshot().message.c_str());
  CHECK(false);
}
}
int main() {
  CHECK(upstream_self_update_main() == 0);
  const std::string manifest = Manifest();
  std::string error;
  CHECK(ValidateBackendManifest(manifest, &error));
  for (const auto& change : std::vector<std::pair<std::string, std::string>>{
      {"PPSA77382", "FAKE77031"}, {"\"frontend_abi\":1", "\"frontend_abi\":2"},
      {"ps5-13.60-reviewed", "untested"}, {"7d30aaa7dc3011561eae728badc133a6bcc7f73b", "HEAD"},
      {"\"runtime_revision\":\"0.28\"", "\"runtime_revision\":\"0.13\""}}) {
    auto bad = manifest; bad.replace(bad.find(change.first), change.first.size(), change.second);
    CHECK(!ValidateBackendManifest(bad, &error));
  }
  Fixture fixture;
  fixture.root = zip_fixture::TempFolder("vapor-backend-update");
  const std::string installed = fixture.root + "/homebrew/PPSA77382";
  Put(installed + "/eboot.bin", "old app");
  Put(installed + "/playden-version", "0.18.0\n");
  Put(installed + "/sce_sys/param.json", R"({"titleId":"PPSA77382","contentVersion":"00.018.000"})");
  CHECK(InstalledVaporVersion(installed) == playden::update::ParseVersion("0.18.0"));
  Put(installed + "/playden-version", "0.18.0-beta.2\n");
  CHECK(InstalledVaporVersion(installed) == playden::update::ParseVersion("0.18.0-beta.2"));
  Put(installed + "/playden-version", "99.0.0\n");
  CHECK(InstalledVaporVersion(installed) == playden::update::ParseVersion("0.18.0"));
  Put(installed + "/playden-version", "0.18.0\n");
  const std::string app = zip_fixture::Build({
      {"PPSA77382/eboot.bin", "new app", 0100755},
      {"PPSA77382/playden-version", "0.18.1\n"},
      {"PPSA77382/sce_sys/param.json", R"({"titleId":"PPSA77382","contentVersion":"00.018.001"})"}});
  const std::string kit = zip_fixture::Build({
      {"playden-kit-0.28/playden-kit/manifest.json", kKitSha},
      {"playden-kit-0.28/playden-kit/runtime/wine.bin", "wine"}});
  const auto asset = [](const std::string& name, const std::string& body) {
    return R"({"name":")" + name + R"(","size":)" + std::to_string(body.size()) +
        R"(,"digest":"sha256:)" + FakeSha(body) + R"(","browser_download_url":")" + Downloads + name + R"("})";
  };
  fixture.server.files = {{Downloads + BackendManifestAsset, manifest}, {Downloads + "vapor.zip", app}, {Downloads + "kit.zip", kit}};
  fixture.server.latest = R"({"tag_name":"v0.18.1","draft":false,"prerelease":false,"assets":[)" +
      asset(BackendManifestAsset, manifest) + "," + asset("vapor.zip", app) + "," + asset("kit.zip", kit) + "]}";
  auto config = fixture.Config({0, 18, 0});
  config.latest_release_url = VaporLatest;
  config.title_id = "PPSA77382"; config.installed_root = installed;
  config.releases_page = "https://github.com/KPKev/WinBrew-11/releases";
  std::mutex server_mutex;
  config.fetch = [&](const HttpRequest& request, const std::atomic<bool>&) {
    std::lock_guard lock(server_mutex);
    if (request.url == UpstreamLatest) {
      const std::string json = R"({"tag_name":"v0.11.0","draft":false,"prerelease":false,"assets":[]})";
      HttpResponse response; response.status_code = 200; response.body.assign(json.begin(), json.end()); return response;
    }
    auto mapped = request;
    if (mapped.url == VaporLatest) mapped.url = kLatest;
    return fixture.server(mapped);
  };
  {
    auto updates = std::make_shared<BackendUpdates>(config, "0.8.0");
    updates->Start();
    WaitFor(*updates, BackendUpdatePhase::available);
    CHECK(updates->Snapshot().latest_upstream == "0.11.0");
    CHECK(!updates->RestartAction(false));
    CHECK(updates->Install());
    WaitFor(*updates, BackendUpdatePhase::staged);
    CHECK(Read(installed + "/eboot.bin") == "old app"); // staging cannot replace the running app
    auto apply = updates->RestartAction(false); CHECK(apply && apply());
    CHECK(Read(installed + "/eboot.bin") == "new app");
    updates.reset();
  }
  {
    config.current = {0, 18, 1};
    auto updates = std::make_shared<BackendUpdates>(config, "0.11.0");
    updates->Start();
    WaitFor(*updates, BackendUpdatePhase::current);
    CHECK(updates->Snapshot().rollback_available);
    auto restore = updates->RestartAction(true); CHECK(restore && restore());
    CHECK(Read(installed + "/eboot.bin") == "old app");
  }
  RemoveTree(fixture.root);
  std::puts("Vapor backend update transactions passed");
}
