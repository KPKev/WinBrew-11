// Host-only experiment: deliberately not wired into runtime or packaging.
#include "experimental_retained_ledger.hpp"
#include "platform/ps5/title_allow_list.h"
#include <charconv>
#include <string>

namespace {
constexpr char name[] = "host-retained-downloads.v1";
constexpr std::size_t limit = 65536;
std::string Encode(const ExperimentalRetainedLedger::Entries& entries) {
  std::string result = "retained 2\n";
  for (const auto& [app, context] : entries)
    result += std::to_string(app) + " " + std::to_string(context.account) + " " +
      std::to_string(context.volume) + " " + std::to_string(context.volume_generation) + " " +
      std::to_string(context.attempt) + "\n";
  return result;
}
}
ExperimentalRetainedLedger::ExperimentalRetainedLedger(const playden::platform::ManagedDirectory& state,
    const playden::platform::ManagedFileLock& lease)
    : state_(state), lease_(lease), storage_(state.path().string()) {}
bool ExperimentalRetainedLedger::Valid() const {
  return state_.Validate() && lease_.Validate(state_, "wine11-state.lock");
}
std::optional<ExperimentalRetainedLedger::Entries> ExperimentalRetainedLedger::ReadLocked() {
  if (!Valid()) return {};
  auto read = storage_.ReadDurable(name, limit);
  if (!read.ok() || !Valid()) return {};
  std::string text(read.value().begin(), read.value().end());
  if (text.compare(0, 11, "retained 2\n") != 0) return {};
  Entries entries;
  std::size_t pos = 11;
  while (pos < text.size()) {
    const auto end = text.find('\n', pos);
    if (end == std::string::npos) return {};
    std::uint64_t values[5]{};
    for (int i = 0; i < 5; ++i) {
      const auto separator = i == 4 ? end : text.find(' ', pos);
      if (separator == std::string::npos || separator > end || separator == pos) return {};
      const auto parsed = std::from_chars(text.data() + pos, text.data() + separator, values[i]);
      if (parsed.ec != std::errc{} || parsed.ptr != text.data() + separator || !values[i]) return {};
      pos = separator + 1;
    }
    if (values[0] > UINT32_MAX || !entries.emplace(static_cast<std::uint32_t>(values[0]),
        Context{values[1], values[2], values[3], values[4]}).second) return {};
    pos = end + 1;
  }
  if (Encode(entries) != text) return {}; // canonical ordering and spelling
  return entries;
}
std::optional<ExperimentalRetainedLedger::Entries> ExperimentalRetainedLedger::Read() {
  const auto uid = playden::ps5::HoldEffectiveUid(); return ReadLocked();
}
bool ExperimentalRetainedLedger::Initialize() {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!Valid()) return false;
  const auto text = Encode({});
  if (!storage_.WriteDurableExclusive(name, playden::Bytes(text.begin(), text.end())).ok()) return false;
  const auto result = ReadLocked(); return result && result->empty();
}
bool ExperimentalRetainedLedger::Retain(std::uint32_t app, const Context& context) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!app || !context.account || !context.volume || !context.volume_generation || !context.attempt) return false;
  auto entries = ReadLocked(); if (!entries) return false;
  auto [it, inserted] = entries->emplace(app, context);
  if (!inserted) return it->second == context;
  const auto text = Encode(*entries); if (text.size() > limit || !Valid()) return false;
  if (!storage_.WriteDurable(name, playden::Bytes(text.begin(), text.end())).ok()) return false;
  auto verified = ReadLocked(); return verified && *verified == *entries;
}
