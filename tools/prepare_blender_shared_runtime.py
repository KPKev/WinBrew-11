"""Source overlays for automatic Blender sharing; no console operations."""
def once(s,a,b):
 if s.count(a)!=1:raise ValueError('Unexpected source: '+a[:100])
 return s.replace(a,b,1)
def package_source(s):
 s=once(s,'#include "runtime/package_installer.h"','#include "runtime/package_installer.h"\n#include "blender_shared_release_policy.hpp"')
 s=once(s,'if (!CompatibilityTitlePath(file.path) || !IsSafeRelative(file.path)', 'if ((!CompatibilityTitlePath(file.path) && !ww::steam_integration::BlenderSharedHelperFileAllowed(runtime,file)) || !IsSafeRelative(file.path)')
 return s
SYNC=r'''
template<class Records>static bool SyncBlenderSharedEnrollment(const Records& records) {
#if defined(PLAYDEN_PLATFORM_PS5) && PLAYDEN_PLATFORM_PS5
  std::string selected;
  bool valid=true;
  for(const auto& record:records){
    if(record.runtime_number!=365670 || GameOf(record.game,record.runtime_number).steam_app()!=std::optional<std::uint32_t>(365670) ||
       (record.state!=rt::PreparationState::kReady && record.state!=rt::PreparationState::kPreparing && record.state!=rt::PreparationState::kRegistering))continue;
    bool bound=false;
    for(const auto& a:record.artifacts)bound|=a.artifact_id=="compatibility-title" && a.identity=="blender-019.3-title" && a.sha256==ww::steam_integration::kBlenderSharedReleasePin;
    if(!bound)continue;
    if(!ww::steam_integration::BlenderSharedTitleId(record.title_id)||(!selected.empty()&&selected!=record.title_id)){valid=false;break;}
    selected=record.title_id;
  }
  if(!valid)selected.clear();
  const bool written=playden::ps5::WithRoot([&]{return ww::steam_integration::WriteBlenderSharedEnrollment("/data",selected);});
  return valid&&written;
#else
  (void)records;return true;
#endif
}
'''
def runtime_host(s):
 s=once(s,'#include "blender_artifact_migration.hpp"','#include "blender_artifact_migration.hpp"\n#include "blender_shared_upgrade.hpp"\n#include "blender_shared_enrollment.hpp"\n#include "platform/ps5/title_allow_list.h"')
 s=once(s,'const bool legacy_blender = ','const bool legacy_blender = ww::steam_integration::CanMigrateBlenderSharedRelease(target.runtime_number,target.artifacts,c.installer->ExpectedArtifacts(target)) || ')
 needle='void RuntimeHost::PublishInstalls(const Composition& composition, const std::string& only) {'
 s=once(s,needle,SYNC+'\n'+needle+'\n  const bool shared_ready=SyncBlenderSharedEnrollment(composition.repository.Snapshot());')
 s=once(s,'    installs.push_back(std::move(view));','    if(record.runtime_number==365670&&!shared_ready){view.runtime_current=false;view.failure="Blender shared-folder access could not be prepared.";}\n    installs.push_back(std::move(view));')
 s=once(s,'      return changed ? std::string{} : "This game is already prepared.";','      if(request.runtime_number==365670&&!SyncBlenderSharedEnrollment(c.repository.Snapshot()))return "Blender shared-folder access could not be prepared.";\n      return changed ? std::string{} : "This game is already prepared.";')
 s=once(s,'    if (current->state == rt::PreparationState::kReady) {\n      OpenGameFolders(c, *current);','    if (current->state == rt::PreparationState::kReady) {\n      if(request.runtime_number==365670&&!SyncBlenderSharedEnrollment(c.repository.Snapshot()))return "Blender shared-folder access could not be prepared.";\n      OpenGameFolders(c, *current);')
 return s
