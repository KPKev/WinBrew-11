// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam/backend/library_client.h"
#include <cassert>
#include <cstdio>
using namespace playden;
using namespace playden::steam;
int main() {
  std::atomic<bool> cancel{false}; int calls=0;
  LibraryClient client(LibraryClientHooks{[&](const HttpRequest& request,const std::atomic<bool>&) {
    ++calls;
    assert(request.url.find("&include_played_free_games=true")!=std::string::npos);
    assert(request.url.find("&include_free_sub=true")!=std::string::npos);
    assert(request.url.find("&skip_unvetted_apps=false")!=std::string::npos);
    assert(request.url.find("access_token=synthetic%2Btoken")!=std::string::npos);
    const std::string body=calls==1
        ? R"({"response":{"game_count":1,"games":[{"appid":10,"name":"Old game"}]}})"
        : R"({"response":{"game_count":2,"games":[{"appid":10,"name":"Old game"},{"appid":20,"name":"New free game","playtime_forever":0}]}})";
    HttpResponse response; response.status_code=200; response.body.assign(body.begin(),body.end()); return response;
  }});
  StoredSession session{42,"Player","refresh","synthetic+token",{}};
  auto result=client.Fetch(session,cancel);
  assert(result.error==LibraryError::kNone && result.games.size()==1);
  result=client.Fetch(session,cancel);
  assert(result.error==LibraryError::kNone && result.games.size()==2 && calls==2);
  assert(result.games.back().id==GameId::Steam(20) && result.games.back().playtime_minutes==0);
  cancel=true; assert(client.Fetch(session,cancel).error==LibraryError::kCanceled && calls==2);
  std::puts("PASS: inclusive Steam query, token encoding, fresh rows, unplayed games, cancellation");
}
