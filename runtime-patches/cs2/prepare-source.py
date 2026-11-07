#!/usr/bin/env python3
import tarfile,pathlib,hashlib,json,subprocess,shutil
repo=pathlib.Path.cwd();work=pathlib.Path.home()/'counterstrike-wine019-work';work.mkdir(exist_ok=True)
records=json.loads((repo/'evidence/cs2-source-archives.json').read_text())
for item,leaf in zip(records,['prospero','wine-base']):
 archive=repo/item['path'].replace('\\','/')
 assert hashlib.file_digest(archive.open('rb'),'sha256').hexdigest()==item['sha256']
 out=work/leaf
 if out.exists():raise RuntimeError('Source directory exists; inspect')
 out.mkdir()
 with tarfile.open(archive) as t:
  prefix=t.getmembers()[0].name.split('/')[0]+'/'
  for m in t.getmembers():
   if not m.name.startswith(prefix):continue
   m.name=m.name[len(prefix):]
   if m.name:t.extract(m,out,filter='data')
 print('Extracted',leaf,flush=True)
source=work/'wine-source';shutil.copytree(work/'wine-base',source)
patches=sorted((work/'prospero/wine/patches').glob('*.patch'))
with (work/'patches-applied.log').open('w') as log:
 for p in patches:
  subprocess.run(['patch','--batch','-p1','-i',str(p)],cwd=source,stdout=log,stderr=subprocess.STDOUT,check=True)
print('Applied',len(patches),'exact7537patches',flush=True)
subprocess.run(['python3',str(work/'prospero/tools/stage_vk_batch.py'),'--source',str(source),'--repo',str(work/'prospero')],check=True)
shutil.copyfile(work/'prospero/wine/ps5/time/pw_qpc_clock.h',source/'dlls/ntdll/pw_qpc_clock.h')
shutil.copyfile(work/'prospero/wine/ps5/input/pw_key_shared.h',source/'dlls/win32u/pw_key_shared.h')

shutil.copyfile(work/'prospero/wine/ps5/menu/pw_title_menu.h',source/'dlls/ntdll/pw_title_menu.h')
