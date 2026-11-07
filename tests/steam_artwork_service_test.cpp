#include "../src/steam_integration/artwork.hpp"
#include "steam/backend/http_client.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <filesystem>
#include <fstream>

static std::atomic<int> cache_network_calls{0};
static std::atomic<bool> cache_test_mode{false}, cache_offline{false};
static std::atomic<bool> cache_block_icon{false}, cache_icon_entered{false}, cache_release_icon{false};
static std::atomic<bool> first_entered{false}, release_first{false}, news_blocked{false};
static std::atomic<int> bulk_icon_calls{0};
static std::atomic<int> reopened_hero{0}, reopened_details{0}, reopened_news{0};
static std::atomic<int> transient_details{0}, permanent_details{0}, transient_hero{0}, transient_logo{0};
static std::atomic<int> cs2_headers{0}, cs2_heroes{0}, bad_header_calls{0}, bad_hero_calls{0};
static std::atomic<int> portrait_primary_calls{0}, portrait_fallback_calls{0};
static std::atomic<int> malformed_icon_calls{0}, malformed_cover_calls{0}, icon_fallback_calls{0}, cover_fallback_calls{0};
static std::atomic<int> saturation_portraits{0}, saturation_covers{0}, fairness_blocked{0}, fairness_portraits_before_cover{0};
static std::atomic<bool> fairness_release{false}, fairness_cover_started{false};
namespace playden::steam {
class HttpClient::Impl {};
HttpClient::HttpClient(std::string) : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;
HttpResponse HttpClient::Perform(const HttpRequest& request, const std::atomic<bool>& cancel) {
  if (cache_test_mode) {
    ++cache_network_calls;
    if (cache_block_icon && request.url.find("/222/header.jpg") != std::string::npos) {
      cache_icon_entered = true;
      while (!cache_release_icon && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      HttpResponse old; old.status_code = 200; old.body = {0xff, 0xd8, 0xff, 99}; return old;
    }
    if (cache_offline) { HttpResponse r; r.status_code = 404; return r; }
    if (request.url.find("appid=222&") != std::string::npos) {
      const std::string body = R"({"appnews":{"appid":222,"newsitems":[]}})";
      HttpResponse r; r.status_code = 200; r.body.assign(body.begin(), body.end()); return r;
    }
  }
  if (request.url.find("https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/") == 0) {
    const bool short_news = request.url.find("&maxlength=300&") != std::string::npos;
    assert(short_news || request.url.find("&maxlength=0&") != std::string::npos);
    HttpResponse response; response.status_code = 200;
    if (request.url.find("appid=671&") != std::string::npos) {
      if (!short_news) { response.error = HttpError::kResponseTooLarge; return response; }
      const std::string body = R"({"appnews":{"appid":671,"newsitems":[{"appid":671,"title":"Fallback","contents":"Short text","date":10}]}})";
      response.body.assign(body.begin(), body.end()); return response;
    }

    if (request.url.find("appid=222&") != std::string::npos) ++reopened_news;
    if (request.url.find("appid=669&") != std::string::npos) {
      news_blocked = true;
      while (!cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      response.error = HttpError::kCanceled; return response;
    }
    std::string body;
    if (request.url.find("appid=666&") != std::string::npos)
      body = R"({"appnews":{"appid":666,"newsitems":[{"appid":666,"title":"Older","date":10},{"appid":777,"title":"Wrong game","date":40},{"appid":666,"title":"[AMA] Real update","feedlabel":"Official","contents":"[p][b]Patch[/b][/p][h3]Notes[/h3][p]fixes [img]https://example.invalid/image[/img] issues [broken","date":30}]}})";
    else if (request.url.find("appid=670&") != std::string::npos)
      body = std::string(R"({"appnews":{"appid":670,"newsitems":[{"appid":670,"title":")") + std::string(191, 'a') + "\xf0\x9f\x8e\xae" + R"(","contents":")" + std::string(299, 'b') + "\xf0\x9f\x8e\xae" + R"(","date":10}]}})";
    else if (request.url.find("appid=667&") != std::string::npos)
      body = R"({"appnews":{"appid":667,"newsitems":[]}})";
    else if (request.url.find("appid=668&") != std::string::npos)
      body = R"({"appnews":{"appid":999,"newsitems":[{"appid":999,"title":"Foreign","date":100}]}})";
    else body = "{}";
    response.body.assign(body.begin(), body.end()); return response;
  }
  if (request.url.find("https://media.steampowered.com/steamcommunity/public/images/apps/") == 0) {
    HttpResponse response; response.status_code = 200; response.body = {0xff, 0xd8, 0xff, 3}; return response;
  }
  if (request.url.find("https://store.steampowered.com/api/appdetails?") == 0) {
    if (request.url.find("appids=222") != std::string::npos) ++reopened_details;
    if (request.url.find("appids=333") != std::string::npos && ++transient_details == 1) {
      HttpResponse response; response.status_code = 503; return response;
    }
    if (request.url.find("appids=444") != std::string::npos) {
      ++permanent_details; HttpResponse response; response.status_code = 404; return response;
    }
    const auto id = request.url.find("appids=333") != std::string::npos ? "333" :
        request.url.find("appids=222") != std::string::npos ? "222" : "111";
    const std::string body = std::string("{\"") + id + "\":{\"success\":true,\"data\":{\"steam_appid\":" + id + ",\"short_description\":\"A &amp; B\",\"genres\":[{\"description\":\"Action\"}]}}}";
    HttpResponse response; response.status_code = 200; response.body.assign(body.begin(), body.end()); return response;
  }
  assert(request.url.find("https://cdn.cloudflare.steamstatic.com/steam/apps/") == 0);
  unsigned id = 0;
  if (std::sscanf(request.url.c_str(), "https://cdn.cloudflare.steamstatic.com/steam/apps/%u/", &id) == 1) {
    const bool portrait = request.url.find("/library_600x900.jpg") != std::string::npos;
    const bool cover = request.url.find("/header.jpg") != std::string::npos;
    if (portrait && id >= 50000 && id <= 50100) ++saturation_portraits;
    if (cover && id >= 51000 && id <= 51100) ++saturation_covers;
    if (portrait && (id == 60000 || id == 60001)) {
      ++fairness_blocked;
      while (!fairness_release && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (portrait && id >= 60002 && id <= 60010 && !fairness_cover_started) ++fairness_portraits_before_cover;
    if (cover && id == 61000) fairness_cover_started = true;
  }
  if (request.url.find("/90") != std::string::npos && request.url.find("/header.jpg") != std::string::npos) ++bulk_icon_calls;
  if (request.url.find("/222/library_hero.jpg") != std::string::npos) ++reopened_hero;
  if (request.url.find("/555/library_hero.jpg") != std::string::npos && ++transient_hero == 1) {
    HttpResponse response; response.status_code = 503; return response;
  }
  if (request.url.find("/555/logo.png") != std::string::npos && ++transient_logo == 1) {
    HttpResponse response; response.error = HttpError::kTimeout; return response;
  }
  if (request.url.find("/222/logo.png") != std::string::npos) {
    HttpResponse response; response.status_code = 200;
    response.body = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}; return response;
  }
  if (request.url.find("/772/library_600x900.jpg") != std::string::npos) ++portrait_primary_calls;
  if (request.url.find("/772/header.jpg") != std::string::npos) ++portrait_fallback_calls;
  if (request.url.find("/770/header.jpg") != std::string::npos) ++malformed_icon_calls;
  if (request.url.find("/771/header.jpg") != std::string::npos) ++malformed_cover_calls;
  if (request.url.find("/770/library_hero.jpg") != std::string::npos) ++icon_fallback_calls;
  if (request.url.find("/771/library_hero.jpg") != std::string::npos) ++cover_fallback_calls;
  if (request.url.find("/731/header.jpg") != std::string::npos) {
    ++bad_header_calls; HttpResponse response; response.status_code = 200;
    response.body = {'<', 'h', 't', 'm', 'l', '>'}; return response;
  }
  if (request.url.find("/731/library_hero.jpg") != std::string::npos) ++bad_hero_calls;
  if (request.url.find("/730/header.jpg") != std::string::npos) {
    ++cs2_headers; HttpResponse response; response.status_code = 404; return response;
  }
  if (request.url.find("/730/library_hero.jpg") != std::string::npos) ++cs2_heroes;
  if (request.url.find("/111/library_hero.jpg") != std::string::npos) {
    first_entered = true;
    while (!release_first && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  HttpResponse response; response.status_code = 200;
  response.body = request.url.find("/222/") != std::string::npos ? std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 2}) : std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1});
  return response;
}
}
static void persistent_cache_test() {
  namespace fs = std::filesystem;
  using ww::steam_integration::ArtworkService;
  const auto root = fs::temp_directory_path() / ("winbrew-art-cache-" + std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(root);
  ww::steam_integration::ArtworkCache cache{
    [&](const std::string& key, std::size_t max) {
      std::ifstream in(root / key, std::ios::binary | std::ios::ate);
      if (!in || in.tellg() < 0 || std::size_t(in.tellg()) > max) return std::vector<std::uint8_t>{};
      std::vector<std::uint8_t> bytes(std::size_t(in.tellg())); in.seekg(0);
      if (!in.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return std::vector<std::uint8_t>{};
      return bytes;
    },
    [&](const std::string& key, const std::vector<std::uint8_t>& bytes) {
      std::ofstream out(root / key, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    },
    [&](const std::string& key) { fs::remove(root / key); }
  };
  auto receive = [](ArtworkService& service) {
    ww::SteamArtwork all;
    for (int i = 0; i < 3000; ++i) {
      ww::SteamArtwork part;
      if (service.Take(222, part)) {
        if (!part.hero.empty()) all.hero = std::move(part.hero);
        if (!part.logo.empty()) all.logo = std::move(part.logo);
        if (!part.description.empty()) all.description = std::move(part.description);
        if (part.news_state != ww::SteamNewsState::pending) all.news_state = part.news_state;
      }
      if (!all.hero.empty() && !all.logo.empty() && !all.description.empty() && all.news_state == ww::SteamNewsState::empty) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "cached details were not delivered");
  };
  auto receive_images = [](ArtworkService& service) {
    std::vector<std::uint8_t> bytes; bool icon = false, cover = false, portrait = false;
    service.RequestIcon(222, ""); service.RequestCover(222); service.RequestPortrait(222);
    for (int i = 0; i < 3000; ++i) {
      if (!icon) icon = service.TakeIcon(222, bytes);
      if (!cover) cover = service.TakeCover(222, bytes);
      if (!portrait) portrait = service.TakePortrait(222, bytes);
      if (icon && cover && portrait) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "cached images were not delivered");
  };
  cache_test_mode = true; cache_offline = false; cache_network_calls = 0;
  {
    ArtworkService service("synthetic-ca", [] { return std::chrono::steady_clock::now(); }, cache);
    service.Request(222); receive(service); receive_images(service);
  }
  assert(cache_network_calls == 7);
  // Simulate a restart and removed upstream assets: all seven channels still load.
  cache_offline = true; cache_network_calls = 0;
  {
    ArtworkService service("synthetic-ca", [] { return std::chrono::steady_clock::now(); }, cache);
    service.Request(222); receive(service); receive_images(service);
    service.Request(222); receive(service);
  }
  assert(cache_network_calls == 0);
  // Refresh removes just this game, then refetches it. Another game's cache survives.
  { std::ofstream other(root / "vapor-steam-333-hero-v1.cache"); other << "keep"; }
  cache_offline = false;
  {
    ArtworkService service("synthetic-ca", [] { return std::chrono::steady_clock::now(); }, cache);
    service.Reset(222); receive(service); receive_images(service);
  }
  assert(cache_network_calls == 7 && fs::exists(root / "vapor-steam-333-hero-v1.cache"));
  // A corrupt cached image is a miss and is replaced, rather than shown forever.
  { std::ofstream bad(root / "vapor-steam-222-hero-v1.cache", std::ios::binary); bad << "invalid"; }
  cache_network_calls = 0;
  {
    ArtworkService service("synthetic-ca", [] { return std::chrono::steady_clock::now(); }, cache);
    service.Request(222); receive(service);
  }
  assert(cache_network_calls == 1);
  // Reset while an icon is downloading: its old response must not repopulate disk or UI.
  fs::remove(root / "vapor-steam-222-icon-v1.cache");
  cache_block_icon = true; cache_icon_entered = false; cache_release_icon = false;
  {
    ArtworkService service("synthetic-ca", [] { return std::chrono::steady_clock::now(); }, cache);
    service.RequestIcon(222, "");
    for (int i = 0; i < 3000 && !cache_icon_entered; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(cache_icon_entered);
    service.Reset(222); receive(service);
    cache_block_icon = false; cache_release_icon = true;
    std::vector<std::uint8_t> bytes; bool received = false;
    for (int i = 0; i < 3000 && !received; ++i) {
      service.RequestIcon(222, ""); received = service.TakeIcon(222, bytes);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(received && bytes.back() == 2);
    // Also reset completed, unconsumed results: their requested-set entries must be released.
    service.RequestCover(222);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    service.Reset(222); receive(service); receive_images(service);
  }
  cache_test_mode = false; cache_offline = false;
  fs::remove_all(root);
}
int main() {
  persistent_cache_test();
  std::atomic<int> fake_seconds{0};
  ww::steam_integration::ArtworkService service("synthetic-ca", [&] {
    return std::chrono::steady_clock::time_point{} + std::chrono::seconds(fake_seconds.load());
  });
  service.Request(111);
  for (int i = 0; i < 1000 && !first_entered; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(first_entered);
  ww::SteamArtwork early;
  bool details_before_hero = false;
  for (int i = 0; i < 1000 && !details_before_hero; ++i) {
    ww::SteamArtwork part;
    if (service.Take(111, part) && !part.description.empty()) details_before_hero = true;
    if (!details_before_hero) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(details_before_hero); // A slow image must not block store details.
  service.Request(222);
  bool switched_while_blocked = false;
  for (int i = 0; i < 1000 && !switched_while_blocked; ++i) {
    ww::SteamArtwork part;
    if (service.Take(222, part) && !part.hero.empty()) switched_while_blocked = true;
    if (!switched_while_blocked) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(switched_while_blocked && !release_first); // Obsolete requests must not hold the next selection.
  service.Request(111); release_first = true;
  ww::SteamArtwork artwork;
  bool first_received = false;
  for (int i = 0; i < 1000 && !first_received; ++i) {
    ww::SteamArtwork part;
    if (service.Take(111, part) && !part.hero.empty()) { artwork = std::move(part); first_received = true; }
    if (!first_received) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(first_received && artwork.hero == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1}));
  service.Request(222);
  artwork = {};
  for (int i = 0; i < 1000 && (artwork.hero.empty() || artwork.logo.empty() || artwork.description.empty()); ++i) {
    ww::SteamArtwork part;
    if (service.Take(222, part)) {
      if (!part.hero.empty()) artwork.hero = std::move(part.hero);
      if (!part.logo.empty()) artwork.logo = std::move(part.logo);
      if (!part.description.empty()) artwork.description = std::move(part.description);
      if (!part.genres.empty()) artwork.genres = std::move(part.genres);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(artwork.hero == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 2}) && artwork.logo == std::vector<std::uint8_t>({0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}));
  assert(artwork.description == "A & B" && artwork.genres == "Action");
  // Home clears the rendered copy, then reopening the same app asks again.
  // All detail channels must be fetched and delivered a second time.
  const int hero_before = reopened_hero, details_before = reopened_details, news_before = reopened_news;
  service.Request(222); artwork = {};
  for (int i = 0; i < 2000 && (artwork.hero.empty() || artwork.description.empty() || artwork.news_state == ww::SteamNewsState::pending); ++i) {
    ww::SteamArtwork part;
    if (service.Take(222, part)) {
      if (!part.hero.empty()) artwork.hero = std::move(part.hero);
      if (!part.description.empty()) artwork.description = std::move(part.description);
      if (part.news_state != ww::SteamNewsState::pending) artwork.news_state = part.news_state;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(!artwork.hero.empty() && artwork.description == "A & B" && artwork.news_state != ww::SteamNewsState::pending);
  assert(reopened_hero > hero_before && reopened_details > details_before && reopened_news > news_before);
  service.Request(333);
  bool recovered_details = false;
  for (int i = 0; i < 3000 && !recovered_details; ++i) {
    ww::SteamArtwork part;
    if (service.Take(333, part) && part.description == "A & B") recovered_details = true;
    if (!recovered_details) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(recovered_details && transient_details == 2);
  service.Request(444);
  for (int i = 0; i < 1000 && !permanent_details; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(permanent_details == 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  assert(permanent_details == 1); // A permanent response must not be retried.
  service.Request(555);
  bool recovered_hero = false, recovered_logo = false;
  for (int i = 0; i < 3000 && !(recovered_hero && recovered_logo); ++i) {
    ww::SteamArtwork part;
    if (service.Take(555, part)) {
      recovered_hero |= !part.hero.empty(); recovered_logo |= !part.logo.empty();
    }
    if (!(recovered_hero && recovered_logo)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(recovered_hero && recovered_logo && transient_hero == 2 && transient_logo == 2);
  service.Request(669);
  for (int i = 0; i < 1000 && !news_blocked; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(news_blocked); // The next selection must cancel a stalled news request.
  for (std::uint32_t app : {666u, 667u, 668u, 670u, 671u}) {
    service.Request(app); ww::SteamArtwork news;
    for (int i = 0; i < 1000 && news.news_state == ww::SteamNewsState::pending; ++i) {
      ww::SteamArtwork part;
      if (service.Take(app, part) && part.news_state != ww::SteamNewsState::pending) news = std::move(part);
      if (news.news_state == ww::SteamNewsState::pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (app == 666) assert(news.news_state == ww::SteamNewsState::available && news.news_title == "[AMA] Real update" && news.news_feed == "Official" && news.news_description == "Patch Notes fixes issues" && news.news_published == 30);
    if (app == 667) assert(news.news_state == ww::SteamNewsState::empty && news.news_title.empty());
    if (app == 668) assert(news.news_state == ww::SteamNewsState::unavailable && news.news_title.empty());
    if (app == 671) assert(news.news_state == ww::SteamNewsState::available && news.news_description == "Short text");
    if (app == 670) assert(news.news_title == std::string(191, 'a') && news.news_description == std::string(299, 'b'));
  }
  // 200 with a valid JPEG prefix can still be undecodable. UI rejection must
  // suppress repeated requests until cooldown expires, then allow recovery.
  std::vector<std::uint8_t> broken; bool received = false;
  service.RequestIcon(770, "");
  for (int i = 0; i < 1000 && !(received = service.TakeIcon(770, broken)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(received && broken == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1}));
  service.RejectIcon(770);
  for (int i = 0; i < 10; ++i) service.RequestIcon(770, "");
  std::this_thread::sleep_for(std::chrono::milliseconds(30)); assert(malformed_icon_calls == 1 && !service.TakeIcon(770, broken));
  fake_seconds = 61; service.RequestIcon(770, ""); received = false;
  for (int i = 0; i < 1000 && !(received = service.TakeIcon(770, broken)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(received && malformed_icon_calls == 1 && icon_fallback_calls == 1);
  fake_seconds = 0; service.RequestCover(771); received = false;
  for (int i = 0; i < 1000 && !(received = service.TakeCover(771, broken)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(received && broken == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1}));
  service.RejectCover(771);
  for (int i = 0; i < 10; ++i) service.RequestCover(771);
  std::this_thread::sleep_for(std::chrono::milliseconds(30)); assert(malformed_cover_calls == 1 && !service.TakeCover(771, broken));
  fake_seconds = 61; service.RequestCover(771); received = false;
  for (int i = 0; i < 1000 && !(received = service.TakeCover(771, broken)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(received && malformed_cover_calls == 1 && cover_fallback_calls == 1);
  const std::string valid_icon = "https://media.steampowered.com/steamcommunity/public/images/apps/222/0123456789abcdef0123456789abcdef01234567.jpg";
  service.RequestIcon(222, "https://example.test/not-steam.jpg");
  std::vector<std::uint8_t> icon; assert(!service.TakeIcon(222, icon));
  service.RequestIcon(222, valid_icon);
  bool icon_received = false;
  for (int i = 0; i < 1000 && !(icon_received = service.TakeIcon(222, icon)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(icon_received && icon == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 3}));
  service.RequestIcon(222, valid_icon); icon.clear(); icon_received = false;
  for (int i = 0; i < 1000 && !(icon_received = service.TakeIcon(222, icon)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(icon_received && icon == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 3}));
  service.RequestCover(222);
  std::vector<std::uint8_t> cover;
  bool cover_received = false;
  for (int i = 0; i < 1000 && !(cover_received = service.TakeCover(222, cover)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(cover_received && cover == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 2}));
  service.RequestCover(730); cover.clear(); cover_received = false;
  for (int i = 0; i < 1000 && !(cover_received = service.TakeCover(730, cover)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(cover_received && cover == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1})); // fallback hero
  const int headers_before = cs2_headers, heroes_before = cs2_heroes;
  service.RequestIcon(730, ""); icon.clear(); icon_received = false;
  for (int i = 0; i < 1000 && !(icon_received = service.TakeIcon(730, icon)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(icon_received && icon == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1}));
  assert(cs2_headers == headers_before + 1 && cs2_heroes == heroes_before + 1);
  service.RequestCover(731); cover.clear(); cover_received = false;
  for (int i = 0; i < 1000 && !(cover_received = service.TakeCover(731, cover)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(cover_received && cover == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1}));
  assert(bad_header_calls == 1 && bad_hero_calls == 1); // 200 HTML uses the image fallback.
  service.RequestIcon(553850, ""); icon.clear(); icon_received = false;
  for (int i = 0; i < 1000 && !(icon_received = service.TakeIcon(553850, icon)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(icon_received && icon == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 1})); // missing icon uses Steam header
  service.RequestCover(222); cover.clear(); cover_received = false;
  for (int i = 0; i < 1000 && !(cover_received = service.TakeCover(222, cover)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(cover_received && cover == std::vector<std::uint8_t>({0xff, 0xd8, 0xff, 2}));
  service.RequestPortrait(772); std::vector<std::uint8_t> portrait; bool portrait_received = false;
  for (int i = 0; i < 1000 && !(portrait_received = service.TakePortrait(772, portrait)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(portrait_received && portrait_primary_calls == 1 && portrait_fallback_calls == 0);
  service.RejectPortrait(772); service.RequestPortrait(772);
  assert(!service.TakePortrait(772, portrait) && portrait_primary_calls == 1);
  fake_seconds += 61; service.RequestPortrait(772); portrait_received = false;
  for (int i = 0; i < 1000 && !(portrait_received = service.TakePortrait(772, portrait)); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(portrait_received && portrait_fallback_calls == 1); // corrupt primary rotates after cooldown
  {
    ww::steam_integration::ArtworkService bulk("synthetic-ca");
    for (std::uint32_t app = 90000; app < 90129; ++app) {
      bulk.RequestIcon(app, "");
      const int expected = int(app - 90000 + 1);
      for (int i = 0; i < 1000 && bulk_icon_calls < expected; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      assert(bulk_icon_calls >= expected);
    }
    std::vector<std::uint8_t> latest;
    bool received_latest = false;
    for (int i = 0; i < 1000 && !(received_latest = bulk.TakeIcon(90128, latest)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received_latest && !latest.empty()); // New visible icons replace stale, unconsumed icons.
    std::vector<std::uint8_t> old; assert(!bulk.TakeIcon(90000, old));
    bulk.RequestIcon(90000, ""); bool received_old = false;
    for (int i = 0; i < 1000 && !(received_old = bulk.TakeIcon(90000, old)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received_old); // Eviction must also clear deduplication state.
  }
  first_entered = false; release_first = false;
  {
    ww::steam_integration::ArtworkService second("synthetic-ca"); second.Request(111);
    for (int i = 0; i < 1000 && !first_entered; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(first_entered); // Destruction cancels and joins this in-flight request.
  }
  // Results for cards that scrolled away must not permanently fill the buffer.
  {
    ww::steam_integration::ArtworkService saturated("synthetic-ca");
    for (unsigned app = 50000; app < 50064; ++app) saturated.RequestPortrait(app);
    for (int i = 0; i < 3000 && saturation_portraits < 64; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(saturation_portraits == 64);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    saturated.RequestPortrait(50100);
    std::vector<std::uint8_t> visible; bool received = false;
    for (int i = 0; i < 3000 && !(received = saturated.TakePortrait(50100, visible)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received && saturation_portraits == 65);
    unsigned evicted_portrait = 0;
    for (unsigned app = 50000; app < 50064; ++app) {
      visible.clear(); if (!saturated.TakePortrait(app, visible)) { evicted_portrait = app; break; }
    }
    assert(evicted_portrait);
    saturated.RequestPortrait(evicted_portrait); visible.clear(); received = false;
    for (int i = 0; i < 3000 && !(received = saturated.TakePortrait(evicted_portrait, visible)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received && saturation_portraits == 66);
    for (unsigned app = 51000; app < 51064; ++app) saturated.RequestCover(app);
    for (int i = 0; i < 3000 && saturation_covers < 64; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(saturation_covers == 64);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    saturated.RequestCover(51100); visible.clear(); received = false;
    for (int i = 0; i < 3000 && !(received = saturated.TakeCover(51100, visible)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received && saturation_covers == 65);
    unsigned evicted_cover = 0;
    for (unsigned app = 51000; app < 51064; ++app) {
      visible.clear(); if (!saturated.TakeCover(app, visible)) { evicted_cover = app; break; }
    }
    assert(evicted_cover);
    saturated.RequestCover(evicted_cover); visible.clear(); received = false;
    for (int i = 0; i < 3000 && !(received = saturated.TakeCover(evicted_cover, visible)); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(received && saturation_covers == 66);
  }
  // A stream of portrait work must not starve a waiting landscape cover.
  {
    ww::steam_integration::ArtworkService fair("synthetic-ca");
    fair.RequestPortrait(60000); fair.RequestPortrait(60001);
    for (int i = 0; i < 3000 && fairness_blocked < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(fairness_blocked == 2);
    for (unsigned app = 60002; app <= 60010; ++app) fair.RequestPortrait(app);
    fair.RequestCover(61000); fairness_release = true;
    for (int i = 0; i < 3000 && !fairness_cover_started; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(fairness_cover_started && fairness_portraits_before_cover <= 1);
  }
}
