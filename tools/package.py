from pathlib import Path
import hashlib,json,zipfile
r=Path(__file__).resolve().parents[1];d=r/'dist/PPSA77381'
helper=json.loads((d/'lapy-manifest.json').read_text())
assert helper['target_title']=='PPSA77381' and helper['max_requests']==1 and not helper['service']
assert helper['protocol_sha256']=='bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869'
assert helper['elf_sha256']==hashlib.sha256((d/'lapy.elf').read_bytes()).hexdigest()
files={str(p.relative_to(d)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(d.rglob('*')) if p.is_file() and p.name!='manifest.json'}
(d/'manifest.json').write_text(json.dumps({'project':'WinBrew 11','milestone':16,'version':'01.016.000','console_tested':False,'browser':'PS5 system browser; Chrome-inspired launcher','launch_targets':{'Quake II RTX':'FAKE78003','Playden':'FAKE77031'},'files':files},indent=2))
with zipfile.ZipFile(r/'dist/WinBrew11-M1-PPSA77381.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in d.rglob('*'):
  if p.is_file():
   i=zipfile.ZipInfo(str(p.relative_to(d.parent)).replace('\\','/'));i.external_attr=0o100777<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,p.read_bytes())
print('Packaged separate title PPSA77381, hashes and license notices')
