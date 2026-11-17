"""The release ZIPs remain byte-identical when packaging times differ."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

class BackendPackageTest(unittest.TestCase):
    def test_reproducible_archives_include_upstream_notices(self):
        spec = importlib.util.spec_from_file_location('backend_package', ROOT / 'tools/package-vapor-backend-update.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app, kit = root / 'app', root / 'kit'
            (app / 'sce_sys').mkdir(parents=True)
            kit.mkdir()
            (app / 'eboot.bin').write_bytes(b'synthetic app')
            (app / 'sce_sys/param.json').write_text(json.dumps({'titleId':'PPSA77382','contentVersion':'01.018.000'}))
            files = {p.relative_to(app).as_posix(): module.sha(p) for p in app.rglob('*') if p.is_file()}
            (app / 'manifest.json').write_text(json.dumps({'files':files}))
            wine = kit / 'wine.txt'
            wine.write_bytes(b'synthetic runtime')
            wine.chmod(0o644)
            (kit / 'manifest.json').write_text(json.dumps({'runtime':{'artifact_revision':'synthetic-kit'},'files':[{'path':'wine.txt','size':wine.stat().st_size,'mode':'0644','sha256':module.sha(wine)}]}))
            (root / 'build').mkdir()
            notices = root / 'build/playden-kit-0.28.zip'
            with zipfile.ZipFile(notices,'w') as z:
                z.writestr('playden-kit-0.28/LICENSES/test.txt', 'synthetic license')
                z.writestr('playden-kit-0.28/SOURCES.md', 'synthetic source reference')
            real_sha = module.sha
            def pinned_sha(path):
                return '9287fc92e1cd22dac3c09ded274c8e0a58ba1609ceab5b609265da9ab4caf424' if Path(path)==notices else real_sha(path)
            policy = types.SimpleNamespace(PREFIX='synthetic/',check_header=lambda:None,expected_hashes=lambda:{},entries=lambda:[])
            module.__file__ = str(root / 'tools/package-vapor-backend-update.py')
            with mock.patch.object(module,'cs2',policy), mock.patch.object(module,'blender',policy), mock.patch.object(module,'sha',pinned_sha):
                for name,year in [('first',2020),('second',2021)]:
                    with mock.patch('zipfile.time.localtime',return_value=(year,1,2,3,4,6,0,0,0)):
                        module.package(app,kit,root/name)
            for name in ['WinBrew-11-v1.18.0.zip','Vapor-backend-kit-0.28.zip']:
                self.assertEqual(real_sha(root/'first'/name),real_sha(root/'second'/name))
            with zipfile.ZipFile(root/'first/Vapor-backend-kit-0.28.zip') as z:
                self.assertEqual(z.read('release-notices/LICENSES/test.txt'),b'synthetic license')
                self.assertEqual(z.getinfo('release-notices/LICENSES/test.txt').date_time,(2026,11,16,0,0,0))

if __name__ == '__main__':
    unittest.main()
