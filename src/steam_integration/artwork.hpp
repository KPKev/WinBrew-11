// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../steam_ui.hpp"
#include <atomic>
#include <functional>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include "steam/backend/http_client.h"

namespace ww::steam_integration {
// Public presentation data only. Callbacks run on workers, never during drawing.
struct ArtworkCache {
  std::function<std::vector<std::uint8_t>(const std::string&, std::size_t)> read;
  std::function<void(const std::string&, const std::vector<std::uint8_t>&)> write;
  std::function<void(const std::string&)> remove;
};
class ArtworkSource {
 public:
  virtual ~ArtworkSource() = default;
  virtual void Request(std::uint32_t app) = 0;
  virtual bool Take(std::uint32_t app, SteamArtwork& artwork) = 0;
  virtual void Reset(std::uint32_t) {}
  virtual void RequestIcon(std::uint32_t, const std::string&) {}
  virtual bool TakeIcon(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void RejectIcon(std::uint32_t) {}
  virtual void RequestCover(std::uint32_t) {}
  virtual bool TakeCover(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void RejectCover(std::uint32_t) {}
  virtual void RequestPortrait(std::uint32_t) {}
  virtual bool TakePortrait(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void RejectPortrait(std::uint32_t) {}
};

// Only fixed HTTPS Steam CDN URLs are requested, without account credentials.
// A worker owns the network client; the application thread never waits on it.
class ArtworkService final : public ArtworkSource {
 public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;
  explicit ArtworkService(std::string ca_bundle, Clock clock = [] { return std::chrono::steady_clock::now(); }, ArtworkCache cache = {});
  ~ArtworkService();
  void Request(std::uint32_t app) override;
  void Reset(std::uint32_t app) override;
  bool Take(std::uint32_t app, SteamArtwork& artwork) override;
  void RequestIcon(std::uint32_t app, const std::string& url) override;
  bool TakeIcon(std::uint32_t app, std::vector<std::uint8_t>& bytes) override;
  void RejectIcon(std::uint32_t app) override;
  void RequestCover(std::uint32_t app) override;
  bool TakeCover(std::uint32_t app, std::vector<std::uint8_t>& bytes) override;
  void RejectCover(std::uint32_t app) override;
  void RequestPortrait(std::uint32_t app) override;
  bool TakePortrait(std::uint32_t app, std::vector<std::uint8_t>& bytes) override;
  void RejectPortrait(std::uint32_t app) override;
 private:
  void Run();
  void RunIcons();
  void RunCovers();
  ArtworkCache cache_;
  std::mutex cache_mutex_;
  std::set<std::string> rejected_cache_;
  std::atomic<std::uint64_t> cache_epoch_{0};
  playden::steam::HttpResponse Fetch(playden::steam::HttpClient&, const playden::steam::HttpRequest&,
      const std::atomic<bool>&, std::uint32_t app, const char* kind, std::uint64_t epoch);
  void RejectCache(std::uint32_t app, const char* kind);
  std::string ca_;
  Clock clock_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::condition_variable icon_wake_;
  std::condition_variable cover_wake_;
  std::deque<std::uint32_t> cover_queue_, portrait_queue_;
  std::deque<std::uint32_t> cover_result_order_, portrait_result_order_;
  bool prefer_portrait_ = true;
  std::set<std::uint32_t> portrait_requested_;
  std::map<std::uint32_t, std::vector<std::uint8_t>> portrait_results_;
  std::map<std::uint32_t, int> portrait_result_source_, portrait_last_source_, portrait_next_source_;
  std::map<std::uint32_t, std::chrono::steady_clock::time_point> portrait_retry_after_;
  std::set<std::uint32_t> cover_requested_;
  std::map<std::uint32_t, std::vector<std::uint8_t>> cover_results_;
  std::map<std::uint32_t, int> cover_result_source_, cover_last_source_, cover_next_source_;
  std::map<std::uint32_t, std::chrono::steady_clock::time_point> cover_retry_after_;
  std::deque<std::pair<std::uint32_t, std::string>> icon_queue_;
  std::set<std::uint32_t> icon_requested_;
  std::map<std::uint32_t, std::vector<std::uint8_t>> icon_results_;
  std::deque<std::uint32_t> icon_result_order_;
  std::map<std::uint32_t, int> icon_result_source_, icon_last_source_, icon_next_source_;
  std::map<std::uint32_t, std::chrono::steady_clock::time_point> icon_retry_after_;
  std::uint32_t requested_ = 0, finished_ = 0;
  std::uint64_t request_serial_ = 0, working_serial_ = 0;
  SteamArtwork artwork_;
  std::atomic<bool> stopping_{false};
  std::shared_ptr<std::atomic<bool>> active_detail_cancel_;
  std::thread worker_;
  std::thread icon_worker_a_, icon_worker_b_;
  std::thread cover_worker_a_, cover_worker_b_;
};
}
