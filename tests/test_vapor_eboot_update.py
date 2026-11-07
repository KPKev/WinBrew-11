"""The eboot updater must refuse changed inputs before any remote rename."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vapor_update', ROOT / 'tools/install_skyrim_vapor_fix.py')
update = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update)


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.old, self.new = self.root / 'old', self.root / 'new'
        for folder, binary in ((self.old, b'old executable'), (self.new, b'new executable')):
            folder.mkdir()
            (folder / 'eboot.bin').write_bytes(binary)
            manifest = {'files': {'eboot.bin': hashlib.sha256(binary).hexdigest()}}
            (folder / 'manifest.json').write_text(json.dumps(manifest))
        self.status = self.root / 'status.json'
        self.status.write_text(json.dumps({'phase': 'staged', 'changed': ['eboot.bin']}))
        self.files = {}
        for name in ('eboot.bin', 'manifest.json'):
            self.files[update.remote(name)] = (self.old / name).read_bytes()
            self.files[update.remote(name) + '.test-update.stage'] = (self.new / name).read_bytes()
        fake = type('Remote', (), {})()
        fake.read = lambda path: self.files[path]
        fake.listing = lambda _: [{'name': Path(path).name} for path in self.files]
        self.patches = [patch.object(update, 'OLD', self.old), patch.object(update, 'NEW', self.new),
                        patch.object(update, 'STATUS', self.status), patch.object(update, 'TAG', 'test-update'),
                        patch.object(update, 'WFM', return_value=fake), patch.object(update, 'post'),
                        patch.object(update.urllib.request, 'urlopen', return_value=io.BytesIO(b'{"processes":[]}'))]
        self.mocks = [p.start() for p in self.patches]

    def tearDown(self):
        for p in reversed(self.patches):
            p.stop()
        self.tmp.cleanup()

    def refused_without_mutation(self, reason):
        with self.assertRaisesRegex(RuntimeError, reason):
            update.apply()
        self.mocks[5].assert_not_called()
        self.assertEqual(json.loads(self.status.read_text())['phase'], 'staged')

    def test_changed_installed_manifest(self):
        self.files[update.remote('manifest.json')] = b'changed'
        self.refused_without_mutation('manifest changed')

    def test_changed_installed_executable(self):
        self.files[update.remote('eboot.bin')] = b'changed'
        self.refused_without_mutation('Installed file changed')

    def test_changed_staged_executable(self):
        self.files[update.remote('eboot.bin') + '.test-update.stage'] = b'changed'
        self.refused_without_mutation('Staged file changed')

    def test_backup_appeared(self):
        self.files[update.remote('eboot.bin.before-test-update.bak')] = b'prior backup'
        self.refused_without_mutation('Backup appeared')

    def test_running_title(self):
        self.mocks[6].return_value = io.BytesIO(b'{"processes":[{"name":"eboot.bin"}]}')
        self.refused_without_mutation('Close Vapor')


if __name__ == '__main__':
    unittest.main()
