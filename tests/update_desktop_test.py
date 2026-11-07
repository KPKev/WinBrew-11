"""Exercise real updater promotion/rollback against an isolated file service."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import sys
import tempfile
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import update_desktop as update

def sha(data): return hashlib.sha256(data).hexdigest()

for injected_failure in (False,True):
    with tempfile.TemporaryDirectory(prefix='wine11-update-test-') as directory:
        root=Path(directory);(root/'evidence').mkdir()
        source=root/'dist'/update.TITLE
        wanted={'eboot.bin':b'new desktop','assets/font.bin':b'new atlas','sce_sys/icon0.png':b'accepted icon',
                'sce_sys/param.json':json.dumps({'titleId':update.TITLE,'contentVersion':'01.004.000'}).encode(),
                'lapy.elf':b'exact helper','lapy-manifest.json':b'helper metadata','sce_module/libc.prx':b'fixed runtime',
                'licenses/new-icons.txt':b'new icon notice'}
        for name,data in wanted.items():
            target=source/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        manifest={'project':'Wine 11','files':{n:sha(d) for n,d in wanted.items()}}
        (source/'manifest.json').write_text(json.dumps(manifest))
        old={update.DEST+'/'+n:d for n,d in wanted.items() if not n.startswith('licenses/')}
        old[update.DEST+'/eboot.bin']=b'old desktop';old[update.DEST+'/assets/font.bin']=b'old atlas'
        before={'project':'WinBrew 11','files':{n.removeprefix(update.DEST+'/'):sha(d) for n,d in old.items()}}
        old[update.DEST+'/manifest.json']=json.dumps(before).encode();original=dict(old)
        class Service:
            def __init__(self,*args): self.fail=injected_failure
            def read(self,path): return b'' if path=='/data/shadowmount/debug.log' else old[path]
            def listing(self,path):
                return [{'name':PurePosixPath(n).name} for n in old if str(PurePosixPath(n).parent)==path]
            def upload(self,path,names):
                for name in names: old[update.DEST+'/'+name]=(path/name).read_bytes()
            def permissions(self,*args): pass
            def post(self,endpoint,params):
                assert endpoint=='/api/rename'
                src=params['path'];dst=str(PurePosixPath(src).with_name(params['name']))
                if self.fail and src.endswith('.stage') and dst==update.DEST+'/manifest.json':
                    self.fail=False;raise RuntimeError('Injected manifest promotion failure')
                assert dst not in old;old[dst]=old.pop(src)
        with patch.object(update,'ROOT',root),patch.object(update,'WFM',Service),patch.object(sys,'argv',['update_desktop.py','--apply']):
            try: update.main()
            except RuntimeError as error: assert injected_failure and 'Injected' in str(error)
        if injected_failure:
            assert all(old[n]==d for n,d in original.items())
            assert update.DEST+'/licenses/new-icons.txt' not in old
        else:
            assert all(old[update.DEST+'/'+n]==d for n,d in wanted.items())
            report=json.loads((root/'evidence/desktop-update.json').read_text())
            assert report['files']['licenses/new-icons.txt']['before'] is None
print('PASS: additive icon resources publish safely; injected failure restores old files and unpublishes additions')
