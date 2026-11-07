"""Read-only, sanitized preflight for the Wine 11 Quake return-wrapper upgrade.

This is an observation aid, never installation authorization. WFM cannot prove
native owner UID/inode/link-count semantics, obtain the qualified publication
lease, or establish native stopped state. It uses only listing/file-download
endpoints and never changes console files, permissions, registry or ownership.
Private document bytes are processed in memory and never printed or saved.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import urllib.error
import urllib.parse
import urllib.request

PROJECT = Path(__file__).resolve().parents[1]
TITLE = 'FAKE78003'
APP = 1089130
ROOT = '/data/homebrew/' + TITLE
PROVIDER = 'f14d5e3ecb31951e3cf5bfdc41972181a9be1d96ed0dc7cb3aeebb8e4aab844a'
BASE = 'ce289425bb34acbcc98f77b4e00ac98b3c16e4bc'


class Unavailable(Exception):
    pass


def pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError('duplicate')
        result[key] = value
    return result


def document(raw, maximum):
    if not raw or len(raw) > maximum:
        raise ValueError('size')
    result = json.loads(raw, object_pairs_hook=pairs)
    if not isinstance(result, dict):
        raise ValueError('object')
    return result


def marker(raw, kind):
    if not raw or len(raw) > 4096:
        raise ValueError('size')
    result = {}
    for line in raw.decode('ascii').splitlines():
        key, equal, value = line.partition('=')
        if not equal or not key or key in result or not value or any(ord(c) < 32 for c in value):
            raise ValueError('marker')
        result[key] = value
    needed = {'schema', 'kind', 'install_id', 'title_id', 'kit_id', 'files_sha256',
              'runtime_bundle_id', 'runtime_revision', 'runtime_path', 'variant_id',
              'template_path', 'recipe_id', 'recipe_version'}
    if set(result) != needed or result['schema'] != '1' or result['kind'] != kind:
        raise ValueError('marker schema')
    if result['title_id'] != TITLE or result['install_id'] != 'steam-' + str(APP):
        raise ValueError('identity')
    if not re.fullmatch('[0-9a-f]{64}', result['files_sha256']):
        raise ValueError('digest')
    if not re.fullmatch('[a-zA-Z0-9_-]{1,64}', result['kit_id']):
        raise ValueError('kit')
    return result


def compatible_marker_identity(first, second):
    # Strict same-kit subset of Playden9f5a7cb RespinIdentity. No renamed-kit
    # aliases are assumed. The original inventory must agree before a refresh.
    ignored = {'kind', 'files_sha256', 'runtime_revision', 'runtime_path', 'variant_id'}
    return {k: v for k, v in first.items() if k not in ignored} == {
        k: v for k, v in second.items() if k not in ignored}


class ReadOnlyWfm:
    def __init__(self, host, port):
        self.base = f'http://{host}:{port}'

    def _read_response(self, request, maximum):
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                data = response.read(maximum + 1)
            if len(data) > maximum:
                raise Unavailable('oversized')
            return data
        except (urllib.error.URLError, OSError, TimeoutError):
            raise Unavailable('unreadable') from None

    def _post(self, endpoint, values=None, query=None):
        if endpoint not in ('/api/list', '/api/download/prepare'):
            raise ValueError('non-read endpoint')
        url = self.base + endpoint
        if query:
            url += '?' + urllib.parse.urlencode(query)
        body = urllib.parse.urlencode(values or {}).encode()
        request = urllib.request.Request(url, data=body, method='POST', headers={
            'Content-Type': 'application/x-www-form-urlencoded'})
        try:
            result = document(self._read_response(request, 1024 * 1024), 1024 * 1024)
        except (ValueError, UnicodeError):
            raise Unavailable('invalid-response') from None
        if result.get('ok') is not True:
            raise Unavailable('unreadable')
        return result

    def listing(self, path):
        result = self._post('/api/list', query={'path': path})
        entries = result.get('entries')
        if not isinstance(entries, list):
            raise Unavailable('invalid-response')
        return entries

    def read(self, path, maximum):
        # Download preparation allocates a normal service read task; it does not
        # invoke upload/chmod/delete/rename/launch or alter the source document.
        task = self._post('/api/download/prepare', {'paths': path}).get('task_id')
        if not isinstance(task, (int, str)) or not re.fullmatch('[a-zA-Z0-9_-]{1,80}', str(task)):
            raise Unavailable('invalid-response')
        url = self.base + '/api/download?' + urllib.parse.urlencode({'id': task})
        return self._read_response(urllib.request.Request(url, method='GET'), maximum)


def diagnose(reader, candidate):
    checks = []
    blocked = []

    def check(name, result):
        checks.append({'check': name, 'result': result})

    def read(role, path, maximum):
        try:
            raw = reader.read(path, maximum)
            check(role + '_read', 'pass')
            return raw
        except Unavailable:
            check(role + '_read', 'unavailable')
            blocked.append(role + '_unreadable')
            return None

    def parse(role, raw, function):
        if raw is None:
            return None
        try:
            value = function(raw)
            check(role + '_format', 'pass')
            return value
        except (ValueError, UnicodeError, TypeError, KeyError):
            check(role + '_format', 'fail')
            blocked.append(role + '_invalid')
            return None

    try:
        entries = reader.listing(ROOT)
        modes = {x.get('name'): x.get('mode') for x in entries
                 if isinstance(x, dict) and isinstance(x.get('name'), str)}
        check('title_directory_observed', 'pass')
        for name, role in [('.playden-title-owner', 'title_owner'), ('game-launch.json', 'descriptor')]:
            check(role + '_listed', 'pass' if name in modes else 'unavailable')
            check(role + '_private_mode', 'pass' if modes.get(name) == 0o600 else 'unverified')
    except Unavailable:
        check('title_directory_observed', 'unavailable')

    native_raw = read('installed_wrapper_record', ROOT + '/native-title-build.json', 16384)
    native = parse('installed_wrapper_record', native_raw, lambda raw: document(raw, 16384))
    if native:
        check('installed_publication_guard', 'pass' if (
            native.get('managed_descriptor_required') is True and
            native.get('managed_publication_guard') == 'playden-publication-guard-v1' and
            native.get('publication_provider_sha256') == PROVIDER) else 'fail')
        eboot = read('installed_wrapper_binary', ROOT + '/eboot.bin', 1024 * 1024)
        if eboot is not None:
            check('installed_wrapper_record_hash', 'pass' if hashlib.sha256(eboot).hexdigest() == native.get('eboot_sha256') else 'fail')
    try:
        candidate_record = document((candidate / 'native-title-build.json').read_bytes(), 16384)
        digest = hashlib.sha256((candidate / 'eboot.bin').read_bytes()).hexdigest()
        check('candidate_wrapper_record_hash', 'pass' if digest == candidate_record.get('eboot_sha256') else 'fail')
        feature = candidate_record.get('wine11_return', {})
        check('candidate_fixed_return_feature', 'pass' if (
            feature.get('feature') == 'wine11-return-on-exit-v1' and
            feature.get('argument') == 'wine11-return=PPSA77381' and
            feature.get('target') == 'PPSA77381') else 'fail')
        if native:
            check('candidate_matches_installed_base', 'pass' if feature.get('base_source_revision') == native.get('source_revision') == BASE else 'fail')
            check('candidate_preserves_publication_guard', 'pass' if all(
                candidate_record.get(k) == native.get(k) for k in (
                    'managed_descriptor_required', 'managed_publication_guard', 'publication_provider_sha256', 'any_title', 'title_rule')) else 'fail')
    except (OSError, ValueError, TypeError, AttributeError):
        check('candidate_artifact_readable', 'unavailable')
        blocked.append('candidate_artifact_unavailable')

    owner = parse('title_owner', read('title_owner', ROOT + '/.playden-title-owner', 4096), lambda raw: marker(raw, 'title'))
    desc_raw = read('descriptor', ROOT + '/game-launch.json', 8192)
    descriptor = parse('descriptor', desc_raw, lambda raw: document(raw, 8192))
    if descriptor:
        # Refuse to follow caller-controlled paths beyond this known Quake
        # install. The actual native parser remains the admission authority.
        expected = '/data/playden/games/steam-' + str(APP)
        identity = (descriptor.get('schema_version') in (1, 2) and
                    descriptor.get('title_id') == TITLE and descriptor.get('app_id') == APP and
                    descriptor.get('install_id') == 'steam-' + str(APP) and
                    descriptor.get('data_root') == expected and descriptor.get('exit_policy') == 'exit-title')
        check('descriptor_fixed_quake_identity', 'pass' if identity else 'fail')
        if not identity:
            descriptor = None
            blocked.append('descriptor_identity_mismatch')
    if owner and descriptor:
        data_root = descriptor['data_root']
        bottle_raw = read('template_owner', data_root + '/library/prefixes/game/.playden-template-owner', 4096)
        bottle = parse('template_owner', bottle_raw, lambda raw: marker(raw, 'template'))
        if bottle:
            check('owner_respin_identity', 'pass' if compatible_marker_identity(owner, bottle) else 'fail')
            check('owner_old_inventory_agreement', 'pass' if owner['files_sha256'] == bottle['files_sha256'] else 'fail')
        receipt_raw = read('publication_receipt', data_root + '/.playden-control/publication-state.v1', 8192)
        receipt = parse('publication_receipt', receipt_raw, lambda raw: document(raw, 8192))
        metadata_raw = read('publication_metadata', data_root + '/library/profiles/.playden-publication', 32768)
        if receipt:
            check('publication_receipt_committed', 'pass' if receipt.get('phase') == 'committed' else 'fail')
            keys = ['install_id', 'title_id', 'runtime_bundle_id', 'recipe_id', 'recipe_version', 'settings_revision']
            check('publication_receipt_descriptor_identity', 'pass' if all(
                receipt.get(k) == descriptor.get(k) for k in keys) else 'fail')
            if metadata_raw is not None:
                check('publication_metadata_receipt_hash', 'pass' if hashlib.sha256(metadata_raw).hexdigest() == receipt.get('publication_sha256') else 'fail')
        kit_path = '/data/playden/kits/' + owner['kit_id'] + '/manifest.json'
        kit_raw = read('installed_kit_manifest', kit_path, 8 * 1024 * 1024)
        kit = parse('installed_kit_manifest', kit_raw, lambda raw: document(raw, 8 * 1024 * 1024))
        if kit:
            check('owner_bound_to_observed_kit_inventory', 'pass' if kit.get('kit_id') == owner['kit_id'] and kit.get('files_sha256') == owner['files_sha256'] else 'fail')

    # Transport metadata is insufficient for the source's native admission
    # checks, even when every content comparison above happens to pass.
    for name in ['native_uid_inode_and_single_link_ownership', 'qualified_publication_lease',
                 'native_publication_semantic_admission', 'native_title_stopped_state',
                 'proposed_whole_kit_and_same_artifact_admission']:
        check(name, 'unverified')
    blocked.extend(x['check'] for x in checks if x['result'] == 'fail')
    return {'scope': 'Quake II RTX native return-wrapper preflight', 'title': TITLE,
            'safe_to_install': False, 'read_only': True,
            'checks': checks, 'blockers': sorted(set(blocked)),
            'required_next_step': 'Read native owner and committed publication metadata through an authorized existing owner context, then use the exact native admission and lease rules. Do not chmod, replace markers or install the wrapper alone.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='192.168.7.26')
    parser.add_argument('--port', type=int, default=8889)
    parser.add_argument('--candidate', type=Path, default=PROJECT / 'build/return-wrapper/review')
    args = parser.parse_args()
    print(json.dumps(diagnose(ReadOnlyWfm(args.host, args.port), args.candidate), indent=2))


if __name__ == '__main__':
    main()
