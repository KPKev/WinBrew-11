import hashlib,importlib.util,io,json,tempfile,unittest
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);return mod
fetch=load('fetch_ca','tools/fetch-steam-ca.py');stage=load('stage_assets','tools/stage-steam-runtime-assets.py')
class FetchTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.dest=Path(self.tmp.name)/'cacert.pem';self.data=b'pinned public fixture';self.sha=hashlib.sha256(self.data).hexdigest();self.p1=mock.patch.object(fetch,'EXPECTED_SIZE',len(self.data));self.p2=mock.patch.object(fetch,'EXPECTED_SHA256',self.sha);self.p1.start();self.p2.start()
 def tearDown(self):self.p2.stop();self.p1.stop();self.tmp.cleanup()
 def test_verified_download_and_exclusive_destination(self):
  with mock.patch.object(fetch.urllib.request,'urlopen',side_effect=AssertionError('unbound network')):
   fetch.fetch(self.dest,opener=lambda *a,**k:io.BytesIO(self.data))
  self.assertEqual(self.dest.read_bytes(),self.data)
  with self.assertRaises(FileExistsError):fetch.fetch(self.dest,opener=lambda *a,**k:io.BytesIO(b'bad'))
  self.assertEqual(self.dest.read_bytes(),self.data)
 def test_mismatch_leaves_no_file(self):
  with self.assertRaisesRegex(ValueError,'does not match'):fetch.fetch(self.dest,opener=lambda *a,**k:io.BytesIO(b'bad'))
  self.assertFalse(self.dest.exists())
 def test_symlink_refused(self):
  target=self.dest.parent/'missing'
  try:self.dest.symlink_to(target)
  except OSError:self.skipTest('Symlinks are unavailable to this test user')
  with self.assertRaises(FileExistsError):fetch.fetch(self.dest,opener=lambda *a,**k:io.BytesIO(self.data))
class StagingTests(unittest.TestCase):
 def test_explicit_pinned_ca_allows_fresh_staging_and_bad_ca_refused(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);repo=root/'repo';repo.mkdir();bundle=root/'ca';ca=b'x'*188900;bundle.write_bytes(ca)
   names=['runtime/presets.json','runtime/recipes/recipes.lst','runtime/recipes/LICENSE','runtime/input/LICENSE','runtime/steamworks/LICENSE','runtime/steamworks/README.md','runtime/steamworks/steam_api.dll','runtime/steamworks/steam_api64.dll','runtime/steamworks/steam.dll']
   data={n:('fixture '+n).encode() for n in names}
   for n,content in data.items():p=repo/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(content)
   pins={n:hashlib.sha256(data['runtime/steamworks/'+n]).hexdigest() for n in ('steam_api.dll','steam_api64.dll','steam.dll')}
   def git(*args):
    if args[0]=='rev-parse':return (stage.REVISION+'\n').encode()
    if args[0]=='ls-tree':return ('\n'.join(names)+'\n').encode()
    if args[0]=='show':return data[args[1].split(':',1)[1]]
    raise AssertionError(args)
   lock=root/'assets-lock.json';lock.write_text(json.dumps({'source_revision':stage.REVISION,'files':{n:hashlib.sha256(v).hexdigest() for n,v in data.items()}}))
   with mock.patch.object(stage,'REPO',repo),mock.patch.object(stage,'STEAMWORKS',pins),mock.patch.object(stage,'CA_SHA256',hashlib.sha256(ca).hexdigest()),mock.patch.object(stage,'ASSET_LOCK',lock),mock.patch.object(stage,'ASSET_LOCK_SHA256',hashlib.sha256(lock.read_bytes()).hexdigest()),mock.patch.object(stage,'git',side_effect=AssertionError('export must not need git')):
    stage.stage(root/'output',bundle)
    self.assertEqual((root/'output/assets/steam/cacert.pem').read_bytes(),ca)
    lock.write_text('tampered')
    with self.assertRaisesRegex(ValueError,'asset lock differs'):stage.stage(root/'bad-lock',bundle)
    self.assertFalse((root/'bad-lock').exists())
    lock.write_text(json.dumps({'source_revision':stage.REVISION,'files':{n:hashlib.sha256(v).hexdigest() for n,v in data.items()}}))
    (repo/'runtime/presets.json').write_bytes(b'tampered')
    with self.assertRaisesRegex(ValueError,'differs from the pinned source'):stage.stage(root/'bad-source',bundle)
    self.assertFalse((root/'bad-source').exists())
    (repo/'runtime/presets.json').write_bytes(data['runtime/presets.json'])
    bundle.write_bytes(b'bad')
    with self.assertRaisesRegex(ValueError,'CA bundle differs'):stage.stage(root/'refused',bundle)
    self.assertFalse((root/'refused').exists())
if __name__=='__main__':unittest.main()
