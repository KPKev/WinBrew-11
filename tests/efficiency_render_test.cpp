#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
struct Library:ww::SteamUi {
  mutable unsigned reads=0;unsigned count=1000;bool renamed=false;
  void tick()override{} ww::SteamStatus status()const override{ww::SteamStatus s;s.account=ww::SteamAccount::online;s.library=ww::SteamLibrary::ready;s.game_count=count;return s;}
  bool game(std::size_t i,ww::SteamGame& g)const override{++reads;if(i>=count)return false;g={};g.app_id=i+1;std::snprintf(g.title,sizeof(g.title),"Game %05zu",i+1);if(renamed&&i==984)std::snprintf(g.title,sizeof(g.title),"Renamed game");g.install=ww::SteamInstallState::installed;g.has_last_played=true;g.last_played_unix_seconds=1700000000+i;return true;}
  std::uint32_t library_poll_interval_ms()const override{return 60000;}
  bool qr_module(int,int)const override{return false;}void sign_in()override{}void sign_out()override{}void cancel()override{}void refresh()override{}
};
int main(){namespace fs=std::filesystem;const auto root=fs::temp_directory_path()/("winbrew-efficiency-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));assert(fs::create_directory(root));ww::Shell s;assert(s.init("assets"));assert(s.vapor_shortcuts.load((root/"settings.ini").string().c_str()));
for(unsigned app=985;app<=1000;++app)assert(s.vapor_shortcuts.toggle(app,ww::VaporShortcuts::Favorite));Library l;s.steam=&l;s.draw();assert(l.reads==1000);assert(s.desktop_shortcut_labels[0]=="Game 00985");l.reads=0;s.draw();assert(l.reads==0);
s.activate(35);s.draw();l.reads=0;s.draw();assert(l.reads<100);assert(s.steam_recent_games.size()==1000);
++l.count;assert(s.vapor_shortcuts.toggle(1001,ww::VaporShortcuts::Favorite));s.draw();assert(s.desktop_shortcut_labels[16]=="Game 01001"&&s.steam_rows.size()==1001);
l.renamed=true;s.shortcut_labels_next_scan_ms=0;s.steam_rows_next_scan_ms=0;s.draw();assert(s.desktop_shortcut_labels[0]=="Renamed game");fs::remove_all(root);std::puts("PASS: shortcut labels use one traversal then zero warm scans; Recent cache, discovery and timed name refresh remain correct");}
