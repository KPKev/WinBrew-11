#include "../src/steam_integration/title_absence.hpp"
#include <cassert>
using namespace ww::steam_integration;
int main() {
  TitleObservation observations[5] = {TitleObservation::absent, TitleObservation::absent, TitleObservation::absent,
                                      TitleObservation::absent, TitleObservation::absent};
  auto fn = [&](int i) { return [&, i](const std::string& title) { assert(title == "FAKE78008"); return observations[i]; }; };
  TitleAbsenceInputs inputs{fn(0), fn(1), fn(2), fn(3), fn(4)};
  assert(VerifyTitleAbsent("FAKE78008", inputs) == TitleObservation::absent);
  assert(VerifyTitleAbsent("FAKE77031", inputs) == TitleObservation::unknown);
  for (auto& observation : observations) {
    observation = TitleObservation::unknown; assert(VerifyTitleAbsent("FAKE78008", inputs) == TitleObservation::unknown);
    observation = TitleObservation::present; assert(VerifyTitleAbsent("FAKE78008", inputs) == TitleObservation::present);
    observation = TitleObservation::absent;
  }
  inputs.database = {}; assert(VerifyTitleAbsent("FAKE78008", inputs) == TitleObservation::unknown);
}
