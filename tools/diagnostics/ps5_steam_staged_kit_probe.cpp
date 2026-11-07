// Isolated read-only kit admission probe; does not install or launch games.
#include "runtime/kit_admission.h"
#include "runtime/kit_trust.h"
#include <cstdio>

int main() {
  std::puts("STAGED_KIT_PROBE start writes=none elevation=none");
  std::fflush(stdout);
  const auto result = playden::runtime::VerifyInstalledKit(
      "/data/wine11-kit-stage-0f803ada2eee/playden-kit",
      playden::runtime::DevelopmentKitAdmission());
  std::printf("STAGED_KIT_PROBE status=%d reason=%s manifest=%s\n",
              static_cast<int>(result.status), result.reason.c_str(), result.manifest_sha256.c_str());
  std::puts("STAGED_KIT_PROBE done");
  std::fflush(stdout);
  return 0;
}
