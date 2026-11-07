// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
namespace ww::steam_integration {
// 20 selects calibration; 21 is the fixed-20 sentinel after calibration.
inline int ReadTunedSlots(const std::string& setting) {
  int result = 20;
  if (FILE* file = std::fopen(setting.c_str(), "r")) {
    int slots = 0; const bool ok = std::fscanf(file, "%d", &slots) == 1; std::fclose(file);
    if (ok && slots >= 5 && slots <= 20) result = slots == 20 ? 21 : slots;
  }
  return result;
}
inline void RecordTuning(const std::string& setting, const std::string& logfile,
                         std::uint32_t app, const char* step, const char* category) {
  if (!step || !category || std::strcmp(step, "trace") || std::strncmp(category, "adaptive ", 9)) return;
  int tested = 0, next = 0, done = 0; unsigned long long bytes = 0, ms = 0;
  if (std::sscanf(category, "adaptive tested=%d bytes=%llu ms=%llu next=%d done=%d", &tested, &bytes, &ms, &next, &done) != 5 ||
      tested < 5 || tested > 20 || next < 5 || next > 20 || !ms) return;
  if (FILE* log = std::fopen(logfile.c_str(), "a")) { std::fprintf(log, "app=%u %s\n", app, category); std::fclose(log); }
  if (done) {
    const auto temp = setting + ".tmp";
    if (FILE* file = std::fopen(temp.c_str(), "w")) {
      const bool wrote = std::fprintf(file, "%d\n", next) > 0;
      if (std::fclose(file) == 0 && wrote) std::rename(temp.c_str(), setting.c_str());
    }
  }
}
}
