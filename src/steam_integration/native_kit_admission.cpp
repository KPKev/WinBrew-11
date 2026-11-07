// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_kit_admission.hpp"
#include "runtime/kit_admission.h"
#include "runtime/kit_trust.h"
namespace ww::steam_integration {
std::function<std::optional<playden::homebrew::RuntimeKit>(std::string*)> InstalledKitAdmission() {
  return [](std::string* reason) -> std::optional<playden::homebrew::RuntimeKit> {
    namespace rt = playden::runtime;
    const auto policy = rt::DevelopmentKitAdmission();
    const auto result = rt::VerifyInstalledKit(rt::kDevelopmentKitRoot, policy);
    if (result.status != rt::KitManifestStatus::kVerified || !result.view) {
      if (reason) *reason = "The installed game runtime kit is missing or could not be verified.";
      return std::nullopt;
    }
    std::string revision;
    if (const auto wrapper = result.sources.find("wrapper"); wrapper != result.sources.end())
      if (const auto value = wrapper->second.find("revision"); value != wrapper->second.end()) revision = value->second;
    if (reason) reason->clear();
    return playden::homebrew::RuntimeKit{*result.view, result.capabilities, result.manifest_sha256,
                                         policy.expected_manifest_sha256, revision, result.source_dirty};
  };
}
}
