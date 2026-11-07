// Start only the already-installed, isolated Wine 11 Steam Test title.
#include "platform/ps5/title_runtime.h"
#include <cstdio>
#include <cstdint>
extern "C" {
int sceUserServiceInitialize(const void*);
int sceUserServiceGetForegroundUser(std::uint32_t*);
std::uint32_t sceSystemServiceLaunchApp(const char*, const char**, void*);
}
int main() {
  std::puts("WINE11_TEST_LAUNCH start title=PPSA77382"); std::fflush(stdout);
  (void)sceUserServiceInitialize(nullptr);
  playden::ps5::TitleApi api{};
  api.foreground_user = sceUserServiceGetForegroundUser;
  api.public_launch = sceSystemServiceLaunchApp;
  playden::ps5::TitleBackend launcher(api);
  auto result = launcher.Launch("PPSA77382");
  std::printf("WINE11_TEST_LAUNCH ok=%d code=0x%08x stage=%s\n", result.ok, result.code, result.stage.c_str());
  std::puts("WINE11_TEST_LAUNCH done"); std::fflush(stdout);
  return 0;
}
