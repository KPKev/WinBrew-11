// Exercise the production copier and pinned upstream fixture, including real SHA256.
#define main upstream_package_installer_tests
#include "runtime_package_installer_test.cpp"
#undef main
#include "cs2_runtime_files.hpp"

GameRuntimeOverride Replacement(Fixture& fixture) {
  GameRuntimeOverride value{730, fixture.temporary.path / "fixed-runtime", "cs2-019.1", std::string(64, 'b'), {}};
  for (const auto& [path, bytes] : std::vector<std::pair<std::string, std::string>>{
           {"runtime-inventory.json", "fixed-inventory"},
           {"lib/wine/x86_64-unix/ntdll.prx", "fixed-ntdll"}}) {
    Write(value.root / path, bytes);
    value.files.push_back({path, bytes.size(), RealHashBytes(bytes), 0644});
  }
  return value;
}

int main(int argc, char** argv) {
  // Existing package-copy, recovery and ownership tests remain applicable.
  upstream_package_installer_tests();
  for (int cycle = 0; cycle < 2; ++cycle) {
    Fixture fixture;
    auto record = Record(); record.runtime_number = 730; record.game = playden::GameId::Steam(730);
    auto replacement = Replacement(fixture);
    auto installer = fixture.Installer(); installer.SetGameRuntimeOverride(replacement);
    record.artifacts = installer.ExpectedArtifacts(record);
    CHECK(record.artifacts[0].identity == "wine-wow64-cs2-019.1");
    for (int install = 0; install < 2; ++install) {
      CHECK(installer.StageTitle(record).state == PackageActionState::kComplete);
      const auto title = fixture.host_data / "homebrew/FAKE78001";
      CHECK(Read(title / "win/wine/lib/wine/x86_64-unix/ntdll.prx") == "fixed-ntdll");
      CHECK(!std::filesystem::exists(title / "win/wine/lib/core.prx"));
      CHECK(installer.CopyTemplate(record).state == PackageActionState::kComplete);
      CHECK(installer.ValidateArtifacts(record).state == PackageActionState::kComplete);
      std::filesystem::remove_all(title); // isolated fixture only: simulate removing then reinstalling
    }
    // A different Steam game continues using its original runtime.
    auto other = Record();
    CHECK(installer.ExpectedArtifacts(other)[0].identity == "rev");
    auto local = other; local.runtime_number = 730;
    local.game = playden::GameId::Local("12345678-1234-1234-1234-123456789abc");
    CHECK(installer.ExpectedArtifacts(local)[0].identity == "rev");
    CHECK(installer.StageTitle(other).state == PackageActionState::kComplete);
    CHECK(Read(fixture.host_data / "homebrew/FAKE78001/win/wine/lib/core.prx") == "runtime");
  }
  {
    Fixture fixture; auto installer = fixture.Installer();
    auto record = Record(); record.runtime_number = 730;
    std::filesystem::create_directories(fixture.host_data);
    CHECK(installer.WriteTitleMetadata(record, R"({"titleId":"FAKE78001"})", std::nullopt).state == PackageActionState::kComplete);
    CHECK(installer.StageTitle(record).state == PackageActionState::kComplete);
    CHECK(installer.CopyTemplate(record).state == PackageActionState::kComplete);
    const auto save = fixture.host_data / "playden/games/install-hl2/library/prefixes/game/drive_c/custom-save";
    Write(save, "keep-my-save");
    installer.SetGameRuntimeOverride(Replacement(fixture));
    record.artifacts = installer.ExpectedArtifacts(record);
    const auto refresh = installer.RefreshTitle(record);
    CHECK(refresh.state == PackageActionState::kComplete);
    CHECK(Read(save) == "keep-my-save");
    CHECK(installer.ValidateArtifacts(record).state == PackageActionState::kComplete);
  }
  // Missing, corrupt and symlinked compatibility inputs cannot silently fall back.
  for (int fault = 0; fault < 3; ++fault) {
    Fixture fixture; auto replacement = Replacement(fixture);
    const auto module = replacement.root / "lib/wine/x86_64-unix/ntdll.prx";
    if (fault == 0) std::filesystem::remove(module);
    if (fault == 1) Write(module, "wrong-ntdll");
    if (fault == 2) { std::filesystem::remove(module); std::filesystem::create_symlink(fixture.kit / "runtime/wine/rev/lib/core.prx", module); }
    auto installer = fixture.Installer(); installer.SetGameRuntimeOverride(replacement);
    auto record = Record(); record.runtime_number = 730; record.artifacts = installer.ExpectedArtifacts(record);
    CHECK(installer.StageTitle(record).state != PackageActionState::kComplete);
    CHECK(!std::filesystem::exists(fixture.host_data / "homebrew/FAKE78001"));
  }
  std::puts("PASS: CS2 first install/reinstall, other game preservation, corrupt/missing/symlink refusal");
  if (argc == 2) {
    Fixture fixture; auto installer = fixture.Installer();
    installer.SetGameRuntimeOverride({730, std::filesystem::path(argv[1]), "cs2-019.1",
        ww::steam_integration::kCs2RuntimeManifestSha256, ww::steam_integration::Cs2RuntimeFiles()});
    auto record = Record(); record.runtime_number = 730; record.game = playden::GameId::Steam(730);
    record.artifacts = installer.ExpectedArtifacts(record);
    for (int cycle = 0; cycle < 2; ++cycle) {
      CHECK(installer.StageTitle(record).state == PackageActionState::kComplete);
      const auto title = fixture.host_data / "homebrew/FAKE78001";
      CHECK(*TestHash(title / "win/wine/lib/wine/x86_64-unix/ntdll.prx") ==
          "4f9f9fd45c65be0320794186f94bc2611be0d223199955a06d1e984cd295d94b");
      CHECK(installer.CopyTemplate(record).state == PackageActionState::kComplete);
      CHECK(installer.ValidateArtifacts(record).state == PackageActionState::kComplete);
      std::filesystem::remove_all(title); // temporary fixture, never the owner's title
    }
    std::puts("PASS: complete 1760-file release runtime first install and reinstall");
  }
}
