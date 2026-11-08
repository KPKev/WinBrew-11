#include "../src/vapor_game_details.hpp"
#include <cassert>
#include <cstring>
int main() {
  for (auto activity : {ww::SteamGameActivity::preparing, ww::SteamGameActivity::starting}) {
    const auto low = ww::vapor_play_feedback(activity, 0);
    const auto high = ww::vapor_play_feedback(activity, 900);
    const auto repeat = ww::vapor_play_feedback(activity, 1800);
    assert(low.busy && high.busy && low.fill != high.fill && low.halo != high.halo);
    assert(low.fill == repeat.fill && low.rim == repeat.rim);
  }
  for (auto activity : {ww::SteamGameActivity::idle, ww::SteamGameActivity::playing,
                        ww::SteamGameActivity::registration_required}) {
    const auto a = ww::vapor_play_feedback(activity, 0), b = ww::vapor_play_feedback(activity, 900);
    assert(!a.busy && !b.busy && a.fill == b.fill && !a.halo && !a.rim);
  }
  ww::SteamGame game;
  auto facts = ww::vapor_game_facts(game);
  assert(!facts.playtime_available && !facts.install_size_available && !facts.download_size_available);
  game.install=ww::SteamInstallState::paused;game.has_playtime=true;game.playtime_minutes=125;
  game.has_install_bytes=true;game.install_bytes=1536ull*1024*1024;
  game.has_download_bytes=true;game.download_bytes=524288;
  facts=ww::vapor_game_facts(game);
  assert(facts.playtime_available && !std::strcmp(facts.playtime,"2 h 5 min"));
  assert(facts.install_size_available && !std::strcmp(facts.install_size,"~1.50 GiB"));
  assert(facts.download_size_available && !std::strcmp(facts.download_size,"~512.0 KiB"));
  game.playtime_minutes=0;game.install_bytes=0;game.download_bytes=1024ull*1024*1024*1024;
  facts=ww::vapor_game_facts(game);
  assert(!std::strcmp(facts.playtime,"0 h 0 min") && !std::strcmp(facts.install_size,"~0 B") && !std::strcmp(facts.download_size,"~1.00 TiB"));
  const std::string unicode = "a\xf0\x9f\x8e\xae" "b";
  assert(ww::vapor_utf8_boundary(unicode, 2) == 1);
  assert(ww::vapor_utf8_boundary(unicode, 4) == 1);
  assert(ww::vapor_utf8_boundary(unicode, 5) == 5);
  game.install = ww::SteamInstallState::not_installed;
  assert(!std::strcmp(ww::vapor_install_size_label(game), "SPACE REQUIRED"));
  game.install = ww::SteamInstallState::installed;
  assert(!std::strcmp(ww::vapor_install_size_label(game), "INSTALL SIZE"));
  ww::SteamDownload dl;dl.app_id=42;dl.state=ww::SteamInstallState::running;dl.total=2048;dl.done=1024;
  auto progress=ww::vapor_download_progress(42,dl);
  assert(progress.available && !std::strcmp(progress.text,"1.0 KiB / 2.0 KiB (50%)"));
  assert(!ww::vapor_download_progress(43,dl).available);
  dl.total=0;assert(!ww::vapor_download_progress(42,dl).available);
  dl.total=2;dl.done=3;progress=ww::vapor_download_progress(42,dl);
  assert(progress.available && std::strstr(progress.text,"(100%)"));
  dl.state=ww::SteamInstallState::failed;assert(!ww::vapor_download_progress(42,dl).available);
  dl.state = ww::SteamInstallState::paused; dl.done = 1024; dl.total = 2048;
  std::snprintf(dl.detail, sizeof(dl.detail), "Awaiting network");
  assert(ww::vapor_download_row_status(dl) == "Paused  1.0 KiB / 2.0 KiB  -  Awaiting network");
  dl.state = ww::SteamInstallState::queued;
  assert(ww::vapor_download_row_status(dl).find("Queued  1.0 KiB / 2.0 KiB") == 0);
  dl.state = ww::SteamInstallState::failed; dl.total = 0;
  assert(ww::vapor_download_row_status(dl) == "Failed  -  Awaiting network");
  dl.has_eta = true; dl.eta_seconds = INT_MAX;
  assert(ww::vapor_download_eta(dl) == "Estimated time unavailable");
  dl.eta_seconds = 61; assert(ww::vapor_download_eta(dl) == "Estimated 2 min remaining");
  dl.has_eta = false; assert(ww::vapor_download_eta(dl).empty());
  dl.detail[0] = 0; dl.state = ww::SteamInstallState::installed;
  assert(ww::vapor_download_row_status(dl) == "Completed");

}
