#!/usr/bin/env python3
"""Package a read-only base-kit TAR export, checking every file against its pin."""
import argparse, hashlib, json, tarfile, zipfile
from pathlib import Path

PIN = '1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc'
def sha(data): return hashlib.sha256(data).hexdigest()
def package(source, output, notices):
    with tarfile.open(source) as archive:
        members = archive.getmembers()
        ordinary = {m.name: m for m in members if m.isfile()}
        if any(not (m.isfile() or m.isdir()) for m in members) or len({m.name for m in members}) != len(members):
            raise ValueError('Unexpected base-kit export member')
        raw = archive.extractfile(ordinary['playden-kit/manifest.json']).read()
        if sha(raw) != PIN: raise ValueError('Base-kit manifest differs')
        files = {'playden-kit/' + f['path']: f for f in json.loads(raw)['files']}
        if set(ordinary) != set(files) | {'playden-kit/manifest.json'}: raise ValueError('Base-kit inventory differs')
        with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
            for name in sorted(ordinary):
                data = archive.extractfile(ordinary[name]).read(); entry = files.get(name)
                if sha(data) != (entry['sha256'] if entry else PIN): raise ValueError('Base-kit file differs: ' + name)
                if entry and len(data) != entry['size']: raise ValueError('Base-kit size differs')
                info = zipfile.ZipInfo(name, (2026,11,7,0,0,0)); info.create_system = 3
                info.external_attr = (0o100000 | (int(entry['mode'],8) if entry else 0o644)) << 16
                info.compress_type = zipfile.ZIP_DEFLATED; bundle.writestr(info, data)
            with zipfile.ZipFile(notices) as upstream:
                if sha(Path(notices).read_bytes()) != '6d58e72112c8e5d8ab0a4872f53cdbefb63adc436d366201cd02e704f74482b5':
                    raise ValueError('Upstream base kit differs')
                for item in upstream.infolist():
                    if not item.is_dir() and (item.filename.startswith('playden-kit-0.13/LICENSES/') or item.filename.endswith('/SOURCES.md')):
                        bundle.writestr('release-notices/' + item.filename.split('/',1)[1], upstream.read(item))
    print('PASS: base kit complete; SHA256', sha(Path(output).read_bytes()))

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('source'); p.add_argument('output'); p.add_argument('upstream_release_zip')
    a=p.parse_args(); package(a.source,a.output,a.upstream_release_zip)
