"""Correct Blender's overlapping Steam x86/x64 depot choice in the isolated runtime."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'vendor/playden-source/src/steam/backend/app_info.cpp').read_text()
old='''  if (eligible.empty()) {
    return Fail(reason, "This game has no downloadable Windows content for English.");
  }
  std::vector<DepotInfo> result;'''
new='''  // Blender's x86 depot contains an older complete build whose filenames
  // overlap its x64 depot. Installing both lets the x86 build win the merge.
  // Keep the 32-bit depot if the account cannot access the 64-bit one.
  if (app.app_id == 365670 && std::any_of(eligible.begin(), eligible.end(), [](const DepotInfo& d) {
        return d.id == 365671 && d.manifest_gid && *d.manifest_gid != 0;
      })) {
    eligible.erase(std::remove_if(eligible.begin(), eligible.end(), [](const DepotInfo& d) {
      return d.id == 365672;
    }), eligible.end());
  }
  if (eligible.empty()) {
    return Fail(reason, "This game has no downloadable Windows content for English.");
  }
  std::vector<DepotInfo> result;'''
if source.count(old)!=1:raise SystemExit('Steam depot selection changed; refusing ambiguous patch')
out=Path(sys.argv[1]) if len(sys.argv)==2 else ROOT/'build/steam-native-experiment/src/steam_integration/blender_depot_app_info.cpp'
out.write_text(source.replace(old,new),encoding='utf8',newline='\n')
