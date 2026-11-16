#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ww::steam_integration {
// Admit only the known original 0.13 Blender binding when the same recipe is
// retained and the pinned replacement introduces one compatibility receipt.
// The caller must still verify ownership and copy/hash the new files before
// committing the new binding.
template <class Artifact>
bool CanMigrateLegacyBlenderArtifacts(std::uint32_t app,
                                      const std::vector<Artifact>& old,
                                      const std::vector<Artifact>& wanted) {
  if (app != 365670 || old.size() != 2 || wanted.size() != 3) return false;
  const auto find_unique = [](const auto& list, const char* id) -> const Artifact* {
    const Artifact* found = nullptr;
    for (const auto& entry : list) if (entry.artifact_id == id) {
      if (found) return nullptr;
      found = &entry;
    }
    return found;
  };
  const auto* old_runtime = find_unique(old,"runtime");
  const auto* old_recipe = find_unique(old,"recipe");
  const auto* new_runtime = find_unique(wanted,"runtime");
  const auto* new_recipe = find_unique(wanted,"recipe");
  const auto* receipt = find_unique(wanted,"compatibility-title");
  if (!old_runtime || !old_recipe || !new_runtime || !new_recipe || !receipt) return false;
  return old_runtime->identity == "wine-wow64-031302d-kit-0.13" &&
         old_runtime->sha256 == "6f5d2365ad5553819a21a390edb57c739d3af99b2392b16141b7fc8c140a5adb" &&
         old_recipe->identity == new_recipe->identity && old_recipe->sha256 == new_recipe->sha256 &&
         ((new_runtime->identity == "wine-wow64-blender-019.1" && receipt->identity == "blender-019.1-title") ||
          (new_runtime->identity == "wine-wow64-blender-019.2" && receipt->identity == "blender-019.2-title")) &&
         new_runtime->sha256.size() == 64 && receipt->sha256.size() == 64;
}
// Replace only the previously shipped Blender receipt and identical runtime
// inventory. Any divergent recipe or missing/duplicate artifact is refused.
template <class Artifact>
bool CanMigrateBlenderInlineEditors(std::uint32_t app,
                                    const std::vector<Artifact>& old,
                                    const std::vector<Artifact>& wanted) {
  if (app != 365670 || old.size() != 3 || wanted.size() != 3) return false;
  const auto one=[](const auto& entries,const char* id) -> const Artifact* {
    const Artifact* out=nullptr;
    for(const auto& e:entries) if(e.artifact_id==id) { if(out) return nullptr; out=&e; }
    return out;
  };
  const auto *orun=one(old,"runtime"), *orecipe=one(old,"recipe"), *oreceipt=one(old,"compatibility-title");
  const auto *nrun=one(wanted,"runtime"), *nrecipe=one(wanted,"recipe"), *nreceipt=one(wanted,"compatibility-title");
  if(!orun || !orecipe || !oreceipt || !nrun || !nrecipe || !nreceipt) return false;
  return orun->identity=="wine-wow64-blender-019.1" && nrun->identity=="wine-wow64-blender-019.2" &&
         orun->sha256==nrun->sha256 && orun->sha256.size()==64 &&
         orecipe->identity==nrecipe->identity && orecipe->sha256==nrecipe->sha256 &&
         oreceipt->identity=="blender-019.1-title" &&
         oreceipt->sha256=="2191c6d66d0964fb3916cc797d992204729e3bcf3ac0db73f57a4630ef1f2644" &&
         nreceipt->identity=="blender-019.2-title" && nreceipt->sha256.size()==64;
}

}
