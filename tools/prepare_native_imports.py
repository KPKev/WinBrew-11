"""Prepare link-only native import catalogs without changing the pinned SDK.

Run under WSL. Generated stubs describe public function names only; they are
never copied into the app. A build-local copy of the upstream driver uses the
combined catalog. All upstream source and runtime files stay unchanged.
"""
from pathlib import Path
import hashlib,json,shlex,shutil,subprocess
r=Path(__file__).resolve().parents[1]
v=r/'vendor/ps5-native-app-boilerplate'
sdk=v/'.deps/native/ps5-payload-sdk'
out=r/'build/native-imports';out.mkdir(parents=True,exist_ok=True)
for p in (sdk/'target/lib').glob('*.so'):
 target=out/p.name
 if not target.exists() or target.read_bytes()!=p.read_bytes():shutil.copy2(p,target)
catalog=json.loads((r/'src/native_imports.json').read_text())
for library,names in catalog.items():
 assert library.startswith('libSce') and library.replace('_','').isalnum()
 assert all(n.startswith('sce') and n.isalnum() for n in names)
 assembly=out/(library+'.S')
 assembly.write_text('.text\n'+''.join('.global '+n+'\n.type '+n+',@function\n'+n+':\n ret\n' for n in names))
 obj=out/(library+'.o')
 subprocess.run(['clang-18','--target=x86_64-unknown-freebsd','-c',str(assembly),'-o',str(obj)],check=True)
 subprocess.run(['ld.lld-18','-shared','--soname='+library+'.sprx','--hash-style=both',str(obj),'-o',str(out/(library+'.so'))],check=True)
source=(v/'tools/build.sh').read_text()
root_line='root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)'
assert source.count(root_line)==1
source=source.replace(root_line,'root='+shlex.quote(str(v)))
assert source.count('"$sdk_root"/target/lib/*.so')==3
assert source.count('"$sdk_root/target/lib"')==1
source=source.replace('"$sdk_root"/target/lib/*.so',shlex.quote(str(out))+'/*.so')
source=source.replace('"$sdk_root/target/lib"',shlex.quote(str(out)))
anchor='    args+=("${pacbrew_cflags[@]}")'
assert source.count(anchor)==1
source=source.replace(anchor,'    args+=(-include "$root/wine11/src/owner_admission/forced_config.h")\n'+anchor)
(r/'build/native-build.sh').write_text(source,newline='\n')
(out/'provenance.json').write_text(json.dumps({'source':'src/native_imports.json','link_only':True,'upstream_build_sha256':hashlib.sha256((v/'tools/build.sh').read_bytes()).hexdigest(),'modules':catalog},indent=2)+'\n')
print('Prepared native import catalogs; SDK and packaged runtime unchanged')
