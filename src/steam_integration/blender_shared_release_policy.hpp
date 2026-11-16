// Generated from the pinned Blender019.3 manifest.
#pragma once
namespace ww::steam_integration {
inline constexpr const char* kBlenderSharedReleasePin="4021aa951c421d520e3c2ddfc719315428f6369b4ee588a11af1c0d9491be9e2";
template<class Runtime,class File>bool BlenderSharedHelperFileAllowed(const Runtime&runtime,const File&file){
 return runtime.steam_app_id==365670 && runtime.revision=="blender-019.3" && runtime.manifest_sha256==kBlenderSharedReleasePin && (
      (file.path=="title/lapy-manifest.json" && file.size==741 && file.sha256=="f02e6408bc1e43737f719ca136ba9844b55ac52eb32a7d7cf3d45c8bcf2b4b54" && file.mode==0644) ||
      (file.path=="title/lapy.elf" && file.size==173080 && file.sha256=="decdc69a718522f64f504af9f759791e60be05f52b80fb9dcca7e2925f94ab33" && file.mode==0755));
}
}
