import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('review',root/'tools/assemble-steam-review.py');review=importlib.util.module_from_spec(spec);spec.loader.exec_module(review)
class Tests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.assets=Path(self.tmp.name);self.runtime=b'known source asset';self.ca=b'x'*188900
  self.files={'runtime/presets.json':self.runtime,'assets/steam/cacert.pem':self.ca}
  for n,v in self.files.items():p=self.assets/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
  self.sha=lambda b:hashlib.sha256(b).hexdigest();self.pins={n:self.sha(v) for n,v in self.files.items()}
  self.module=SimpleNamespace(REVISION='fixed',CA_SHA256=self.pins['assets/steam/cacert.pem'],VAPOR_RECIPE_MANIFEST_SHA256='reviewed',expected_runtime_asset_hashes=lambda:{'runtime/presets.json':self.pins['runtime/presets.json']})
  self.patch=mock.patch.object(review,'ASSETS',self.assets);self.patch.start();self.receipt()
 def tearDown(self):self.patch.stop();self.tmp.cleanup()
 def receipt(self):
  (self.assets/'runtime-assets.json').write_text(json.dumps({'source_revision':'fixed','vapor_recipe_overlay_sha256':'reviewed','files':{n:self.sha((self.assets/n).read_bytes()) for n in self.files}}))
 def test_exported_assets_verify_without_git(self):self.assertEqual(len(review.verify_assets(self.module)['files']),2)
 def test_altered_asset_and_receipt_still_refused(self):
  (self.assets/'runtime/presets.json').write_bytes(b'forged');self.receipt()
  with self.assertRaisesRegex(ValueError,'pinned Playden source'):review.verify_assets(self.module)
 def test_unknown_file_refused(self):
  (self.assets/'extra').write_bytes(b'x')
  with self.assertRaisesRegex(ValueError,'file set differs'):review.verify_assets(self.module)
 def test_stale_recipe_receipt_refused(self):
  receipt=json.loads((self.assets/'runtime-assets.json').read_text());receipt.pop('vapor_recipe_overlay_sha256')
  (self.assets/'runtime-assets.json').write_text(json.dumps(receipt))
  with self.assertRaisesRegex(ValueError,'restage runtime assets'):review.verify_assets(self.module)
if __name__=='__main__':unittest.main()
