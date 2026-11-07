// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real durable OWNER journal across Vapor restarts.
#include "runtime_host_fixture.h"

using namespace playden::homebrew;
using namespace playden::runtime;
using namespace playden::test::runtime_host;
namespace fs = std::filesystem;

void Restart(bool registered, NativeActivity activity) {
  Fixture fixture;
  fixture.registers = false;
  auto config = fixture.Config();
  config.host_namespace_prefix = fixture.root;
  config.package_paths.console_data_root = "/data";
  config.package_paths.host_data_root = fixture.root / "data";
  config.package_operations.hash_file = HashArtifactFile;
  config.package_operations.hash_bytes = HashArtifactBytes;
  config.package_operations.free_space_unverifiable = true;
  config.package_operations.validate_bottle = [](const fs::path& bottle) {
    return fs::is_regular_file(bottle / "system.reg") && fs::is_regular_file(bottle / "user.reg");
  };
  auto kit = SyntheticKit(fixture.root / "kit");
  config.admit_kit = [&kit](std::string*) { return std::optional<RuntimeKit>(kit); };
  unsigned uuids = 0;
  config.uuid = [&uuids]() -> std::optional<std::string> {
    return std::string("00000000-0000-4000-8000-00000000000") + std::to_string(++uuids);
  };
  config.unix_time = [] { return std::int64_t{1760000000}; };
  const auto program = fixture.root / "data/homebrew/FAKE77031/games/220/hl2.exe";
  Write(program, "existing-game", 0644);
  const RuntimePrepareRequest request{220, "/data/homebrew/FAKE77031/games/220", "internal", "Half-Life 2"};
  {
    RuntimeHost host(config);
    CHECK(host.Start()); CHECK(WaitFor(host, Started).kit_available);
    host.Prepare(request);
    const auto waiting = WaitFor(host, [](const RuntimeSnapshot& s) {
      return s.prepares_handled && !s.preparing_app;
    });
    if (waiting.refusals.count(220)) std::fprintf(stderr, "%s\n", waiting.refusals.at(220).c_str());
    CHECK(!waiting.refusals.count(220));
    CHECK(waiting.installs.size() == 1 && waiting.installs[0].awaiting_registration);
    host.Shutdown();
  }
  const auto submissions = fixture.registry_mutations;
  CHECK(submissions > 0);
  fixture.registered = registered;
  fixture.activity->store(static_cast<int>(activity));
  // Settings selected while the old registration journal was still waiting
  // must apply immediately after registration is reconciled on the next start.
  config.settings_record = [](const playden::GameId&) -> std::optional<GameSettingsRecord> {
    GameSettingsRecord record;
    record.revision = (1ull << 32) + 1;
    record.overrides = {{"controller", "xinput"}, {"controller_preset", "none"}};
    return record;
  };
  {
    RuntimeHost host(config);
    CHECK(host.Start());
    const auto restarted = WaitFor(host, Started);
    CHECK(restarted.kit_available && restarted.installs.size() == 1);
    const auto& install = restarted.installs[0];
    if (registered && activity == NativeActivity::kStopped) {
      CHECK(install.state == PreparationState::kReady);
      CHECK(!install.awaiting_registration && !install.recovery_pending && install.admission == "ok");
      CHECK(install.settings_revision == (1ull << 32) + 1);
      const auto profile = fixture.root / "data/playden/games/steam-220/library/profiles/playden-steam-220.profile";
      CHECK(Read(profile).find("mode = xinput") != std::string::npos);
    } else {
      CHECK(install.awaiting_registration && install.recovery_pending);
      CHECK(install.state != PreparationState::kReady);
    }
    // Startup may query an armed registration, but cannot submit it again.
    CHECK(fixture.registry_mutations == submissions);
    CHECK(Read(program) == "existing-game");
    host.Shutdown();
  }
}

int main(int argc, char** argv) {
  CHECK(argc == 2);
  g_runtime_dir = argv[1];
  Restart(false, NativeActivity::kStopped);
  Restart(true, NativeActivity::kStopped);
  Restart(true, NativeActivity::kActive);
  Restart(true, NativeActivity::kUnknown);
  std::puts("PASS: registered titles become Ready after restart; pending/active/unknown stay blocked; no repeat submission; newer XInput settings publish");
}
