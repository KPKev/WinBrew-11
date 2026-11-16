// Reuse upstream fixtures and unaffected cases against our generated backend.
#include "../build/vapor-library-refresh/upstream_entitled_library_fixtures.hpp"

namespace {
void MissingMetadataIsRetried() {
  for (int variant=0;variant<4;++variant) {
    Owned owned; Acquisitions acquisitions;
    acquisitions.result.package_apps={{200,{2000}},{300,{3000}}};
    // Confirmed DLC remains cached; incomplete game metadata must be retried.
    acquisitions.app_info[3000]=AppInfoText(3000,"DLC","dlc");
    if (variant==1) acquisitions.app_info[2000]="not app info";
    if (variant==2) acquisitions.app_info[2000]=AppInfoText(2000,"app_2000","game");
    if (variant==3) acquisitions.app_info[2000]=AppInfoText(2000,"New game","");
    EntitledLibraryBackend backend(owned,acquisitions); std::atomic<bool> cancel{false};
    assert(backend.Fetch(Session(),cancel).games.size()==2);
    acquisitions.app_info[2000]=AppInfoText(2000,"Newly added game","game");
    auto result=backend.Fetch(Session(),cancel);
    assert(result.games.size()==3 && result.games.back().title=="Newly added game");
    assert((acquisitions.app_info_requests.back()==std::vector<std::uint32_t>{2000}));
    result=backend.Fetch(Session(),cancel);
    assert(result.games.size()==3 && acquisitions.app_info_requests.size()==2);
  }
}
void LicensedApplicationsAppearAndIncompleteMetadataRetries() {
  Owned owned; Acquisitions acquisitions;
  acquisitions.result.package_apps={{200,{365670,1234}}};
  acquisitions.app_info[365670]=AppInfoText(365670,"app_365670","Application");
  acquisitions.app_info[1234]=AppInfoText(1234,"Tool","Tool");
  EntitledLibraryBackend backend(owned,acquisitions); std::atomic<bool> cancel{false};
  assert(backend.Fetch(Session(),cancel).games.size()==2);
  acquisitions.app_info[365670]=AppInfoText(365670,"Blender","Application");
  auto result=backend.Fetch(Session(),cancel);
  assert(result.games.size()==3 && result.games.back().id==SteamId(365670));
  assert((acquisitions.app_info_requests.back()==std::vector<std::uint32_t>{365670}));
}
void BundleGamesAreNotOmitted() {
  Owned owned; Acquisitions acquisitions;
  acquisitions.result.package_apps={{100,{10,365670,11}}};
  acquisitions.app_info[365670]=AppInfoText(365670,"Blender","game");
  acquisitions.app_info[11]=AppInfoText(11,"Extra DLC","dlc");
  EntitledLibraryBackend backend(owned,acquisitions); std::atomic<bool> cancel{false};
  const auto result=backend.Fetch(Session(),cancel);
  assert(result.games.size()==3 && result.games.back().id==SteamId(365670));
}
void NewLicensesAppearOnRefresh() {
  Owned owned; Acquisitions acquisitions;
  EntitledLibraryBackend backend(owned,acquisitions); std::atomic<bool> cancel{false};
  assert(backend.Fetch(Session(),cancel).games.size()==2);
  acquisitions.result.package_apps[200]={2000};
  acquisitions.app_info[2000]=AppInfoText(2000,"New license","game");
  const auto result=backend.Fetch(Session(),cancel);
  assert(result.games.size()==3 && result.games.back().id==SteamId(2000));
}
}
int main() {
  MissingMetadataIsRetried(); NewLicensesAppearOnRefresh(); BundleGamesAreNotOmitted(); LicensedApplicationsAppearAndIncompleteMetadataRetries();
  FamilySharedAppsAreMarked(); OwnedAndBorrowedIsNotShared(); SharedAppsLeftOutAreLogged();
  SharedAppInfoFailureIsRetried(); FamilyServiceIsAskedOncePerRun(); FamilyServiceNeverFailsTheLoad();
  FamilyWithoutLicensesIsAskedAgain(); StalledFamilyServiceHoldsNothingUp(); LateFamilyAnswerIsLogged();
  FamilyAppLinesAreCapped(); CmFailureIsLoggedForFamily(); LicensedDemosAreAddedAndMarked();
  DatesMergeIntoOwnedGames(); CmFailureIsNeverFatal(); OwnedFailureStopsTheCmSession();
  CancelReachesARunningCmSession();
  std::puts("PASS: new licenses, retryable metadata, cached DLC, demos, family, cancellation");
}
