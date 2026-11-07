"""Generate the reviewed adaptive downloader overlay, without editing Playden."""
from pathlib import Path
import os
import sys

root = Path(__file__).resolve().parents[1]
source = (root / 'vendor/playden-source/src/steam/backend/depot_installer.cpp').read_text()

def replace(old, new):
    global source
    if source.count(old) != 1:
        raise SystemExit('Upstream downloader changed; refusing ambiguous adaptive tuning patch')
    source = source.replace(old, new, 1)


replace('#include "steam/backend/depot_installer.h"',
        '#include "steam/backend/depot_installer.h"\n#include "steam_integration/download_tuner.hpp"\n#include "steam_integration/network_traffic.hpp"\n#include "steam_integration/download_space_status.hpp"')
replace('  std::map<std::string, Placement> placements;', '''  // Refuse an ambiguous overwrite between explicitly different Windows CPU
  // architectures. Unique files from both remain allowed; untagged depots
  // retain the normal Steam merge behavior.
  const auto architecture = [&target, &manifests](std::size_t index) {
    for (const auto& depot : target.depots) {
      if (depot.id != manifests[index].depot_id) continue;
      const auto arch = Lower(depot.os_arch);
      if (arch == "32" || arch == "x86") return 32;
      if (arch == "64" || arch == "x64" || arch == "amd64") return 64;
      break;
    }
    return 0;
  };
  std::map<std::string, Placement> placements;''')
replace('        if (SameFile(*found->second.file, file)) continue;', '''        if (SameFile(*found->second.file, file)) continue;
        if (!file.is_directory() && !file.is_symlink() &&
            !found->second.file->is_directory() && !found->second.file->is_symlink()) {
          const int earlier_arch = architecture(found->second.depot);
          const int later_arch = architecture(depot);
          if (earlier_arch && later_arch && earlier_arch != later_arch) {
            log_merge("architecture-conflict", entry(found->second.depot, *found->second.file) + " | " + entry(depot, file));
            return fail(InstallError::kProtocol, ("Conflicting 32-bit and 64-bit game files: " + file.path).c_str());
          }
        }''')
if os.environ.get('WINE11_EXPERIMENT_RESUME_VERIFY') == '1':
    replace('#include "steam/backend/depot_installer.h"', '#include "steam/backend/depot_installer.h"\n#include "steam_integration/resume_chunk_verify.hpp"')
    replace('  // Space still needed: per file, the smaller of its unwritten chunks and its unallocated bytes.', '''  // Do not credit journal entries without checking their installed bytes.
  for (const auto& [key, placement] : placements) {
    if (cancel.load()) return fail(InstallError::kCanceled, "canceled");
    if (placement.file->is_directory() || placement.file->is_symlink()) continue;
    const auto depot = manifests[placement.depot].depot_id;
    for (const auto& chunk : placement.file->chunks) {
      if (cancel.load()) return fail(InstallError::kCanceled, "canceled");
      const auto entry = JournalKey(depot, placement.file->path, chunk.offset);
      if (!done.count(entry)) continue;
      const auto verified = ww::steam_integration::VerifyResumeChunk(target.install_root, placement.file->path,
                                                                     placement.file->size, chunk, cancel);
      if (verified == ww::steam_integration::ResumeChunk::canceled)
        return fail(InstallError::kCanceled, "canceled");
      if (verified == ww::steam_integration::ResumeChunk::unavailable)
        return fail(InstallError::kStorage, "cannot verify resumed download");
      if (verified == ww::steam_integration::ResumeChunk::repair) done.erase(entry);
    }
  }
  // Space still needed: per file, the smaller of its unwritten chunks and its unallocated bytes.''')
replace('  InstallOutcome outcome;\n  InstallStats stats;',
        '  auto traffic_counter = ww::steam_integration::traffic::capture();\n  InstallOutcome outcome;\n  InstallStats stats;')
replace('                return self->fetcher.fetch(servers[server_cursor % servers.size()], grant.depot_id, Hex(chunk.sha),',
        '                ww::steam_integration::traffic::Scope traffic_scope(traffic_counter);\n                return self->fetcher.fetch(servers[server_cursor % servers.size()], grant.depot_id, Hex(chunk.sha),')
# Preserve refusal when the query fails, but distinguish it from verified low space.
replace('std::uint64_t DefaultFreeBytes(const std::string& path) {',
        'thread_local bool default_space_query_failed = false;\nstd::uint64_t DefaultFreeBytes(const std::string& path) {')
replace('  return VolumeSpaceSupported() ? 0 : std::numeric_limits<std::uint64_t>::max();',
        '  if (VolumeSpaceSupported()) { default_space_query_failed = true; return 0; }\n  return std::numeric_limits<std::uint64_t>::max();')
replace('  const auto available_bytes = hooks_.free_bytes(target.install_root);',
        '  default_space_query_failed = false;\n  const auto available_bytes = hooks_.free_bytes(target.install_root);\n  if (default_space_query_failed) return fail(InstallError::kStorage, ww::steam_integration::kSpaceQueryUnavailable);')
replace('const int slots = std::clamp(tuning_.slots, 1, 8);',
        'const int slots = std::clamp(tuning_.slots, 1, 20);')
replace('std::size_t next_chunk = 0, occupied = 0, waiting = 0;',
        '''std::size_t next_chunk = 0, occupied = 0, waiting = 0;
  ww::steam_integration::DownloadTuner auto_tuner;
  const bool adaptive = tuning_.slots == ww::steam_integration::DownloadTuner::Maximum;
  int active_limit = adaptive ? auto_tuner.slots() : slots;
  stats.slots = active_limit;''')
replace('changed.wait(lock, [&] { return stop.load() || !self->busy; });',
        'changed.wait(lock, [&] { return stop.load() || (!self->busy && occupied < static_cast<std::size_t>(active_limit)); });')
replace('    snapshot();\n    report_progress();\n    if (ready) {',
        '''    snapshot();
    report_progress();
    if (adaptive && !auto_tuner.finished()) {
      const auto now_ms = static_cast<std::uint64_t>(Milliseconds(Clock::now() - download_start));
      const bool was_finished = auto_tuner.finished();
      const bool changed_limit = auto_tuner.observe(now_ms, stats.compressed_bytes);
      if (changed_limit || (!was_finished && auto_tuner.finished())) {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "adaptive tested=%d bytes=%llu ms=%llu next=%d done=%d",
          auto_tuner.sampled_slots(), static_cast<unsigned long long>(auto_tuner.sampled_bytes()),
          static_cast<unsigned long long>(auto_tuner.sampled_ms()), auto_tuner.slots(), auto_tuner.finished() ? 1 : 0);
        trace(detail);
        std::lock_guard<std::mutex> lock(mutex);
        active_limit = auto_tuner.slots(); stats.slots = active_limit; changed.notify_all();
      }
    }
    if (ready) {''')
out = Path(sys.argv[1]) if len(sys.argv) == 2 else root / 'build/steam-native-experiment/src/steam_integration/tuned_depot_installer.cpp'
out.write_text(source, encoding='utf-8', newline='\n')
print('Generated bounded adaptive Steam downloader overlay')
