// Read-only verification of every staged kit file against the pinned manifest.
#include "runtime/artifact_hash.h"
#include "steam/backend/json.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
  namespace fs = std::filesystem;
  namespace rt = playden::runtime;
  namespace json = playden::steam::json;
  const fs::path root = "/data/wine11-kit-stage-0f803ada2eee/playden-kit";
  constexpr auto pinned = "1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc";
  std::puts("STAGED_HASH_PROBE start writes=none elevation=none"); std::fflush(stdout);
  auto manifest_hash = rt::HashArtifactFile(root / "manifest.json");
  if (!manifest_hash || *manifest_hash != pinned) { std::puts("STAGED_HASH_PROBE failure=manifest"); return 1; }
  std::ifstream input(root / "manifest.json", std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(input)), {});
  json::Limits limits; limits.max_bytes = 8 * 1024 * 1024; limits.max_values = 200000; limits.max_string_bytes = 8 * 1024 * 1024;
  auto document = json::Parse(bytes, limits);
  const auto* entries = document ? document->Find("files") : nullptr;
  if (!entries || !entries->is_array() || entries->array().size() != 4348) {
    std::puts("STAGED_HASH_PROBE failure=inventory"); return 1;
  }
  std::size_t checked = 0;
  for (const auto& entry : entries->array()) {
    const auto* name = entry.Find("path"); const auto* expected = entry.Find("sha256");
    if (!name || !name->is_string() || !expected || !expected->is_string()) {
      std::puts("STAGED_HASH_PROBE failure=entry"); return 1;
    }
    const fs::path relative(name->text());
    if (relative.empty() || relative.is_absolute() || relative != relative.lexically_normal()) {
      std::puts("STAGED_HASH_PROBE failure=path"); return 1;
    }
    for (const auto& part : relative) if (part == ".." || part == ".") {
      std::puts("STAGED_HASH_PROBE failure=path"); return 1;
    }
    auto actual = rt::HashArtifactFile(root / relative);
    if (!actual || *actual != expected->text()) {
      std::printf("STAGED_HASH_PROBE failure=content index=%zu\n", checked); std::fflush(stdout); return 1;
    }
    if (++checked % 512 == 0) { std::printf("STAGED_HASH_PROBE checked=%zu\n", checked); std::fflush(stdout); }
  }
  std::printf("STAGED_HASH_PROBE done verified=%zu\n", checked); std::fflush(stdout);
  return 0;
}
