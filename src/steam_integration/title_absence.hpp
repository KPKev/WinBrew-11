// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <string>
namespace ww::steam_integration {
enum class TitleObservation { absent, present, unknown };
struct TitleAbsenceInputs {
  std::function<TitleObservation(const std::string&)> runtime_record;
  std::function<TitleObservation(const std::string&)> database;
  std::function<TitleObservation(const std::string&)> shadowmount;
  std::function<TitleObservation(const std::string&)> homebrew_source;
  std::function<TitleObservation(const std::string&)> native_install;
};
// Must run on a worker: database and ShadowMount observations may block.
// Any unavailable or inconclusive source prevents an absence claim.
TitleObservation VerifyTitleAbsent(const std::string& title, const TitleAbsenceInputs& inputs);
}
