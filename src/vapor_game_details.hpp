// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "steam_ui.hpp"
#include <cstdio>
#include <climits>
#include <cmath>
namespace ww {
// The clock belongs to the renderer: no worker polling or fake percentage is
// needed to make an indeterminate preparation visibly active.
constexpr std::uint32_t kVaporStatusGold = 0xff64c0ff;
struct VaporPlayFeedback {
  bool busy = false;
  std::uint32_t fill = 0xff2b9c66, rim = 0, halo = 0;
};
inline std::uint32_t vapor_mix_color(std::uint32_t a, std::uint32_t b, double amount) {
  std::uint32_t result = 0xff000000;
  for (int shift = 0; shift < 24; shift += 8) {
    const int from = int((a >> shift) & 255), to = int((b >> shift) & 255);
    result |= std::uint32_t(from + int((to - from) * amount)) << shift;
  }
  return result;
}
inline VaporPlayFeedback vapor_play_feedback(SteamGameActivity activity, std::uint64_t now_ms) {
  VaporPlayFeedback feedback;
  if (activity == SteamGameActivity::registration_required) feedback.fill = 0xffb5791a;
  feedback.busy = activity == SteamGameActivity::preparing || activity == SteamGameActivity::starting;
  if (!feedback.busy) return feedback;
  const double pulse = (1.0 - std::cos(double(now_ms % 1800) * 6.283185307179586 / 1800.0)) * 0.5;
  feedback.fill = vapor_mix_color(0xff2b9c66, 0xff49c98a, pulse);
  feedback.rim = vapor_mix_color(0xff479b68, 0xff89e5b8, pulse);
  feedback.halo = vapor_mix_color(0xff30261e, 0xff49c98a, 0.12 + pulse * 0.28);
  return feedback;
}
// Format facts only when Steam supplied the underlying value. The view decides
// when and where to display them.
struct VaporGameFacts {
  bool playtime_available = false, install_size_available = false, download_size_available = false;
  char playtime[48]{}, install_size[48]{}, download_size[48]{};
};
inline void vapor_format_size(char *out, std::size_t capacity, std::uint64_t bytes) {
  constexpr double kib = 1024.0, mib = kib * kib, gib = mib * kib, tib = gib * kib;
  if (double(bytes) >= tib) std::snprintf(out, capacity, "%.2f TiB", double(bytes) / tib);
  else if (double(bytes) >= gib) std::snprintf(out, capacity, "%.2f GiB", double(bytes) / gib);
  else if (double(bytes) >= mib) std::snprintf(out, capacity, "%.1f MiB", double(bytes) / mib);
  else if (double(bytes) >= kib) std::snprintf(out, capacity, "%.1f KiB", double(bytes) / kib);
  else std::snprintf(out, capacity, "%llu B", static_cast<unsigned long long>(bytes));
}
inline VaporGameFacts vapor_game_facts(const SteamGame &game) {
  VaporGameFacts facts;
  if (game.has_playtime) {
    facts.playtime_available = true;
    std::snprintf(facts.playtime, sizeof(facts.playtime), "%llu h %llu min",
      static_cast<unsigned long long>(game.playtime_minutes / 60),
      static_cast<unsigned long long>(game.playtime_minutes % 60));
  }
  if (game.has_install_bytes) {
    facts.install_size_available = true;
    facts.install_size[0] = '~';
    vapor_format_size(facts.install_size + 1, sizeof(facts.install_size) - 1, game.install_bytes);
  }
  if (game.has_download_bytes) {
    facts.download_size_available = true;
    facts.download_size[0] = '~';
    vapor_format_size(facts.download_size + 1, sizeof(facts.download_size) - 1, game.download_bytes);
  }
  return facts;
}
// App-info depot totals are estimates; the amount already installed is not
// additional space required to install the game.
inline const char *vapor_install_size_label(const SteamGame &game) {
  return game.install == SteamInstallState::installed ? "INSTALL SIZE" : "SPACE REQUIRED";
}
// Keep a proposed byte cut at a UTF-8 character boundary. This is useful
// when fitting Steam text into a pixel width without splitting a code point.
inline std::size_t vapor_utf8_boundary(const std::string &text, std::size_t cut) {
  if (cut > text.size()) cut = text.size();
  while (cut && cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xc0) == 0x80) --cut;
  return cut;
}
inline std::string vapor_download_eta(const SteamDownload &job) {
  if (!job.has_eta) return {};
  if (job.eta_seconds == INT_MAX) return "Estimated time unavailable";
  char text[64];
  std::snprintf(text, sizeof(text), "Estimated %lld min remaining", (static_cast<long long>(job.eta_seconds) + 59) / 60);
  return text;
}
// Keep the state visible even when Steam supplies progress or detail text.
inline std::string vapor_download_row_status(const SteamDownload &job) {
  const char* label = job.state == SteamInstallState::running ? "Downloading"
    : job.state == SteamInstallState::queued ? "Queued"
    : job.state == SteamInstallState::paused ? "Paused"
    : job.state == SteamInstallState::installed ? "Completed" : "Failed";
  std::string line(label);
  if (job.total) {
    char done[32], total[32];
    vapor_format_size(done, sizeof(done), job.done);
    vapor_format_size(total, sizeof(total), job.total);
    line += "  "; line += done; line += " / "; line += total;
  }
  if (job.detail[0]) { line += "  -  "; line += job.detail; }
  return line;
}
// A download can belong to a different game or have an unknown total. Neither
// case should be shown as progress for the selected game's details.
struct VaporDownloadProgress {
  bool available = false;
  char text[128]{};
};
inline VaporDownloadProgress vapor_download_progress(std::uint32_t app, const SteamDownload &download) {
  VaporDownloadProgress progress;
  if (!app || download.app_id != app || !download.total ||
      (download.state != SteamInstallState::queued && download.state != SteamInstallState::running &&
       download.state != SteamInstallState::paused)) return progress;
  char done[32], total[32];
  vapor_format_size(done, sizeof(done), download.done);
  vapor_format_size(total, sizeof(total), download.total);
  const unsigned percent = download.done >= download.total ? 100u :
    static_cast<unsigned>((static_cast<double>(download.done) * 100.0) / download.total);
  std::snprintf(progress.text, sizeof(progress.text), "%s / %s (%u%%)", done, total, percent);
  progress.available = true;
  return progress;
}

}
