import importlib.util,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import prospero_validator_source as source
spec=importlib.util.spec_from_file_location('fetch',ROOT/'tools/fetch-prospero-validator.py');fetch=importlib.util.module_from_spec(spec);spec.loader.exec_module(fetch)
class SourceTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.repo=Path(self.temp.name)/'repo';self.repo.mkdir();(self.repo/'tools').mkdir();(self.repo/'tools/package_playden_kit.py').write_text('# fixture\n')
  def git(*args):subprocess.run(['git','-C',str(self.repo),*args],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  git('init');git('config','core.autocrlf','false');git('config','user.email','test@example.invalid');git('config','user.name','Fixture');git('add','tools');git('commit','-m','fixture')
  self.revision=subprocess.check_output(['git','-C',str(self.repo),'rev-parse','HEAD'],text=True).strip();self.pin=mock.patch.object(source,'REVISION',self.revision);self.pin.start()
 def tearDown(self):self.pin.stop();self.temp.cleanup()
 def test_clean_exact_source_passes_and_existing_fetch_does_not_clone(self):
  self.assertEqual(source.verify_source(self.repo),self.repo.resolve())
  with mock.patch.object(fetch,'verify_source',return_value=self.repo.resolve()) as check:
   self.assertEqual(fetch.prepare(self.repo),self.repo.resolve());check.assert_called_once_with(self.repo)
 def test_fresh_clone_preserves_lf_with_global_autocrlf(self):
  script=self.repo/'tools/build_native.sh';script.write_bytes(b'#!/bin/sh\nprintf ready\n')
  subprocess.run(['git','-C',str(self.repo),'add','tools/build_native.sh'],check=True)
  subprocess.run(['git','-C',str(self.repo),'commit','-m','shell fixture'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  rev=subprocess.check_output(['git','-C',str(self.repo),'rev-parse','HEAD'],text=True).strip()
  config=Path(self.temp.name)/'gitconfig';config.write_text('[core]\n\tautocrlf = true\n')
  destination=Path(self.temp.name)/'fresh'
  with mock.patch.dict('os.environ',{'GIT_CONFIG_GLOBAL':str(config)}), mock.patch.object(source,'REVISION',rev), mock.patch.object(fetch,'REVISION',rev), mock.patch.object(fetch,'URL',str(self.repo)):
   self.assertEqual(fetch.prepare(destination),destination.resolve())
  self.assertEqual((destination/'tools/build_native.sh').read_bytes(),b'#!/bin/sh\nprintf ready\n')
  self.assertEqual(subprocess.check_output(['git','-C',str(destination),'config','--local','core.autocrlf'],text=True).strip(),'false')
 def test_wrong_revision_and_dirty_refused_without_modification(self):
  with mock.patch.object(source,'REVISION','0'*40):
   with self.assertRaisesRegex(ValueError,'revision differs'):source.verify_source(self.repo)
  p=self.repo/'tools/package_playden_kit.py';p.write_text('changed\n')
  with self.assertRaisesRegex(ValueError,'local changes'):source.verify_source(self.repo)
  self.assertEqual(p.read_text(),'changed\n')
 def test_untracked_files_refused(self):
  (self.repo/'unexpected').write_text('keep')
  with self.assertRaisesRegex(ValueError,'local changes'):source.verify_source(self.repo)
 def test_non_repository_refused(self):
  with self.assertRaises(ValueError):source.verify_source(Path(self.temp.name))
if __name__=='__main__':unittest.main()
