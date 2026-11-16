import importlib.util
from pathlib import Path
import tempfile, unittest, os
spec=importlib.util.spec_from_file_location('compat',Path(__file__).resolve().parents[1]/'runtime-patches/blender/launcher/winbrew_blender_startup.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
class Recycle(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.docs=self.root/'Documents';self.docs.mkdir();c.ROOTS=((str(self.docs),'/data/WinBrew/Users/Default/Documents'),)
 def tearDown(self):self.temp.cleanup()
 def test_preserve_bytes_and_restore_marker(self):
  p=self.docs/'scene.blend';p.write_bytes(b'BLENDER-test-scene');target=Path(c.recycle(c.capture(str(p))))
  self.assertRegex(target.parent.name, r'^\d+-\d+$');self.assertFalse(p.exists());self.assertEqual(target.read_bytes(),b'BLENDER-test-scene');self.assertEqual((target.parent/'.winbrew-original-location').read_text(),'WINBREW-ORIGIN-1\n/data/WinBrew/Users/Default/Documents/scene.blend')
 def test_directory_preserved(self):
  p=self.docs/'Project';p.mkdir();(p/'asset').write_bytes(b'asset');target=Path(c.recycle(c.capture(str(p))));self.assertEqual((target/'asset').read_bytes(),b'asset')
 def test_changed_file_retained(self):
  p=self.docs/'scene';p.write_bytes(b'a');item=c.capture(str(p));p.write_bytes(b'longer')
  with self.assertRaises(ValueError):c.recycle(item)
  self.assertTrue(p.exists())
 def test_outside_and_link_refused(self):
  p=self.root/'outside';p.write_bytes(b'keep');link=self.docs/'linked';link.symlink_to(p)
  for path in (p,link,self.docs):
   with self.assertRaises(ValueError):c.capture(str(path))
  self.assertEqual(p.read_bytes(),b'keep')
 def test_recycle_store_symlink_refused(self):
  (self.docs/'.winbrew-trash').symlink_to(self.root,target_is_directory=True);p=self.docs/'scene';p.write_bytes(b'keep')
  with self.assertRaises(ValueError):c.recycle(c.capture(str(p)))
  self.assertTrue(p.exists())
 def test_existing_recycled_name_not_overwritten(self):
  for content in (b'first',b'second'):
   p=self.docs/'same';p.write_bytes(content);c.recycle(c.capture(str(p)))
  self.assertEqual(sorted(x.read_bytes() for x in (self.docs/'.winbrew-trash').glob('*/same')),[b'first',b'second'])
if __name__=='__main__':unittest.main()
