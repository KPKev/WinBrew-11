#define main upstream_package_installer_tests
#include "runtime_package_installer_test.cpp"
#undef main
#include "blender_runtime_files.hpp"

GameRuntimeOverride BlenderFixture(Fixture& f) {
  GameRuntimeOverride r{365670,f.temporary.path/"blender-runtime","blender-019.1",RealHashBytes("receipt"),{},{}};
  for (const auto& [path,bytes] : std::vector<std::pair<std::string,std::string>>{
    {"runtime-inventory.json","blender-inventory"},{"lib/wine/x86_64-unix/ntdll.prx","matched-ntdll"},
    {"lib/wine/x86_64-unix/win32u.prx","cursor-module"},
    {"title/eboot.bin","portable-blender-launcher"},{"title/vapor-compatibility.json","receipt"},
    {"title/win/mesa-zink/x86_64-windows/opengl32.dll","mesa"}}) {
    Write(r.root/path,bytes);
    auto file=KitFile{path,bytes.size(),RealHashBytes(bytes),0644};
    if(path.rfind("title/",0)==0)r.title_files.push_back(file);else r.files.push_back(file);
  }
  return r;
}
int main(int argc,char**argv) {
  for(int cycle=0;cycle<2;cycle++) {
    Fixture f;auto r=Record();r.game=playden::GameId::Steam(365670);r.runtime_number=365670;
    auto installer=f.Installer();installer.SetGameRuntimeOverride(BlenderFixture(f));
    r.artifacts=installer.ExpectedArtifacts(r);
    CHECK(r.artifacts.size()==f.view.artifacts.size()+1);
    for(int repeat=0;repeat<2;repeat++) {
      CHECK(installer.StageTitle(r).state==PackageActionState::kComplete);
      CHECK(Read(f.host_data/"homebrew/FAKE78001/eboot.bin")=="portable-blender-launcher");
      CHECK(Read(f.host_data/"homebrew/FAKE78001/win/wine/lib/wine/x86_64-unix/win32u.prx")=="cursor-module");
      CHECK(Read(f.host_data/"homebrew/FAKE78001/vapor-compatibility.json")=="receipt");
      CHECK(installer.CopyTemplate(r).state==PackageActionState::kComplete);
      CHECK(installer.ValidateArtifacts(r).state==PackageActionState::kComplete);
      std::filesystem::remove_all(f.host_data/"homebrew/FAKE78001"); // fixture only
    }
    auto other=Record();other.runtime_number=730;other.game=playden::GameId::Steam(730);
    CHECK(installer.ExpectedArtifacts(other)[0].identity=="rev");
    other.runtime_number=365670;other.game=playden::GameId::Local("12345678-1234-1234-1234-123456789abc");
    CHECK(installer.ExpectedArtifacts(other)[0].identity=="rev");
  }
  {
    Fixture f;auto installer=f.Installer();auto r=Record();r.runtime_number=365670;
    std::filesystem::create_directories(f.host_data);
    CHECK(installer.WriteTitleMetadata(r,R"({"titleId":"FAKE78001"})",std::nullopt).state==PackageActionState::kComplete);
    CHECK(installer.StageTitle(r).state==PackageActionState::kComplete);
    CHECK(installer.CopyTemplate(r).state==PackageActionState::kComplete);
    auto project=f.host_data/"playden/games/install-hl2/library/prefixes/game/drive_c/scene.blend";
    auto preference=f.host_data/"playden/games/install-hl2/library/profiles/user-choice.txt";
    Write(project,"owner-scene");Write(preference,"owner-preference");
    installer.SetGameRuntimeOverride(BlenderFixture(f));r.artifacts=installer.ExpectedArtifacts(r);
    CHECK(installer.RefreshTitle(r).state==PackageActionState::kComplete);
    CHECK(Read(project)=="owner-scene"&&Read(preference)=="owner-preference");
    CHECK(installer.ValidateArtifacts(r).state==PackageActionState::kComplete);
  }
  for(int fault=0;fault<6;fault++) {
    Fixture f;auto replacement=BlenderFixture(f);auto module=replacement.root/"title/eboot.bin";
    if(fault==0)std::filesystem::remove(module);
    if(fault==1)Write(module,"corrupt-launcher");
    if(fault==2){std::filesystem::remove(module);std::filesystem::create_symlink(f.kit/"runtime/wine/rev/lib/core.prx",module);}
    if(fault==3)replacement.title_files[0].path="title/game-launch.json";
    if(fault==4)replacement.manifest_sha256=std::string(64,'a');
    if(fault==5)replacement.title_files.push_back(replacement.title_files[0]);
    auto installer=f.Installer();installer.SetGameRuntimeOverride(replacement);auto r=Record();r.runtime_number=365670;
    r.artifacts=installer.ExpectedArtifacts(r);CHECK(installer.StageTitle(r).state!=PackageActionState::kComplete);
    CHECK(!std::filesystem::exists(f.host_data/"homebrew/FAKE78001"));
  }
  if(argc==2) {
    Fixture f;auto installer=f.Installer();auto r=Record();r.runtime_number=365670;r.game=playden::GameId::Steam(365670);
    installer.SetGameRuntimeOverride({365670,std::filesystem::path(argv[1]),"blender-019.1",
      ww::steam_integration::kBlenderRuntimeManifestSha256,ww::steam_integration::BlenderRuntimeFiles(),ww::steam_integration::BlenderTitleFiles()});
    r.artifacts=installer.ExpectedArtifacts(r);
    for(int repeat=0;repeat<2;repeat++) {
      CHECK(installer.StageTitle(r).state==PackageActionState::kComplete);
      CHECK(installer.CopyTemplate(r).state==PackageActionState::kComplete);
      CHECK(installer.ValidateArtifacts(r).state==PackageActionState::kComplete);
      std::filesystem::remove_all(f.host_data/"homebrew/FAKE78001");
    }
  }
  std::puts("PASS: Blender first install/reinstall, legacy refresh, save/settings preservation and source refusal");
}
