import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import prepare_blender_runtime as bundle

class Runtime(unittest.TestCase):
    def test_committed_manifest_and_compiled_table_agree(self):
        bundle.check_header()
        files=bundle.entries()
        self.assertEqual(len(files),1768)
        self.assertEqual(next(f['sha256'] for f in files if f['path']=='lib/wine/x86_64-unix/win32u.prx'),
                         '3f942103da2f100fdb68abe4fee91e0716a050e97a6c83acd26fcd90787f5af4')
        self.assertTrue(any(f['path']=='title/eboot.bin' and f['mode']=='0755' for f in files))

    def test_corrupt_archive_refused_before_extraction(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'wrong.zip').write_bytes(b'not the pinned archive')
            with self.assertRaisesRegex(ValueError,'digest differs'):
                bundle.prepare(root/'new',root/'wrong.zip')
            self.assertFalse((root/'new').exists())

    def test_unsafe_path_cannot_be_authorized_by_a_forged_manifest_pin(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'manifest.json';value=json.loads(bundle.MANIFEST.read_bytes())
            value['title_files'][0]['path']='title/../../game-launch.json'
            raw=json.dumps(value).encode();p.write_bytes(raw)
            with mock.patch.object(bundle,'MANIFEST',p),mock.patch.object(bundle,'MANIFEST_SHA256',hashlib.sha256(raw).hexdigest()):
                with self.assertRaisesRegex(ValueError,'Unsafe'):bundle.entries()

    def test_manifest_pin_is_required(self):
        with mock.patch.object(bundle,'MANIFEST_SHA256','0'*64):
            with self.assertRaisesRegex(ValueError,'compiled pin'):bundle.entries()

    def test_unpinned_extra_file_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'extra.bin').write_bytes(b'extra')
            with self.assertRaisesRegex(ValueError,'inventory differs'):bundle.verify(root)

if __name__=='__main__':unittest.main()
