// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend_updates.hpp"
#include "steam/backend/json.h"
#include "update/file_ops.h"
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <cstring>

namespace ww::steam_integration {
namespace {
constexpr const char* UpstreamRelease = "https://api.github.com/repos/leolobato/playden-ps5/releases/latest";
constexpr const char* VaporRelease = "https://api.github.com/repos/KPKev/WinBrew-11/releases/latest";
bool HexRevision(const std::string& s) {
  return s.size() == 40 && std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
}
std::optional<playden::update::Version> InstalledVaporVersion(const std::string& root) {
  std::string bytes;
  if (!playden::update::ReadSmallFile(root + "/sce_sys/param.json", &bytes, 64 << 10)) return {};
  const auto doc = playden::steam::json::Parse(bytes);
  const auto* title = doc ? doc->Find("titleId") : nullptr;
  const auto* version = doc ? doc->Find("contentVersion") : nullptr;
  if (!title || !title->is_string() || title->text() != "PPSA77382" || !version || !version->is_string()) return {};
  const auto base = playden::update::ParseVersion(version->text());
  if (!base) return {};
  if (playden::update::ReadSmallFile(root + "/playden-version", &bytes, 128)) {
    while (!bytes.empty() && (bytes.back() == '\n' || bytes.back() == '\r')) bytes.pop_back();
    const auto full = playden::update::ParseVersion(bytes);
    if (full && full->major == base->major && full->minor == base->minor && full->patch == base->patch) return full;
  }
  return base;
}
bool ValidateBackendManifest(std::string_view bytes, std::string* error) {
  const auto fail = [&](const char* text) { if (error) *error = text; return false; };
  const auto manifest = playden::update::ParseUpdateManifest(bytes, error);
  if (!manifest) return false;
  if (manifest->title_id != "PPSA77382" || manifest->app_prefix != "PPSA77382/" ||
      manifest->kit.kit_id != "playden-kit") return fail("This package is not for Vapor.");
  const auto doc = playden::steam::json::Parse(bytes);
  const auto* contract = doc ? doc->Find("vapor_backend") : nullptr;
  if (!contract || !contract->is_object()) return fail("No Vapor compatibility contract.");
  const auto number = [&](const char* key) { const auto* v = contract->Find(key); return v ? v->AsUint64() : std::nullopt; };
  const auto string = [&](const char* key) { const auto* v = contract->Find(key); return v && v->is_string() ? v->text() : std::string(); };
  if (number("schema") != 1 || number("frontend_abi") != BackendFrontendAbi)
    return fail("This update needs a different Vapor integration.");
  if (!HexRevision(string("source_revision")) || !playden::update::ParseVersion(string("upstream_version")) ||
      string("runtime_revision") != manifest->kit.revision || string("package_kind") != "complete-winbrew")
    return fail("Incomplete Vapor backend/runtime compatibility information.");
  if (string("qualification") != "ps5-13.60-reviewed")
    return fail("This backend has not been qualified for this console integration.");
  return true;
}
BackendUpdates::BackendUpdates(playden::update::UpdateConfig config, std::string installed)
    : fetch_(config.fetch), release_url_(config.latest_release_url) {
  status_.installed_backend = std::move(installed);
  // The URL is configured by the app, never from a game, recipe or server response.
  if (release_url_ != VaporRelease || !config.local_release_dir.empty())
    throw std::invalid_argument("Unexpected Vapor update channel");
  const auto transport = config.fetch;
  config.admit_manifest = ValidateBackendManifest;
  config.fetch = [transport](const playden::steam::HttpRequest& input, const std::atomic<bool>& cancel) {
    auto request = input;
    const std::string old = "playden-update.json";
    const auto at = request.url.rfind(old);
    const bool manifest = at != std::string::npos && at + old.size() == request.url.size();
    if (manifest) request.url.replace(at, old.size(), BackendManifestAsset);
    auto response = transport(request, cancel);
    if (manifest && response.ok() && response.status_code == 200 &&
        !ValidateBackendManifest(std::string_view(reinterpret_cast<const char*>(response.body.data()), response.body.size()), nullptr)) {
      response.error = playden::steam::HttpError::kProtocolError;
      response.body.clear();
    }
    // UpdateService locates its manifest by name in GitHub's asset inventory.
    // Alias only the publisher's compatible manifest; stock archives stay refused.
    if (request.url == VaporRelease && response.ok() && response.status_code == 200) {
      std::string json(response.body.begin(), response.body.end());
      std::size_t position = 0;
      while ((position = json.find(BackendManifestAsset, position)) != std::string::npos) {
        json.replace(position, std::strlen(BackendManifestAsset), old);
        position += old.size();
      }
      response.body.assign(json.begin(), json.end());
    }
    return response;
  };
  update_ = std::make_unique<playden::update::UpdateService>(std::move(config));
}
BackendUpdates::~BackendUpdates() { Shutdown(); }
void BackendUpdates::Start() { update_->Start(); Check(); }
void BackendUpdates::Shutdown() {
  cancel_ = true;
  if (checker_.joinable()) checker_.join();
  update_->Shutdown();
}
bool BackendUpdates::Check() {
  if (checking_.exchange(true)) return false;
  const auto phase = update_->Snapshot().phase;
  if (phase == playden::update::UpdatePhase::kDownloading || phase == playden::update::UpdatePhase::kUnpacking ||
      phase == playden::update::UpdatePhase::kStaged) { checking_ = false; return false; }
  if (checker_.joinable()) checker_.join();
  cancel_ = false;
  {
    std::lock_guard lock(mutex_); status_.phase = BackendUpdatePhase::checking; status_.message = "Checking backend updates...";
  }
  try {
    checker_ = std::thread([this] {
      playden::update::DownloadOptions options; options.fetch = fetch_; options.cancel = &cancel_;
      std::string error;
      const auto upstream_bytes = playden::update::FetchSmall(UpstreamRelease, 512 << 10, options, "application/vnd.github+json", &error);
      const auto upstream = upstream_bytes ? playden::update::ParseRelease(*upstream_bytes, &error) : std::nullopt;
      const auto compatible_bytes = playden::update::FetchSmall(release_url_, 512 << 10, options, "application/vnd.github+json", &error);
      const auto compatible = compatible_bytes ? playden::update::ParseRelease(*compatible_bytes, &error) : std::nullopt;
      const bool packaged = compatible && compatible->Find(BackendManifestAsset);
      {
        std::lock_guard lock(mutex_);
        if (upstream) status_.latest_upstream = upstream->version.ToString();
        const auto installed = playden::update::ParseVersion(status_.installed_backend);
        const bool newer = upstream && (!installed || *installed < upstream->version);
        status_.phase = newer ? BackendUpdatePhase::upstream_available : BackendUpdatePhase::current;
        status_.message = newer ? "New Playden backend found. Waiting for a compatible Vapor package."
                               : "No newer compatible Vapor backend package is available.";
        if (!upstream_bytes || !compatible_bytes) {
          status_.phase = BackendUpdatePhase::failed; status_.message = "Could not check updates. Try again when connected.";
        }
      }
      if (packaged && !cancel_) update_->Check(false);
      checking_ = false;
    });
  } catch (...) {
    checking_ = false;
    std::lock_guard lock(mutex_); status_.phase = BackendUpdatePhase::failed; status_.message = "Could not start the update check.";
    return false;
  }
  return true;
}
BackendUpdateStatus BackendUpdates::Snapshot() const {
  std::lock_guard lock(mutex_);
  auto result = status_;
  const auto state = update_->Snapshot();
  result.rollback_available = !state.previous.empty();
  if (state.available != playden::update::Version{}) result.available_package = state.available.ToString();
  if (state.phase == playden::update::UpdatePhase::kAvailable) {
    result.phase = BackendUpdatePhase::available; result.message = "A compatible Vapor backend update is available.";
    result.available_package = state.available.ToString();
  } else if (state.phase == playden::update::UpdatePhase::kChecking) result.phase = BackendUpdatePhase::checking;
  else if (state.phase == playden::update::UpdatePhase::kDownloading || state.phase == playden::update::UpdatePhase::kUnpacking) {
    result.phase = BackendUpdatePhase::downloading; result.message = state.phase == playden::update::UpdatePhase::kUnpacking
        ? "Verifying and staging the update..." : "Downloading backend and runtime...";
    result.done = state.done; result.total = state.total;
  } else if (state.phase == playden::update::UpdatePhase::kStaged) {
    result.phase = BackendUpdatePhase::staged; result.message = "Update ready. Restart WinBrew to finish; then reopen it from PS5 Home.";
  } else if (state.phase == playden::update::UpdatePhase::kFailed) {
    result.phase = BackendUpdatePhase::failed; result.message = state.error;
  }
  // A worker can publish its terminal phase before it releases its busy flag.
  // Do not offer actions during that last window (notably cold-start recovery).
  if (checking_ || update_->Busy()) {
    result.rollback_available = false;
    if (state.phase == playden::update::UpdatePhase::kStaged) {
      result.phase = BackendUpdatePhase::downloading;
      result.message = "Finishing update staging...";
    } else if (state.phase != playden::update::UpdatePhase::kDownloading && state.phase != playden::update::UpdatePhase::kUnpacking) {
      result.phase = BackendUpdatePhase::checking;
    }
  }
  if (!state.finished_message.empty() && result.phase == BackendUpdatePhase::current) {
    result.message = state.finished_message;
    if (result.message.rfind("Playden ", 0) == 0) result.message.replace(0, 7, "WinBrew ");
  }
  return result;
}
bool BackendUpdates::Install() {
  if (Snapshot().phase != BackendUpdatePhase::available || checking_ || update_->Busy()) return false;
  update_->Install(); return true;
}
std::function<bool()> BackendUpdates::RestartAction(bool rollback) {
  const auto state = Snapshot();
  if (checking_ || update_->Busy() || (rollback ? !state.rollback_available : state.phase != BackendUpdatePhase::staged)) return {};
  if (rollback) update_->RequestRollback(); else update_->RequestRestart();
  return [self = shared_from_this()] { self->Shutdown(); return self->update_->ApplyAtQuit(); };
}
}
