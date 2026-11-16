#include "../src/steam_integration/blender_artifact_migration.hpp"
#include <cassert>
#include <string>
#include <vector>
struct A { std::string artifact_id,identity,sha256; };
int main() {
 const auto hash=std::string(64,'a');
 std::vector<A> old={{"runtime","wine-wow64-031302d-kit-0.13","6f5d2365ad5553819a21a390edb57c739d3af99b2392b16141b7fc8c140a5adb"},{"recipe","recipe",hash}};
 std::vector<A> wanted={{"runtime","wine-wow64-blender-019.1",hash},{"recipe","recipe",hash},{"compatibility-title","blender-019.1-title",hash}};
 using ww::steam_integration::CanMigrateLegacyBlenderArtifacts;
 assert(CanMigrateLegacyBlenderArtifacts(365670,old,wanted));
 assert(!CanMigrateLegacyBlenderArtifacts(730,old,wanted));
 auto x=old;x[0].sha256=hash;assert(!CanMigrateLegacyBlenderArtifacts(365670,x,wanted));
 x=old;x[1].identity="other";assert(!CanMigrateLegacyBlenderArtifacts(365670,x,wanted));
 auto y=wanted;y[2].artifact_id="unrelated";assert(!CanMigrateLegacyBlenderArtifacts(365670,old,y));
 y=wanted;y[2].identity="different-title";assert(!CanMigrateLegacyBlenderArtifacts(365670,old,y));
 y=wanted;y[1].sha256=std::string(64,'b');assert(!CanMigrateLegacyBlenderArtifacts(365670,old,y));
 x=old;x.push_back(x.back());assert(!CanMigrateLegacyBlenderArtifacts(365670,x,wanted));
 std::vector<A> old2={{"runtime","wine-wow64-blender-019.1",hash},{"recipe","recipe",hash},{"compatibility-title","blender-019.1-title","2191c6d66d0964fb3916cc797d992204729e3bcf3ac0db73f57a4630ef1f2644"}};
 std::vector<A> new2={{"runtime","wine-wow64-blender-019.2",hash},{"recipe","recipe",hash},{"compatibility-title","blender-019.2-title",std::string(64,'b')}};
 using ww::steam_integration::CanMigrateBlenderInlineEditors;
 assert(CanMigrateLegacyBlenderArtifacts(365670,old,new2));
 assert(CanMigrateBlenderInlineEditors(365670,old2,new2));
 assert(!CanMigrateBlenderInlineEditors(730,old2,new2));
 auto altered=old2;altered[2].sha256=hash;assert(!CanMigrateBlenderInlineEditors(365670,altered,new2));
 altered=old2;altered[1].identity="other";assert(!CanMigrateBlenderInlineEditors(365670,altered,new2));
 auto future=new2;future[0].sha256=std::string(64,'c');assert(!CanMigrateBlenderInlineEditors(365670,old2,future));
}
