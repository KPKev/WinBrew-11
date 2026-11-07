// SPDX-License-Identifier: GPL-3.0-or-later
#include "artwork.hpp"
#include "steam/backend/http_client.h"
#include "steam/backend/json.h"
#include "steam/store_details_service.h"
#include <utility>
#include <algorithm>

namespace ww::steam_integration {
namespace {
// Steam image endpoints can return an HTTP 200 error page or other non-image
// payload. Reject these before publishing them to the decoder, so fallback
// and the existing retry cooldown apply instead of downloading every frame.
bool image_response(const playden::steam::HttpResponse& response) {
  const auto& b = response.body;
  if (!response.ok() || response.status_code != 200) return false;
  if (b.size() >= 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff) return true;
  static constexpr unsigned char png[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  return b.size() >= sizeof(png) && std::equal(png, png + sizeof(png), b.begin());
}
// Keep only bounded, single-line text for rendering. Do not interpret links or
// markup from a news response as trusted UI or executable content.
std::string news_text(const std::string& text, std::size_t limit, bool strip_markup = true) {
  std::string result;
  bool space = false;
  for (std::size_t i = 0; i < text.size();) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    // Steam blurbs can contain truncated BBCode or HTML. Omit tags and image
    // URLs, and discard an incomplete tag at the end of a truncated blurb.
    if (strip_markup && (c == '[' || c == '<')) {
      const char close = c == '[' ? ']' : '>';
      const auto end = text.find(close, i + 1);
      if (end == std::string::npos) break;
      if (c == '[' && text.compare(i, 5, "[img]") == 0) {
        const auto image_end = text.find("[/img]", end + 1);
        if (image_end == std::string::npos) break;
        i = image_end + 6;
      } else i = end + 1;
      space = !result.empty(); continue;
    }
    if (c <= 32 || c == 127) { space = !result.empty(); ++i; continue; }
    std::size_t n = 1;
    if (c >= 0xc2 && c <= 0xdf) n = 2;
    else if (c >= 0xe0 && c <= 0xef) n = 3;
    else if (c >= 0xf0 && c <= 0xf4) n = 4;
    else if (c >= 0x80) { ++i; continue; }
    if (i + n > text.size()) break;
    bool valid = true;
    for (std::size_t j = 1; j < n; ++j)
      if ((static_cast<unsigned char>(text[i + j]) & 0xc0) != 0x80) valid = false;
    if (n == 3 && ((c == 0xe0 && static_cast<unsigned char>(text[i+1]) < 0xa0) ||
                   (c == 0xed && static_cast<unsigned char>(text[i+1]) >= 0xa0))) valid = false;
    if (n == 4 && ((c == 0xf0 && static_cast<unsigned char>(text[i+1]) < 0x90) ||
                   (c == 0xf4 && static_cast<unsigned char>(text[i+1]) >= 0x90))) valid = false;
    if (!valid) { ++i; continue; }
    const std::size_t separator = space && !result.empty() ? 1 : 0;
    if (result.size() + separator + n > limit) break;
    if (separator) result += ' ';
    space = false; result.append(text, i, n); i += n;
  }
  return result;
}

SteamArtwork parse_news(std::uint32_t app, const std::vector<std::uint8_t>& bytes) {
  SteamArtwork result; result.news_state = SteamNewsState::unavailable;
  const std::string body(bytes.begin(), bytes.end());
  const auto root = playden::steam::json::Parse(body, {256 * 1024, 16, 4096, 64 * 1024});
  if (!root || !root->is_object()) return result;
  const auto* group = root->Find("appnews");
  if (!group || !group->is_object()) return result;
  const auto* id = group->Find("appid"); const auto* items = group->Find("newsitems");
  if (!id || id->AsUint64() != std::optional<std::uint64_t>(app) || !items || !items->is_array()) return result;
  if (items->array().empty()) { result.news_state = SteamNewsState::empty; return result; }
  for (const auto& item : items->array()) {
    if (!item.is_object()) continue;
    const auto* item_id = item.Find("appid"); const auto* title = item.Find("title");
    const auto* date = item.Find("date"); const auto* feed = item.Find("feedlabel");
    const auto* contents = item.Find("contents");
    if ((item_id && item_id->AsUint64() != std::optional<std::uint64_t>(app)) ||
        !title || !title->is_string() || !date) continue;
    const auto timestamp = date->AsUint64();
    if (!timestamp || !*timestamp || *timestamp <= result.news_published) continue;
    // Steam titles are plain text: bracketed labels such as [AMA] are part of the title.
    auto clean = news_text(title->text(), 192, false); if (clean.empty()) continue;
    result.news_title = std::move(clean);
    result.news_feed = feed && feed->is_string() ? news_text(feed->text(), 64) : std::string{};
    result.news_description = contents && contents->is_string() ? news_text(contents->text(), 300) : std::string{};
    result.news_published = *timestamp; result.news_state = SteamNewsState::available;
  }
  return result;
}
}
ArtworkService::ArtworkService(std::string ca_bundle, Clock clock, ArtworkCache cache) : cache_(std::move(cache)), ca_(std::move(ca_bundle)), clock_(std::move(clock)), worker_([this] { Run(); }),
    icon_worker_a_([this] { RunIcons(); }), icon_worker_b_([this] { RunIcons(); }),
    cover_worker_a_([this] { RunCovers(); }), cover_worker_b_([this] { RunCovers(); }) {}
namespace {
std::string cache_key(std::uint32_t app, const char* kind) {
  return "vapor-steam-" + std::to_string(app) + "-" + kind + "-v1.cache";
}
}
void ArtworkService::RejectCache(std::uint32_t app, const char* kind) {
  // Called with mutex_ held. Actual disk removal is deferred to a worker.
  rejected_cache_.insert(cache_key(app, kind));
}
void ArtworkService::Reset(std::uint32_t app) {
  if (!app) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++cache_epoch_; // old in-flight responses cannot refill a reset cache
    for (const char* kind : {"hero", "logo", "details", "news", "icon", "cover", "portrait"})
      RejectCache(app, kind);
    if (icon_results_.erase(app)) icon_requested_.erase(app);
    if (cover_results_.erase(app)) cover_requested_.erase(app);
    if (portrait_results_.erase(app)) portrait_requested_.erase(app);
    icon_result_source_.erase(app); cover_result_source_.erase(app); portrait_result_source_.erase(app);
    for (auto* order : {&icon_result_order_, &cover_result_order_, &portrait_result_order_})
      order->erase(std::remove(order->begin(), order->end(), app), order->end());
    icon_retry_after_.erase(app); cover_retry_after_.erase(app); portrait_retry_after_.erase(app);
    icon_next_source_.erase(app); cover_next_source_.erase(app); portrait_next_source_.erase(app);
  }
  Request(app);
}
playden::steam::HttpResponse ArtworkService::Fetch(playden::steam::HttpClient& client,
    const playden::steam::HttpRequest& request, const std::atomic<bool>& cancel,
    std::uint32_t app, const char* kind, std::uint64_t epoch) {
  const auto key = cache_key(app, kind);
  auto valid = [app, kind](const playden::steam::HttpResponse& response) {
    if (!response.ok() || response.status_code != 200) return false;
    if (std::string(kind) == "details")
      return bool(playden::steam::ParseStoreDetails(app, std::string(response.body.begin(), response.body.end())));
    if (std::string(kind) == "news")
      return parse_news(app, response.body).news_state != SteamNewsState::unavailable;
    return image_response(response);
  };
  bool reject = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    reject = rejected_cache_.erase(key) != 0;
  }
  {
    std::lock_guard<std::mutex> disk(cache_mutex_);
    if (reject && cache_.remove) cache_.remove(key);
    if (!reject && cache_.read && epoch == cache_epoch_) {
      playden::steam::HttpResponse cached;
      cached.status_code = 200;
      // Fallback artwork can be larger than the primary endpoint.
      cached.body = cache_.read(key, 2 * 1024 * 1024);
      if (valid(cached)) return cached;
    }
  }
  auto response = client.Perform(request, cancel);
  if (!cancel && epoch == cache_epoch_ && valid(response) && cache_.write) {
    std::lock_guard<std::mutex> disk(cache_mutex_);
    if (epoch == cache_epoch_) cache_.write(key, response.body);
  }
  return response;
}
ArtworkService::~ArtworkService() {
  stopping_ = true;
  { std::lock_guard<std::mutex> lock(mutex_); if (active_detail_cancel_) *active_detail_cancel_ = true; }
  wake_.notify_all(); icon_wake_.notify_all(); cover_wake_.notify_all();
  if (worker_.joinable()) worker_.join();
  if (icon_worker_a_.joinable()) icon_worker_a_.join();
  if (icon_worker_b_.joinable()) icon_worker_b_.join();
  if (cover_worker_a_.joinable()) cover_worker_a_.join();
  if (cover_worker_b_.joinable()) cover_worker_b_.join();
}
void ArtworkService::RequestCover(std::uint32_t app) {
  if (!app) return;
  std::lock_guard<std::mutex> lock(mutex_);
  const auto retry = cover_retry_after_.find(app);
  if (retry != cover_retry_after_.end() && clock_() < retry->second) return;
  if (stopping_ || cover_queue_.size() >= 64 || cover_requested_.size() >= 512 || !cover_requested_.insert(app).second) return;
  cover_queue_.push_back(app); cover_wake_.notify_one();
}
bool ArtworkService::TakeCover(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = cover_results_.find(app); if (found == cover_results_.end()) return false;
  bytes = std::move(found->second); cover_results_.erase(found); cover_requested_.erase(app);
  cover_result_order_.erase(std::remove(cover_result_order_.begin(), cover_result_order_.end(), app), cover_result_order_.end());
  if (cover_last_source_.size() >= 512 && !cover_last_source_.count(app)) cover_last_source_.erase(cover_last_source_.begin());
  cover_last_source_[app] = cover_result_source_[app]; cover_result_source_.erase(app); return true;
}
void ArtworkService::RejectCover(std::uint32_t app) {
  if (!app) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (cover_retry_after_.size() >= 512 && !cover_retry_after_.count(app)) cover_retry_after_.erase(cover_retry_after_.begin());
  cover_retry_after_[app] = clock_() + std::chrono::seconds(60);
  RejectCache(app, "cover");
  const auto last = cover_last_source_.find(app);
  if (last != cover_last_source_.end()) {
    if (cover_next_source_.size() >= 512 && !cover_next_source_.count(app)) cover_next_source_.erase(cover_next_source_.begin());
    cover_next_source_[app] = last->second == 1 ? 2 : 1;
  }
}
void ArtworkService::RequestPortrait(std::uint32_t app) {
  if (!app) return;
  std::lock_guard<std::mutex> lock(mutex_);
  const auto retry = portrait_retry_after_.find(app);
  if (retry != portrait_retry_after_.end() && clock_() < retry->second) return;
  if (stopping_ || portrait_queue_.size() >= 64 || portrait_requested_.size() >= 512 || !portrait_requested_.insert(app).second) return;
  portrait_queue_.push_back(app); cover_wake_.notify_one();
}
bool ArtworkService::TakePortrait(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = portrait_results_.find(app); if (found == portrait_results_.end()) return false;
  bytes = std::move(found->second); portrait_results_.erase(found); portrait_requested_.erase(app);
  portrait_result_order_.erase(std::remove(portrait_result_order_.begin(), portrait_result_order_.end(), app), portrait_result_order_.end());
  if (portrait_last_source_.size() >= 512 && !portrait_last_source_.count(app)) portrait_last_source_.erase(portrait_last_source_.begin());
  portrait_last_source_[app] = portrait_result_source_[app]; portrait_result_source_.erase(app); return true;
}
void ArtworkService::RejectPortrait(std::uint32_t app) {
  if (!app) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (portrait_retry_after_.size() >= 512 && !portrait_retry_after_.count(app)) portrait_retry_after_.erase(portrait_retry_after_.begin());
  portrait_retry_after_[app] = clock_() + std::chrono::seconds(60);
  RejectCache(app, "portrait");
  const auto last = portrait_last_source_.find(app);
  if (last != portrait_last_source_.end()) {
    if (portrait_next_source_.size() >= 512 && !portrait_next_source_.count(app)) portrait_next_source_.erase(portrait_next_source_.begin());
    portrait_next_source_[app] = last->second == 1 ? 2 : 1;
  }
}
void ArtworkService::RunCovers() {
  playden::steam::HttpClient client(ca_);
  while (!stopping_) {
    std::uint32_t app = 0; int preferred = 1; bool portrait = false;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cover_wake_.wait(lock, [this] { return stopping_ || !cover_queue_.empty() || !portrait_queue_.empty(); });
      if (stopping_) return;
      portrait = !portrait_queue_.empty() && (cover_queue_.empty() || prefer_portrait_);
      prefer_portrait_ = !portrait;
      if (portrait) {
        app = portrait_queue_.front(); portrait_queue_.pop_front();
        const auto next = portrait_next_source_.find(app); if (next != portrait_next_source_.end()) preferred = next->second;
      } else {
        app = cover_queue_.front(); cover_queue_.pop_front();
        const auto next = cover_next_source_.find(app); if (next != cover_next_source_.end()) preferred = next->second;
      }
    }
    const auto epoch = cache_epoch_.load();
    if (portrait) {
      playden::steam::HttpRequest image;
      const auto cdn = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + "/";
      image.url = cdn + (preferred == 1 ? "library_600x900.jpg" : "header.jpg");
      image.timeout_ms = 5000; image.max_response_bytes = 2 * 1024 * 1024;
      auto response = Fetch(client, image, stopping_, app, "portrait", epoch);
      if (!image_response(response) && !stopping_) {
        image.url = cdn + (preferred == 1 ? "header.jpg" : "library_600x900.jpg");
        response = Fetch(client, image, stopping_, app, "portrait", epoch);
      }
      std::lock_guard<std::mutex> lock(mutex_);
      if (epoch != cache_epoch_) {
        portrait_requested_.erase(app); continue;
      }
      if (image_response(response)) {
        if (portrait_results_.size() >= 64) {
          const auto evicted = portrait_result_order_.front(); portrait_result_order_.pop_front();
          portrait_results_.erase(evicted); portrait_result_source_.erase(evicted); portrait_requested_.erase(evicted);
        }
        portrait_results_[app] = std::move(response.body);
        portrait_result_order_.push_back(app);
        portrait_result_source_[app] = image.url == cdn + "library_600x900.jpg" ? 1 : 2;
        portrait_retry_after_.erase(app);
      } else {
        portrait_requested_.erase(app);
        if (portrait_retry_after_.size() < 512 || portrait_retry_after_.count(app))
          portrait_retry_after_[app] = clock_() + std::chrono::seconds(60);
      }
      continue;
    }
    playden::steam::HttpRequest request;
    request.url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + (preferred == 2 ? "/library_hero.jpg" : "/header.jpg");
    request.timeout_ms = 5000; request.max_response_bytes = preferred == 2 ? 2 * 1024 * 1024 : 512 * 1024;
    auto response = Fetch(client, request, stopping_, app, "cover", epoch);
    if (!image_response(response) && !stopping_) {
      request.url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + (preferred == 2 ? "/header.jpg" : "/library_hero.jpg");
      request.max_response_bytes = preferred == 2 ? 512 * 1024 : 2 * 1024 * 1024;
      response = Fetch(client, request, stopping_, app, "cover", epoch);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (epoch != cache_epoch_) { cover_requested_.erase(app); continue; }
    if (image_response(response)) {
      if (cover_results_.size() >= 64) {
        const auto evicted = cover_result_order_.front(); cover_result_order_.pop_front();
        cover_results_.erase(evicted); cover_result_source_.erase(evicted); cover_requested_.erase(evicted);
      }
      cover_results_[app] = std::move(response.body); cover_result_source_[app] = request.url.find("/library_hero.jpg") != std::string::npos ? 2 : 1; cover_retry_after_.erase(app);
      cover_result_order_.push_back(app);
    } else {
      cover_requested_.erase(app);
      if (cover_retry_after_.size() < 512 || cover_retry_after_.count(app))
        cover_retry_after_[app] = clock_() + std::chrono::seconds(60);
    }
  }
}
void ArtworkService::RequestIcon(std::uint32_t app, const std::string& url) {
  const auto prefix = "https://media.steampowered.com/steamcommunity/public/images/apps/" + std::to_string(app) + "/";
  if (!app) return;
  if (!url.empty()) {
    if (url.size() != prefix.size() + 44 || url.compare(0, prefix.size(), prefix) || url.compare(url.size() - 4, 4, ".jpg")) return;
    for (std::size_t i = prefix.size(); i < prefix.size() + 40; ++i)
      if (!((url[i] >= '0' && url[i] <= '9') || (url[i] >= 'a' && url[i] <= 'f'))) return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const auto retry = icon_retry_after_.find(app);
  if (retry != icon_retry_after_.end() && clock_() < retry->second) return;
  if (stopping_ || icon_queue_.size() >= 64 || icon_requested_.size() >= 512 || !icon_requested_.insert(app).second) return;
  const auto next = icon_next_source_.find(app);
  const auto cdn = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + "/";
  icon_queue_.emplace_back(app, next == icon_next_source_.end() ? (url.empty() ? cdn + "header.jpg" : url) :
                                cdn + (next->second == 2 ? "library_hero.jpg" : "header.jpg")); icon_wake_.notify_one();
}
bool ArtworkService::TakeIcon(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = icon_results_.find(app); if (found == icon_results_.end()) return false;
  bytes = std::move(found->second); icon_results_.erase(found); icon_requested_.erase(app);
  icon_result_order_.erase(std::remove(icon_result_order_.begin(), icon_result_order_.end(), app), icon_result_order_.end());
  if (icon_last_source_.size() >= 512 && !icon_last_source_.count(app)) icon_last_source_.erase(icon_last_source_.begin());
  icon_last_source_[app] = icon_result_source_[app]; icon_result_source_.erase(app); return true;
}
void ArtworkService::RejectIcon(std::uint32_t app) {
  if (!app) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (icon_retry_after_.size() >= 512 && !icon_retry_after_.count(app)) icon_retry_after_.erase(icon_retry_after_.begin());
  icon_retry_after_[app] = clock_() + std::chrono::seconds(60);
  RejectCache(app, "icon");
  const auto last = icon_last_source_.find(app);
  if (last != icon_last_source_.end()) {
    if (icon_next_source_.size() >= 512 && !icon_next_source_.count(app)) icon_next_source_.erase(icon_next_source_.begin());
    icon_next_source_[app] = last->second == 1 ? 2 : 1;
  }
}
void ArtworkService::RunIcons() {
  playden::steam::HttpClient client(ca_);
  while (!stopping_) {
    std::pair<std::uint32_t, std::string> request;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      icon_wake_.wait(lock, [this] { return stopping_ || !icon_queue_.empty(); });
      if (stopping_) return;
      request = std::move(icon_queue_.front()); icon_queue_.pop_front();
    }
    const auto epoch = cache_epoch_.load();
    playden::steam::HttpRequest http; http.url = request.second; http.timeout_ms = 5000;
    http.max_response_bytes = request.second.find("https://cdn.cloudflare.steamstatic.com/") == 0 ? 512 * 1024 : 128 * 1024;
    auto response = Fetch(client, http, stopping_, request.first, "icon", epoch);
    const auto cdn = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(request.first) + "/";
    if (!image_response(response) && !stopping_ && http.url != cdn + "header.jpg") {
      http.url = cdn + "header.jpg";
      http.max_response_bytes = 512 * 1024;
      response = Fetch(client, http, stopping_, request.first, "icon", epoch);
    }
    if (!image_response(response) && !stopping_) {
      http.url = cdn + "library_hero.jpg";
      http.max_response_bytes = 2 * 1024 * 1024;
      response = Fetch(client, http, stopping_, request.first, "icon", epoch);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (epoch != cache_epoch_) { icon_requested_.erase(request.first); continue; }
    if (image_response(response)) {
      if (icon_results_.size() >= 128) {
        const auto evicted = icon_result_order_.front(); icon_result_order_.pop_front();
        icon_results_.erase(evicted); icon_result_source_.erase(evicted); icon_requested_.erase(evicted);
      }
      icon_results_[request.first] = std::move(response.body);
      icon_result_order_.push_back(request.first);
      icon_result_source_[request.first] = http.url == cdn + "header.jpg" ? 1 : http.url == cdn + "library_hero.jpg" ? 2 : 0;
      icon_retry_after_.erase(request.first);
    } else {
      icon_requested_.erase(request.first);
      if (icon_retry_after_.size() < 512 || icon_retry_after_.count(request.first))
        icon_retry_after_[request.first] = clock_() + std::chrono::seconds(60);
    }
  }
}
void ArtworkService::Request(std::uint32_t app) {
  if (!app || stopping_) return;
  std::lock_guard<std::mutex> lock(mutex_);
  // Each request represents a new selection, including reopening the same game.
  // Start a fresh generation so consumed results and temporary failures can recover.
  if (active_detail_cancel_) *active_detail_cancel_ = true;
  requested_ = app; ++request_serial_; finished_ = 0; artwork_ = {}; wake_.notify_all();
}
bool ArtworkService::Take(std::uint32_t app, SteamArtwork& artwork) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!app || finished_ != app || requested_ != app) return false;
  artwork = std::move(artwork_); artwork_ = {}; finished_ = 0; return true;
}
void ArtworkService::Run() {
  while (!stopping_) {
    std::uint32_t app = 0; std::uint64_t serial = 0;
    std::shared_ptr<std::atomic<bool>> cancel;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || (requested_ && request_serial_ != working_serial_); });
      if (stopping_) return;
      app = requested_; serial = working_serial_ = request_serial_;
      cancel = active_detail_cancel_ = std::make_shared<std::atomic<bool>>(false);
    }
    std::set<std::string> invalidated;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      invalidated.swap(rejected_cache_);
    }
    if (cache_.remove) {
      std::lock_guard<std::mutex> disk(cache_mutex_);
      for (const auto& key : invalidated) cache_.remove(key);
    }
    const auto epoch = cache_epoch_.load();
    auto publish = [this, app, serial, epoch](SteamArtwork&& part) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (requested_ != app || request_serial_ != serial || epoch != cache_epoch_) return;
      if (!part.hero.empty()) artwork_.hero = std::move(part.hero);
      if (!part.logo.empty()) artwork_.logo = std::move(part.logo);
      if (!part.description.empty()) artwork_.description = std::move(part.description);
      if (!part.genres.empty()) artwork_.genres = std::move(part.genres);
      if (part.news_state != SteamNewsState::pending) {
        artwork_.news_state = part.news_state; artwork_.news_title = std::move(part.news_title);
        artwork_.news_feed = std::move(part.news_feed); artwork_.news_description = std::move(part.news_description);
        artwork_.news_published = part.news_published;
      }
      finished_ = app;
    };
    // Retry temporary failures once; a new selection or shutdown interrupts
    // every waiting request. Permanent errors and rate limits are not retried.
    auto fetch = [this, &cancel, app, epoch](playden::steam::HttpClient& client, const playden::steam::HttpRequest& request, const char* kind) {
      auto response = Fetch(client, request, *cancel, app, kind, epoch);
      const auto error = response.error;
      const bool transient = error == playden::steam::HttpError::kNameResolutionFailed ||
          error == playden::steam::HttpError::kConnectionFailed || error == playden::steam::HttpError::kTimeout ||
          (response.ok() && response.status_code >= 500 && response.status_code <= 599);
      if (transient && !*cancel) {
        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock, std::chrono::seconds(1), [&] { return stopping_ || *cancel; });
        lock.unlock();
        if (!*cancel && !stopping_) response = Fetch(client, request, *cancel, app, kind, epoch);
      }
      return response;
    };
    // Use separate clients for the four independent requests. This bounds
    // concurrency to four and prevents a slow store response from delaying
    // the start of the images (or vice versa).
    std::thread hero_worker([&] {
    playden::steam::HttpClient client(ca_);
    SteamArtwork artwork;
    for (const char* image : {"library_hero.jpg", "header.jpg"}) {
      playden::steam::HttpRequest request;
      request.url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + "/" + image;
      request.timeout_ms = 8000; request.max_response_bytes = 2 * 1024 * 1024;
      auto response = fetch(client, request, "hero");
      if (image_response(response)) {
        artwork.hero = std::move(response.body); break;
      }
      if (*cancel) break;
    }
    if (!artwork.hero.empty()) publish(std::move(artwork));
    });
    std::thread details_worker([&] {
    playden::steam::HttpClient client(ca_);
    SteamArtwork artwork;
    if (!*cancel) {
      playden::steam::HttpRequest request;
      request.url = playden::steam::StoreDetailsUrl(app);
      request.timeout_ms = 8000; request.max_response_bytes = 2 * 1024 * 1024;
      auto response = fetch(client, request, "details");
      if (response.ok() && response.status_code == 200 && !*cancel) {
        const std::string body(response.body.begin(), response.body.end());
        if (auto details = playden::steam::ParseStoreDetails(app, body)) {
          artwork.description = std::move(details->description);
          if (artwork.description.size() > 1024) artwork.description.resize(1024);
          for (const auto& genre : details->genres) {
            if (artwork.genres.size() >= 256) break;
            if (!artwork.genres.empty()) artwork.genres += ", ";
            artwork.genres.append(genre, 0, 256 - artwork.genres.size());
          }
        }
      }
    }
    if (!artwork.description.empty() || !artwork.genres.empty()) publish(std::move(artwork));
    });
    std::thread news_worker([&] {
      playden::steam::HttpClient client(ca_);
      playden::steam::HttpRequest request;
      request.url = "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=" +
          std::to_string(app) + "&count=3&maxlength=0&format=json";
      request.timeout_ms = 8000; request.max_response_bytes = 256 * 1024;
      SteamArtwork artwork; artwork.news_state = SteamNewsState::unavailable;
      if (!*cancel) {
        auto response = fetch(client, request, "news");
        if (response.ok() && response.status_code == 200 && !*cancel)
          artwork = parse_news(app, response.body);
        // Very large full posts may exceed the response or parser limits.
        // Fall back to Steam's short text rather than losing news entirely.
        if (!*cancel && (response.error == playden::steam::HttpError::kResponseTooLarge ||
                         (response.ok() && response.status_code == 200 && artwork.news_state == SteamNewsState::unavailable))) {
          request.url = "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=" +
              std::to_string(app) + "&count=3&maxlength=300&format=json";
          response = fetch(client, request, "news");
          if (response.ok() && response.status_code == 200 && !*cancel)
            artwork = parse_news(app, response.body);
        }
      }
      if (!*cancel) publish(std::move(artwork));
    });
    if (!*cancel) {
      playden::steam::HttpClient client(ca_);
      SteamArtwork artwork;
      playden::steam::HttpRequest logo;
      logo.url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + "/logo.png";
      logo.timeout_ms = 4000; logo.max_response_bytes = 2 * 1024 * 1024;
      auto response = fetch(client, logo, "logo");
      if (image_response(response)) artwork.logo = std::move(response.body);
      if (!artwork.logo.empty()) publish(std::move(artwork));
    }
    hero_worker.join(); details_worker.join(); news_worker.join();
    if (stopping_) return;
  }
}
}
