import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('verify_launcher', Path(__file__).resolve().parents[1] / 'tools/verify_launcher_release.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class LauncherReleaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.source = self.root / 'source'; self.source.mkdir()
        def git(*args):
            return subprocess.check_output(['git', '-C', str(self.source), *args], stderr=subprocess.DEVNULL, text=True).strip()
        git('init'); git('config', 'user.email', 'test@example.invalid'); git('config', 'user.name', 'Test')
        (self.source / 'main.c').write_text('reviewed source\n')
        git('add', 'main.c'); git('commit', '-m', 'source')
        self.revision = git('rev-parse', 'HEAD')
        self.kit = self.root / 'kit'; (self.kit / 'titles/any').mkdir(parents=True)
        eboot = b'test executable bytes'
        (self.kit / 'titles/any/eboot.bin').write_bytes(eboot)
        build = {'schema':'prospero-win-native-title/1', 'source_dirty':False, 'source_revision':self.revision,
                 'title_id':'FAKE78000', 'any_title':True, 'eboot_sha256':hashlib.sha256(eboot).hexdigest()}
        build_bytes = json.dumps(build).encode()
        (self.kit / 'titles/any/native-title-build.json').write_bytes(build_bytes)
        self.manifest = {'sources':{'wrapper':{'revision':self.revision}}, 'variants':[{'any_title':True,
            'wrapper_revision':self.revision, 'title_id':'FAKE78000', 'path':'titles/any',
            'native_build_manifest':'titles/any/native-title-build.json'}], 'files':[]}
        for name, data in [('titles/any/eboot.bin', eboot), ('titles/any/native-title-build.json', build_bytes)]:
            self.manifest['files'].append({'path':name, 'size':len(data), 'sha256':hashlib.sha256(data).hexdigest()})
        self.save()
    def tearDown(self): self.tmp.cleanup()
    def save(self): (self.kit / 'manifest.json').write_text(json.dumps(self.manifest))
    def check(self): return module.verify(self.source, self.kit, self.revision)
    def test_verified_binding(self): self.assertEqual(self.check()['source_revision'], self.revision)
    def test_git_safety_is_scoped_to_reviewed_checkout(self):
        with mock.patch.object(module.subprocess, 'run') as call:
            call.return_value.stdout = 'ok\n'
            self.assertEqual(module.git(self.source, 'rev-parse', 'HEAD'), 'ok')
            args = call.call_args.args[0]
            self.assertEqual(args[:5], ['git', '-c', 'safe.directory=' + str(self.source), '-C', str(self.source)])
            self.assertEqual(args[5:], ['rev-parse', 'HEAD'])
    def test_dirty_source_refused(self):
        (self.source / 'main.c').write_text('changed')
        with self.assertRaises(ValueError): self.check()
    def test_parent_git_must_not_supply_archive_revision(self):
        child = self.source / 'archive'; child.mkdir()
        with self.assertRaises(ValueError): module.verify(child, self.kit, self.revision)
    def test_changed_eboot_refused(self):
        (self.kit / 'titles/any/eboot.bin').write_bytes(b'other')
        with self.assertRaises(ValueError): self.check()
    def test_wrong_manifest_binding_refused(self):
        self.manifest['files'][0]['sha256'] = '0' * 64; self.save()
        with self.assertRaises(ValueError): self.check()
    def test_duplicate_any_title_refused(self):
        self.manifest['variants'].append(self.manifest['variants'][0]); self.save()
        with self.assertRaises(ValueError): self.check()
    def test_path_escape_refused(self):
        self.manifest['variants'][0]['native_build_manifest'] = '../outside'; self.save()
        with self.assertRaises(ValueError): self.check()

if __name__ == '__main__': unittest.main()
