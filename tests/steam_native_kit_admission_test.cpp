// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_kit_admission.hpp"
#include "runtime/kit_admission.h"
#include "runtime/kit_trust.h"
#include <cassert>
#include <cstdio>
namespace {
bool verified = false;
std::filesystem::path checked;
}
namespace playden::runtime {
KitAdmissionConfig DevelopmentKitAdmission() { KitAdmissionConfig config; config.expected_manifest_sha256 = "expected"; return config; }
KitManifestResult VerifyInstalledKit(const std::filesystem::path& path, const KitAdmissionConfig&) {
  checked = path; KitManifestResult result;
  if (verified) {
    result.status = KitManifestStatus::kVerified; result.view = VerifiedKitView{};
    result.manifest_sha256 = "actual"; result.sources["wrapper"]["revision"] = "revision";
  }
  return result;
}
}
int main() {
  auto admit = ww::steam_integration::InstalledKitAdmission(); std::string reason;
  assert(!admit(&reason) && !reason.empty());
  assert(checked == playden::runtime::kDevelopmentKitRoot);
  verified = true; auto kit = admit(&reason);
  assert(kit && reason.empty() && kit->manifest_sha256 == "actual" && kit->expected_manifest_sha256 == "expected");
  assert(kit->prospero_win_revision == "revision");
  std::puts("PASS: installed kit admission requires Playden's real verification result");
}
