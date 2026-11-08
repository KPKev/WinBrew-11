import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / filename)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


repair = load('mode_repair', 'fix-vapor-install-modes.py')
packager = load('mode_packager', 'package-vapor-cs2-release.py')


class NativeModuleModesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.app = Path(self.tmp.name) / 'PPSA77382'
        self.runtime = {'path': 'wine/bin/wine64', 'mode': '0755',
                        'sha256': hashlib.sha256(b'runtime').hexdigest()}
        self.files = {
            'eboot.bin': b'app', 'lapy.elf': b'helper',
            'vapor-title-remover.elf': b'remover',
            'sce_module/libc.prx': b'native library',
            'assets/icon.png': b'image data',
            'sce_sys/param.json': b'{"titleId":"PPSA77382"}',
            repair.PREFIX + self.runtime['path']: b'runtime',
            repair.PREFIX + 'vapor-runtime.json': json.dumps({'files': [self.runtime]}).encode(),
        }
        for name, data in self.files.items():
            path = self.app / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        self.manifest = json.dumps({'files': {
            name: hashlib.sha256(data).hexdigest() for name, data in self.files.items()
        }}).encode()
        (self.app / 'manifest.json').write_bytes(self.manifest)

    def test_repair_keeps_native_module_executable_and_data_nonexecutable(self):
        data = {repair.APP + '/' + name: body for name, body in self.files.items()}
        data[repair.APP + '/manifest.json'] = self.manifest
        client = mock.Mock()
        client.read.side_effect = data.__getitem__
        with mock.patch.object(repair, 'APP_PIN', hashlib.sha256(self.manifest).hexdigest()), \
             mock.patch.object(repair, 'RUNTIME_PIN', hashlib.sha256(
                 self.files[repair.PREFIX + 'vapor-runtime.json']).hexdigest()):
            modes = repair.plan(client)
        self.assertEqual(modes[repair.APP + '/sce_module/libc.prx'], 0o755)
        self.assertEqual(modes[repair.APP + '/assets/icon.png'], 0o644)
        self.assertEqual(modes[repair.APP + '/' + repair.PREFIX + self.runtime['path']], 0o755)

    def test_zip_keeps_native_module_and_runtime_execute_permissions(self):
        output = Path(self.tmp.name) / 'app.zip'
        with mock.patch.object(packager.cs2, 'check_header'), \
             mock.patch.object(packager.blender, 'check_header'), \
             mock.patch.object(packager.cs2, 'expected_hashes', return_value={}), \
             mock.patch.object(packager.blender, 'expected_hashes', return_value={}), \
             mock.patch.object(packager.cs2, 'entries', return_value=[self.runtime]), \
             mock.patch.object(packager.blender, 'entries', return_value=[]), \
             mock.patch.object(packager, 'document', return_value=b'notice'):
            packager.package(self.app, output)
        with zipfile.ZipFile(output) as bundle:
            self.assertIsNone(bundle.testzip())
            mode = lambda name: (bundle.getinfo('PPSA77382/' + name).external_attr >> 16) & 0o777
            self.assertEqual(mode('sce_module/libc.prx'), 0o755)
            self.assertEqual(mode('assets/icon.png'), 0o644)
            self.assertEqual(mode(packager.cs2.PREFIX + self.runtime['path']), 0o755)
            for name, data in self.files.items():
                self.assertEqual(bundle.read('PPSA77382/' + name), data)


if __name__ == '__main__':
    unittest.main()
