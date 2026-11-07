#include "../src/steam_integration/vapor_title_remover.hpp"
#include "platform/ps5/title_remover.h"
#include "runtime/artifact_hash.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
static int invoked = 0, sent = 0; static bool trusted = true;
namespace playden::runtime {
std::optional<std::string> HashArtifactBytes(std::string_view) {
  return trusted ? "b9f421393039a3d2f3160d2b7e792883fb9aeff80d3540a9e589068500a1faf9" : "bad";
}
}
namespace playden::ps5 {
bool SendToPayloadLoader(const std::string&, std::string*) { ++sent; return true; }
TitleRemoverOutcome RunTitleRemover(const TitleRemoverConfig& c, const std::string& title, const std::string& nonce) {
  ++invoked;
  assert(c.request_directory == "/data/wine11/wine11-games/vapor-title-remover");
  assert(title == "FAKE78008" && nonce == "12345678-1234-4123-8123-123456789abc");
  std::string error; bool ok = c.send("payload bytes", &error);
  return {ok, ok ? std::optional<int>(0) : std::nullopt, error};
}
}
int main() {
  using ww::steam_integration::MakeVaporTitleRemover;
  using ww::steam_integration::VerifyVaporTitleRemover;
  char directory[] = "/tmp/vapor-helper-XXXXXX"; assert(mkdtemp(directory));
  const auto path = std::filesystem::path(directory) / "helper.elf";
  { std::ofstream out(path, std::ios::binary); out << "\x7f" "ELF payload"; }
  assert(VerifyVaporTitleRemover(path.string()));
  const auto alias = std::filesystem::path(directory) / "app0";
  assert(symlink(directory, alias.c_str()) == 0);
  assert(VerifyVaporTitleRemover((alias / "helper.elf").string()));
  std::string reason;
  trusted = false; assert(!VerifyVaporTitleRemover(path.string(), &reason));
  assert(reason == "Removal helper verification failed"); trusted = true;
  const auto link = std::filesystem::path(directory) / "link"; assert(symlink(path.c_str(), link.c_str()) == 0);
  assert(!VerifyVaporTitleRemover(link.string(), &reason));
  assert(reason.find("Removal helper open failed") == 0);
  { std::ofstream out(path, std::ios::binary | std::ios::trunc); out << "not an ELF"; }
  assert(!VerifyVaporTitleRemover(path.string()));
  std::filesystem::remove_all(directory);
  auto invalid = MakeVaporTitleRemover("/tmp/other.elf");
  assert(!invalid("FAKE78008", "12345678-1234-4123-8123-123456789abc").sent && invoked == 0);
  auto run = MakeVaporTitleRemover(ww::steam_integration::kVaporTitleRemoverPath);
  assert(!run("FAKE77031", "12345678-1234-4123-8123-123456789abc").sent && invoked == 0);
  assert(!run("FAKE78008", "wrong").sent && invoked == 0);
  trusted = false; assert(!run("FAKE78008", "12345678-1234-4123-8123-123456789abc").sent && sent == 0);
  trusted = true; auto ok = run("FAKE78008", "12345678-1234-4123-8123-123456789abc");
  assert(ok.sent && ok.code && *ok.code == 0 && sent == 1);
}
