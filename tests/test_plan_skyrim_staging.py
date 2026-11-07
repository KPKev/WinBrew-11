import importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest import mock
spec=importlib.util.spec_from_file_location('stage',Path(__file__).resolve().parents[1]/'tools/plan_skyrim_staging.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class PlanTests(unittest.TestCase):
 def test_unique_and_exact(self):
  a=m.plan();b=m.plan();self.assertNotEqual(a['stage'],b['stage']);self.assertEqual(len(a['files']),27);self.assertFalse(a['console_written']);self.assertFalse(a['publication_authorized'])
 def test_journal_never_overwrites(self):
  with tempfile.TemporaryDirectory() as d, mock.patch.object(m,'PLAN_DIR',Path(d)):
   a=m.plan();p=m.save(a);before=p.read_bytes()
   with self.assertRaises(FileExistsError):m.save(a)
   self.assertEqual(before,p.read_bytes());self.assertEqual(json.loads(before)['manifest_sha256'],m.PIN)
 def test_fake_staging_is_exclusive_and_never_deletes(self):
  class Fake:
   offline_test_transport=True
   def __init__(self):self.dirs={};self.calls=[]
   def mkdir_exclusive(self,path):
    self.calls.append(('mkdir',path))
    if path in self.dirs:raise FileExistsError(path)
    self.dirs[path]={}
   def put_exclusive(self,stage,name,data):
    self.calls.append(('put',stage,name))
    if name in self.dirs[stage]:raise FileExistsError(name)
    self.dirs[stage][name]=data
   def read(self,stage,name):return self.dirs[stage][name]
  f=Fake();p=m.plan();result=m.simulate_stage(p,f)
  self.assertTrue(result['verified']);self.assertFalse(result['published']);self.assertEqual(len(f.dirs[p['stage']]),27)
  before=dict(f.dirs[p['stage']])
  with self.assertRaises(FileExistsError):m.simulate_stage(p,f)
  self.assertEqual(before,f.dirs[p['stage']]);self.assertTrue(all(x[1]==p['stage'] for x in f.calls))
 def test_transport_success_and_interrupt_preserve_partial(self):
  class Transport:
   def __init__(self,fail=False):self.files={};self.dirs=set();self.fail=fail;self.ops=[]
   def mkdir_exclusive(self,stage):self.ops.append(('mkdir',stage))
   def mkdir_child(self,stage,name):self.dirs.add(name)
   def put_exclusive(self,stage,name,data):
    if self.fail and self.files:raise RuntimeError('injected failure')
    if name in self.files:raise FileExistsError(name)
    self.files[name]=data
   def read(self,stage,name):return self.files[name]
  snapshots=[];a=m.plan();t=Transport();m.stage_to_transport(a,t,lambda x:snapshots.append(x['phase']))
  self.assertEqual(a['phase'],'verified-transport-only');self.assertEqual(len(t.files),27);self.assertFalse(a['publication_authorized']);self.assertEqual(snapshots[0],'starting')
  b=m.plan();broken=Transport(True)
  with self.assertRaises(RuntimeError):m.stage_to_transport(b,broken,lambda x:None)
  self.assertEqual(b['phase'],'inspect-before-retry');self.assertEqual(len(broken.files),1)
  with self.assertRaisesRegex(ValueError,'automatic retry refused'):m.stage_to_transport(b,broken,lambda x:None)
 def test_real_transport_refused(self):
  with self.assertRaisesRegex(ValueError,'Offline test transport'):m.simulate_stage(m.plan(),object())
 def test_pin_mismatch_rejected(self):
  with mock.patch.object(m,'PIN','0'*64):
   with self.assertRaisesRegex(ValueError,'Manifest differs'):m.plan()
if __name__=='__main__':unittest.main()
