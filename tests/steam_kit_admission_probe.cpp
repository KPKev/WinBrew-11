// Local probe using the production kit admission and managed-file hashing code.
#include "runtime/kit_admission.h"
#include "runtime/kit_trust.h"
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const auto result = playden::runtime::VerifyInstalledKit(
      argv[1], playden::runtime::DevelopmentKitAdmission());
  std::cout << "status=" << static_cast<int>(result.status) << " reason=" << result.reason
            << " manifest=" << result.manifest_sha256 << '\n';
  return result.status == playden::runtime::KitManifestStatus::kVerified ? 0 : 1;
}
