#!/usr/bin/env bash
set -euo pipefail
echo "PPSA77382 is an existing Vapor app; this archived build script is disabled." >&2
exit 1
cd "$(dirname "$0")/.."
root="$PWD"
out="$root/build/skyrim-admission-isolated-6"
[[ ! -e "$out" ]] || { echo 'Isolated destination exists; preserve it.' >&2; exit 1; }
mkdir -p "$out"
mkdir -p "$out/boilerplate/.deps"
for name in tools tooling runtime wine11; do cp -R "vendor/ps5-native-app-boilerplate/$name" "$out/boilerplate/$name"; done
ln -s "$root/vendor/ps5-native-app-boilerplate/.deps/native" "$out/boilerplate/.deps/native"
b="$out/boilerplate"
python3 - "$root" "$b" <<'PY'
import ast,json,pathlib,shutil,sys
root,b=map(pathlib.Path,sys.argv[1:]); stage=b/'wine11'; src=stage/'src'
def copytree(source,dest):
 for item in source.rglob('*'):
  target=dest/item.relative_to(source)
  if item.is_dir(): target.mkdir(parents=True,exist_ok=True)
  elif item.is_file(): target.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(item,target)
tree=ast.parse((root/'tools/stage_build.py').read_text()); loops=[n.iter for n in ast.walk(tree) if isinstance(n,ast.For) and isinstance(n.target,ast.Name) and n.target.id=='name' and isinstance(n.iter,ast.List)]
if len(loops)!=1: raise SystemExit('source list changed')
for name in ast.literal_eval(loops[0]): shutil.copyfile(root/'src'/name,src/name)
copytree(root/'src/owner_admission',src/'owner_admission')
shutil.copyfile(root/'research/skyrim/admission_observer.c',src/'owner_admission/admission_observer.c')
p=src/'ps5.cpp'; s=p.read_text(); old='/mnt/sandbox/PPSA77381_000/download0'; assert s.count(old)==1; p.write_text(s.replace(old,'/mnt/sandbox/PPSA77382_000/download0'))
copytree(root/'assets',stage/'assets'); copytree(root/'sce_sys',stage/'sce_sys')
p=stage/'sce_sys/param.json'; data=json.loads(p.read_text()); assert data['titleId']=='PPSA77381'; data['titleId']='PPSA77382';data['conceptId']='77382';data['contentId']=data['contentId'].replace('PPSA77381','PPSA77382');data['localizedParameters']['en-US']['titleName']='Wine 11 Skyrim Diagnostic';p.write_text(json.dumps(data,indent=2)+'\n')
script=(root/'build/native-build.sh').read_text(); script=script.replace('root='+str(root/'vendor/ps5-native-app-boilerplate'), 'root='+str(b)); assert 'root='+str(b) in script
(out:=b/'skyrim-build.sh').write_text(script)
PY
(cd "$b/wine11" && git apply "$root/research/skyrim/admission-ui.patch")
export CXX=g++ HOST_CXX=g++ HOST_CC=gcc APP_SOURCE_DIR=wine11/src APP_PARAM=wine11/sce_sys/param.json APP_SCE_SYS=wine11/sce_sys APP_ASSETS=wine11/assets APP_LAPY_HELPER=0 USE_CCACHE=0 BUILD_LABEL='Skyrim diagnostic'
export APP_ROOT_FILES='wine11/LICENSE wine11/THIRD_PARTY_NOTICES.md'
export APP_INCLUDE_PATHS='wine11/src/owner_admission/native wine11/src/owner_admission/src wine11/src/owner_admission/third_party/mbedtls-publication/include wine11/src/owner_admission/third_party/mbedtls-publication'
bash "$b/skyrim-build.sh" Folder
cp "$root/build/wine11-lapy-source/build/owned_root_helper-PPSA77382/lapy.elf" "$b/dist/PPSA77382/"
cp "$root/build/wine11-lapy-source/build/owned_root_helper-PPSA77382/lapy-manifest.json" "$b/dist/PPSA77382/"
cp -R "$root/licenses" "$b/dist/PPSA77382/"
