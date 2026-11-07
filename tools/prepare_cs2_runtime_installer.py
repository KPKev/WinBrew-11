"""Apply Vapor's per-game runtime policy to the pinned installer sources.

The base kit, bottle template, settings and every other game keep their existing
behavior. Overrides pass through the same hash-checked, durable package copier.
"""
from __future__ import annotations


def once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise ValueError("Unexpected pinned source: " + old[:100])
    return text.replace(old, new, 1)


def install_service_header(text: str) -> str:
    return once(text, "  virtual ~PackageInstaller() = default;", """  virtual ~PackageInstaller() = default;
  // Immutable identities selected for this game, including scoped compatibility runtimes.
  virtual std::vector<RuntimeArtifactIdentity> ExpectedArtifacts(const RuntimeInstall&) const { return {}; }""")


def package_header(text: str) -> str:
    text = once(text, "// The variant a title uses:", """// A reviewed, immutable runtime selected for exactly one Steam application.
// Files are pinned by the caller; every copied byte is checked by the installer.
struct GameRuntimeOverride {
  std::uint32_t steam_app_id = 0;
  std::filesystem::path root;
  std::string revision;
  std::string manifest_sha256;
  std::vector<KitFile> files;
};

// The variant a title uses:""")
    text = once(text, "  PackageStatus Inspect(const RuntimeInstall& record) override;", """  void SetGameRuntimeOverride(GameRuntimeOverride runtime);
  std::vector<RuntimeArtifactIdentity> ExpectedArtifacts(const RuntimeInstall& record) const override;
  PackageStatus Inspect(const RuntimeInstall& record) override;""")
    text = once(text, "  struct Mapping;", """  const GameRuntimeOverride* RuntimeOverrideFor(const RuntimeInstall& record) const;
  bool RuntimeOverrideValid(const GameRuntimeOverride& runtime) const;
  std::vector<KitArtifact> EffectiveArtifacts(const RuntimeInstall& record) const;
  std::string OwnerMarkerFor(const RuntimeInstall& record, const char* kind) const;
  struct Mapping;""")
    return once(text, "  VerifiedKitView kit_;", """  std::map<std::uint32_t, GameRuntimeOverride> runtime_overrides_;
  VerifiedKitView kit_;""")


POLICY = r'''
void FilesystemPackageInstaller::SetGameRuntimeOverride(GameRuntimeOverride runtime) {
  // Retain an invalid rule too: a missing/corrupt required runtime must never
  // silently fall back to a version known to fail for the selected game.
  const auto app = runtime.steam_app_id;
  runtime_overrides_[app] = std::move(runtime);
}

const GameRuntimeOverride* FilesystemPackageInstaller::RuntimeOverrideFor(const RuntimeInstall& record) const {
  std::uint32_t app = 0;
  if (record.game != GameId{}) {
    const auto steam = record.game.steam_app();
    if (!steam || *steam != record.runtime_number) return nullptr;
    app = *steam;
  } else {
    // Legacy Steam records predate GameId; their runtime number is the app id.
    app = record.runtime_number;
  }
  const auto found = runtime_overrides_.find(app);
  return found == runtime_overrides_.end() ? nullptr : &found->second;
}

bool FilesystemPackageInstaller::RuntimeOverrideValid(const GameRuntimeOverride& runtime) const {
  if (!runtime.steam_app_id || runtime.root.empty() || !runtime.root.is_absolute() ||
      runtime.revision.empty() || runtime.revision.size() > 128 ||
      !IsHexDigest(runtime.manifest_sha256) || runtime.files.empty() || runtime.files.size() > 8192)
    return false;
  if (!IsDirectoryNoSymlink(runtime.root) ||
      !PathComponentsHaveNoSymlink(runtime.root.root_path(), runtime.root.relative_path(), true)) return false;
  std::set<std::string> paths;
  bool inventory = false, native = false;
  for (const auto& file : runtime.files) {
    if (!IsSafeRelative(file.path) || !IsHexDigest(file.sha256) ||
        (file.mode != 0644 && file.mode != 0755) || !paths.insert(file.path).second) return false;
    inventory |= file.path == "runtime-inventory.json";
    native |= file.path == "lib/wine/x86_64-unix/ntdll.prx";
  }
  return inventory && native;
}

std::vector<KitArtifact> FilesystemPackageInstaller::EffectiveArtifacts(const RuntimeInstall& record) const {
  auto artifacts = kit_.artifacts;
  if (const auto* runtime = RuntimeOverrideFor(record)) {
    const auto inventory = std::find_if(runtime->files.begin(), runtime->files.end(),
        [](const KitFile& file) { return file.path == "runtime-inventory.json"; });
    if (inventory == runtime->files.end()) return {};
    const auto artifact = std::find_if(artifacts.begin(), artifacts.end(),
        [](const KitArtifact& value) { return value.artifact_id == "runtime"; });
    if (artifact == artifacts.end()) return {};
    artifact->identity = kit_.runtime.bundle_id + "-" + runtime->revision;
    artifact->sha256 = inventory->sha256;
    artifact->location = ImmutableArtifactLocation::kStagedTitle;
    artifact->path = "win/wine/runtime-inventory.json";
  }
  return artifacts;
}

std::vector<RuntimeArtifactIdentity> FilesystemPackageInstaller::ExpectedArtifacts(const RuntimeInstall& record) const {
  std::vector<RuntimeArtifactIdentity> result;
  for (const auto& artifact : EffectiveArtifacts(record))
    result.push_back({artifact.artifact_id, artifact.identity, artifact.sha256});
  return result;
}

std::string FilesystemPackageInstaller::OwnerMarkerFor(const RuntimeInstall& record, const char* kind) const {
  auto marker = OwnerMarker(record, kit_, kind);
  if (!std::strcmp(kind, "title")) if (const auto* runtime = RuntimeOverrideFor(record)) {
    const auto start = marker.find("\nruntime_revision=");
    const auto end = start == std::string::npos ? start : marker.find('\n', start + 1);
    if (end == std::string::npos) return {};
    marker.replace(start, end - start, "\nruntime_revision=" + runtime->revision);
    marker += "runtime_override=" + runtime->manifest_sha256 + "\n";
  }
  return marker;
}
'''


def package_source(text: str) -> str:
    text = once(text, '''std::string StageMarker(const RuntimeInstall& record, const VerifiedKitView& kit,
                        const std::string& kind) {
  return OwnerMarker(record, kit, kind.c_str());
}

''', '')
    text = once(text, "  std::shared_ptr<const KitFile> owned;", """  std::shared_ptr<const KitFile> owned;
  fs::path source_root{};  // empty means the admitted base kit""")
    text = once(text, "FilesystemPackageInstaller::FilesystemPackageInstaller(", POLICY + "\nFilesystemPackageInstaller::FilesystemPackageInstaller(")
    text = once(text, "  if (!ValidView() || !Variant(record) ||", """  const auto* replacement = RuntimeOverrideFor(record);
  if ((replacement && !RuntimeOverrideValid(*replacement)) || !ValidView() || !Variant(record) ||""")
    text = once(text, "  std::set<std::string> destinations;\n  for (const auto& file : kit_.files)", """  const auto* replacement = RuntimeOverrideFor(record);
  if (replacement && !RuntimeOverrideValid(*replacement)) return {};
  std::set<std::string> destinations;
  for (const auto& file : kit_.files)""")
    text = once(text, '    } else if (HasPrefix(file.path, kit_.runtime.path, &relative)) {\n      destination = "win/wine/" + relative;', '''    } else if (HasPrefix(file.path, kit_.runtime.path, &relative)) {
      if (replacement) continue;
      destination = "win/wine/" + relative;''')
    needle = "  return mappings;\n}\n\nstd::filesystem::path FilesystemPackageInstaller::TitleMetadataPath"
    text = once(text, needle, """  if (replacement) for (const auto& file : replacement->files) {
    const auto destination = "win/wine/" + file.path;
    if (!IsSafeRelative(file.path) || !destinations.insert(destination).second) return {};
    mappings.push_back({&file, file.path, destination, replacement->root / file.path, nullptr, replacement->root});
  }
  return mappings;
}

std::filesystem::path FilesystemPackageInstaller::TitleMetadataPath""")
    # Every source component retains no-symlink and exact mode/size checks.
    text = text.replace("PathComponentsHaveNoSymlink(kit_.root, mapping.source_relative, true)",
                        "PathComponentsHaveNoSymlink(mapping.source_root.empty() ? kit_.root : mapping.source_root, mapping.source_relative, true)")
    text = once(text, '    times.Add("copy", since);', '''    if (!mapping.source_root.empty()) {
      const auto copied = operations_.hash_file(destination);
      if (!copied || *copied != mapping.file->sha256)
        return Failed("copy-verification", "compatibility runtime digest does not match");
    }
    times.Add("copy", since);''')
    # Mark the selected immutable runtime without changing the mutable bottle's identity.
    for old, new in [
        ('OwnerMarker(record, kit_, "title")', 'OwnerMarkerFor(record, "title")'),
        ('OwnerMarker(record, kit_, "template")', 'OwnerMarkerFor(record, "template")'),
        ('OwnerMarker(record, kit_, kind)', 'OwnerMarkerFor(record, kind)'),
        ('OwnerMarker(record, kit_, stage_kind.c_str())', 'OwnerMarkerFor(record, stage_kind.c_str())'),
        ('StageMarker(record, kit_, stage_kind)', 'OwnerMarkerFor(record, stage_kind.c_str())')]:
        text = text.replace(old, new)
    # The replacement above must not make the member implementation recursive.
    text = once(text, '  auto marker = OwnerMarkerFor(record, kind);', '  auto marker = OwnerMarker(record, kit_, kind);')
    text = once(text, '             line.rfind("runtime_path=", 0) != 0 && line.rfind("variant_id=", 0) != 0)',
                '             line.rfind("runtime_path=", 0) != 0 && line.rfind("variant_id=", 0) != 0 && line.rfind("runtime_override=", 0) != 0)')
    # Runtime identities must agree in inspection and refresh as well as first setup.
    start = text.index("bool FilesystemPackageInstaller::ArtifactsMatch(")
    end = text.index("std::string FilesystemPackageInstaller::AdmissionDiagnostic", start)
    part = text[start:end].replace("  if (record.artifacts.size()", "  const auto artifacts = EffectiveArtifacts(record);\n  if (record.artifacts.size()", 1)
    part = part.replace("kit_.artifacts", "artifacts")
    text = text[:start] + part + text[end:]
    start = text.index("PackageActionResult FilesystemPackageInstaller::RefreshTitle(")
    end = text.index("PackageActionResult FilesystemPackageInstaller::CopyTemplate(", start)
    part = text[start:end].replace("  if (record.artifacts.size()", "  const auto artifacts = EffectiveArtifacts(record);\n  if (record.artifacts.size()", 1)
    part = part.replace("kit_.artifacts", "artifacts")
    part = once(part, '    const auto& source = mapping.source;\n    const int input', '''    const auto& source = mapping.source;
    bool source_mode_ok = false;
    std::error_code source_size_error;
    if (!mapping.source_root.empty() && (!IsRegularNoSymlink(source) ||
        !PathComponentsHaveNoSymlink(mapping.source_root.empty() ? kit_.root : mapping.source_root,
                                    mapping.source_relative, true) ||
        fs::file_size(source, source_size_error) != mapping.file->size || source_size_error ||
        FileMode(source, &source_mode_ok) != mapping.file->mode || !source_mode_ok))
      return Failed("kit-source-invalid", "runtime source differs from its pinned inventory");
    const int input''')
    text = text[:start] + part + text[end:]
    return text


def runtime_host(text: str) -> str:
    text = once(text, '#include "homebrew/runtime_host.hpp"', '#include "homebrew/runtime_host.hpp"\n#include "cs2_runtime_files.hpp"')
    text = once(text, '      installer->AcceptShippedRecipes(std::move(shipped));',
                '      ww::steam_integration::ConfigureCs2Runtime(*installer, config_.runtime_dir);\n      installer->AcceptShippedRecipes(std::move(shipped));')
    replacements = {
        'ArtifactsBound(composition.kit->view.artifacts, record)': 'ArtifactsBound(composition.installer->ExpectedArtifacts(record), record)',
        'ArtifactsBound(c.kit->view.artifacts, record)': 'ArtifactsBound(c.installer->ExpectedArtifacts(record), record)',
        'for (const auto& artifact : c.kit->view.artifacts)\n      current.push_back': 'for (const auto& artifact : c.installer->ExpectedArtifacts(record))\n      current.push_back',
        'target.artifacts.size() == c.kit->view.artifacts.size()': 'target.artifacts.size() == c.installer->ExpectedArtifacts(target).size()',
        'for (const auto& artifact : c.kit->view.artifacts) {': 'for (const auto& artifact : c.installer->ExpectedArtifacts(target)) {'}
    for old, new in replacements.items():
        text = once(text, old, new)
    return text
