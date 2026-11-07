#include "steam/backend/cm_app_details.h"
#include <cassert>
#include <string>
using namespace playden::steam;
static AppDetails details(const char* a, const char* b, const char* c, const char* d) {
  const auto depot = [](const char* id, const char* gid, const char* size, const char* download) {
    return std::string("\"") + id + "\" { \"manifests\" { \"public\" { \"gid\" \"" + gid + "\" \"size\" \"" + size + "\" \"download\" \"" + download + "\" } } } ";
  };
  return DetailsFromAppInfo(440, "\"appinfo\" { \"config\" { \"launch\" { \"0\" { \"executable\" \"game.exe\" } } } \"depots\" { " + depot("441", "1", a, b) + depot("442", "2", c, d) + " } }");
}
int main() {
  auto complete = details("3000", "1200", "500", "200");
  assert(complete.install_bytes == 3500 && complete.download_bytes == 1400);
  auto missing_install = details("3000", "1200", "0", "200");
  assert(!missing_install.install_bytes && missing_install.download_bytes == 1400);
  auto missing_download = details("3000", "1200", "500", "0");
  assert(missing_download.install_bytes == 3500 && !missing_download.download_bytes);
  auto overflow_install = details("18446744073709551615", "10", "1", "20");
  assert(!overflow_install.install_bytes && overflow_install.download_bytes == 11);
  auto overflow_download = details("18446744073709551615", "18446744073709551615", "1", "1");
  assert(!overflow_download.install_bytes && !overflow_download.download_bytes);
  auto capped = details("100", "200", "500", "200");
  assert(capped.install_bytes == 600 && capped.download_bytes == 300);
}
