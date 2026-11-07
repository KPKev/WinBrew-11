import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('readiness', ROOT / 'tools/check_return_readiness.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class Reader:
    def __init__(self, files):
        self.files = files
        self.paths = []

    def listing(self, path):
        self.paths.append(path)
        assert path == probe.ROOT
        return [{'name': '.playden-title-owner', 'mode': 0o600},
                {'name': 'game-launch.json', 'mode': 0o600}]

    def read(self, path, maximum):
        self.paths.append(path)
        if path not in self.files or len(self.files[path]) > maximum:
            raise probe.Unavailable('private content that must never be printed')
        return self.files[path]


class ReadinessTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.candidate = Path(self.temp.name)
        (self.candidate / 'eboot.bin').write_bytes(b'candidate')
        self.native = {'source_revision': probe.BASE, 'eboot_sha256': hashlib.sha256(b'installed').hexdigest(),
                       'managed_descriptor_required': True,
                       'managed_publication_guard': 'playden-publication-guard-v1',
                       'publication_provider_sha256': probe.PROVIDER,
                       'any_title': True, 'title_rule': {'prefix': 'FAKE78'}}
        candidate = {**self.native, 'eboot_sha256': hashlib.sha256(b'candidate').hexdigest(),
                     'wine11_return': {'feature': 'wine11-return-on-exit-v1',
                                       'argument': 'wine11-return=PPSA77381',
                                       'target': 'PPSA77381', 'base_source_revision': probe.BASE}}
        (self.candidate / 'native-title-build.json').write_text(json.dumps(candidate))
        self.files = {probe.ROOT + '/native-title-build.json': json.dumps(self.native).encode(),
                      probe.ROOT + '/eboot.bin': b'installed'}

    def tearDown(self):
        self.temp.cleanup()

    def check(self, report, key):
        return next(item['result'] for item in report['checks'] if item['check'] == key)

    def test_unreadable_private_files_are_a_blocker_without_guessing_paths(self):
        reader = Reader(self.files)
        report = probe.diagnose(reader, self.candidate)
        self.assertFalse(report['safe_to_install'])
        self.assertIn('title_owner_unreadable', report['blockers'])
        self.assertIn('descriptor_unreadable', report['blockers'])
        self.assertEqual('pass', self.check(report, 'candidate_matches_installed_base'))
        self.assertTrue(all(path.startswith(probe.ROOT) for path in reader.paths))
        self.assertNotIn('private content', json.dumps(report))

    def test_no_traversal_from_malformed_descriptor(self):
        self.files[probe.ROOT + '/game-launch.json'] = json.dumps({
            'schema_version': 2, 'title_id': probe.TITLE, 'app_id': probe.APP,
            'install_id': 'steam-1089130', 'data_root': '/data/playden/games/../../private',
            'exit_policy': 'exit-title'}).encode()
        reader = Reader(self.files)
        report = probe.diagnose(reader, self.candidate)
        self.assertEqual('fail', self.check(report, 'descriptor_fixed_quake_identity'))
        self.assertTrue(all(path.startswith(probe.ROOT) for path in reader.paths))

    def test_candidate_hash_and_target_tampering_fail(self):
        (self.candidate / 'eboot.bin').write_bytes(b'changed')
        record = json.loads((self.candidate / 'native-title-build.json').read_text())
        record['wine11_return']['target'] = 'FAKE78003'
        (self.candidate / 'native-title-build.json').write_text(json.dumps(record))
        report = probe.diagnose(Reader(self.files), self.candidate)
        self.assertEqual('fail', self.check(report, 'candidate_wrapper_record_hash'))
        self.assertEqual('fail', self.check(report, 'candidate_fixed_return_feature'))

    def test_private_values_never_appear_even_when_content_checks_pass(self):
        data_root = '/data/playden/games/steam-1089130'
        descriptor = {'schema_version': 2, 'title_id': probe.TITLE, 'app_id': probe.APP,
                      'install_id': 'steam-1089130', 'data_root': data_root, 'exit_policy': 'exit-title',
                      'runtime_bundle_id': 'private-runtime', 'recipe_id': 'private-recipe',
                      'recipe_version': '1', 'settings_revision': 3}
        values = {'schema': '1', 'kind': 'title', 'install_id': 'steam-1089130', 'title_id': probe.TITLE,
                  'kit_id': 'playden-kit', 'files_sha256': 'a' * 64,
                  'runtime_bundle_id': 'private-runtime', 'runtime_revision': 'private-revision',
                  'runtime_path': 'runtime/private', 'variant_id': 'private-variant',
                  'template_path': 'template/private', 'recipe_id': 'private-recipe', 'recipe_version': '1'}

        def marker(kind):
            return ''.join(f'{k}={kind if k == "kind" else v}\n' for k, v in values.items()).encode()

        metadata = b'private publication contents'
        receipt = {k: descriptor[k] for k in ['install_id', 'title_id', 'runtime_bundle_id',
                                               'recipe_id', 'recipe_version', 'settings_revision']}
        receipt.update(phase='committed', publication_sha256=hashlib.sha256(metadata).hexdigest())
        self.files.update({
            probe.ROOT + '/.playden-title-owner': marker('title'),
            probe.ROOT + '/game-launch.json': json.dumps(descriptor).encode(),
            data_root + '/library/prefixes/game/.playden-template-owner': marker('template'),
            data_root + '/.playden-control/publication-state.v1': json.dumps(receipt).encode(),
            data_root + '/library/profiles/.playden-publication': metadata,
            '/data/playden/kits/playden-kit/manifest.json': json.dumps({
                'kit_id': 'playden-kit', 'files_sha256': 'a' * 64}).encode()})
        report = probe.diagnose(Reader(self.files), self.candidate)
        self.assertEqual('pass', self.check(report, 'owner_respin_identity'))
        self.assertEqual('pass', self.check(report, 'publication_metadata_receipt_hash'))
        self.assertEqual('unverified', self.check(report, 'native_publication_semantic_admission'))
        self.assertFalse(report['safe_to_install'])
        for value in ['private-runtime', 'private-recipe', 'private-revision', 'private publication']:
            self.assertNotIn(value, json.dumps(report))

    def test_duplicate_private_fields_fail_closed(self):
        self.files[probe.ROOT + '/game-launch.json'] = b'{"app_id":1089130,"app_id":1}'
        report = probe.diagnose(Reader(self.files), self.candidate)
        self.assertIn('descriptor_invalid', report['blockers'])

    def test_transport_refuses_mutating_endpoints(self):
        reader = probe.ReadOnlyWfm('127.0.0.1', 1)
        for endpoint in ['/api/chmod', '/api/upload-file', '/api/delete', '/api/rename', '/launch']:
            with self.assertRaises(ValueError):
                reader._post(endpoint)


if __name__ == '__main__':
    unittest.main()
