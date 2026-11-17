#!/usr/bin/env python3
"""Package application sources without build outputs or private console state."""
import argparse,hashlib,os,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TREES=['src','tools','tests','assets','art','sce_sys','licenses','runtime-patches']
FILES=['README.md','CHANGELOG.md','LICENSE','THIRD_PARTY_NOTICES.md','dependencies.json','Preview.cmd','.gitignore','.gitattributes']
DOCS=['vapor-backend-updates.md','vapor-backend-verification.json','current-status.md','release-notes-blender-shared-files-v1.md','source-build.md','getting-started.md','blender-runtime.md','cs2-runtime.md','release-notes-next.md','blender-shared-verification.json','compatibility.md','troubleshooting.md','reports/blender-automatic-integration-20261116.md']
EXCLUDE_PATHS={'tools/audit_wsl_cleanup.py','tools/Move-UbuntuToG.ps1','assets/blender-runtime/winbrew-owner-manifest.json','runtime-patches/blender/winbrew-bridge-receipt.json','src/steam_integration/blender_shared_migration.hpp','src/steam_integration/blender_shared_runtime_files.hpp','tests/blender_shared_migration_test.cpp','tools/test-document-bridge-fixtures.sh'}
EXCLUDED={'.git','build','dist','__pycache__','.deps'}
def package(output):
    files=[ROOT/n for n in FILES]+[ROOT/'docs'/n for n in DOCS]
    for tree in TREES:
        for parent,dirs,names in os.walk(ROOT/tree):
            dirs[:]=[n for n in dirs if n not in EXCLUDED and not (Path(parent)/n).is_symlink()]
            files.extend(Path(parent)/n for n in names if not n.endswith(('.pyc','.o','.elf','.bin','.log')))
    receipts=[]
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for path in sorted(set(files)):
            if not path.is_file() or path.is_symlink():continue
            rel=path.relative_to(ROOT).as_posix()
            if rel in EXCLUDE_PATHS or rel.startswith(('runtime-patches/blender/managed-owner/','src/document_bridge/','tests/document_bridge_')):continue
            data=path.read_bytes()
            info=zipfile.ZipInfo('WinBrew11/'+rel,(2026,11,16,0,0,0));info.create_system=3;info.external_attr=(0o100755 if path.suffix=='.sh' else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
            receipts.append(hashlib.sha256(data).hexdigest()+'  '+rel+'\n')
        z.writestr('WinBrew11/SOURCE-SHA256SUMS.txt',''.join(receipts))
    print('Application source:',len(receipts),'files; dependency archives supplied separately')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('new_output',type=Path);package(p.parse_args().new_output)
