import importlib.util, json, sqlite3, sys, tempfile, unittest
from pathlib import Path
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
spec=importlib.util.spec_from_file_location('skyrim_installer',Path(__file__).resolve().parents[1]/'tools/install_skyrim_admission_diagnostic.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class FakeConsole:
 offline_test_transport=True
 def __init__(self, registered=False, collision=None, missing_task=False):
  self.files={};self.dirs={'/data/homebrew','/user/app','/user/appmeta','/system_data/priv/mms'};self.collision=collision;self.missing_task=missing_task;self.events=[];self.pending={};self.serial=0
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'app.db';db=sqlite3.connect(p);db.execute('create table tbl_contentinfo(titleId text)')
   if registered:db.execute('insert into tbl_contentinfo values(?)',(m.TITLE,))
   db.commit();db.close();self.files['/system_data/priv/mms/app.db']=p.read_bytes()
 def listing(self,path):
  result=[]
  for f in self.files:
   if str(Path(f).parent).replace('\\','/')==path:result.append({'name':f.rsplit('/',1)[1],'mode':0o755})
  for d in self.dirs:
   if d.rsplit('/',1)[0]==path:result.append({'name':d.rsplit('/',1)[1],'mode':0o755})
  if self.collision==path:result.append({'name':m.TITLE,'mode':0o755})
  return result
 def read(self,path):return self.files[path]
 def post(self,endpoint,data=None,params=None,headers=None):
  self.events.append((endpoint,data,params));data=data or {};params=params or {}
  if endpoint=='/api/mkdir':self.dirs.add(params['path']+'/'+params['name']);return {'ok':True}
  if endpoint=='/api/upload/prepare':
   self.serial+=1;self.pending[str(self.serial)]={'path':data['path']};return {'ok':True,'task_id':str(self.serial)}
  if endpoint=='/api/upload-file':
   from urllib.parse import unquote
   self.files[unquote(headers['X-WFM-Path'])+'/'+unquote(headers['X-WFM-Rel'])]=data;return {'ok':True}
  if endpoint in ('/api/upload/finish','/api/chmod'):
   self.serial+=1;return {'ok':True,'task_id':str(self.serial)}
  if endpoint=='/api/tasks':
   return {'ok':True,'tasks':[] if self.missing_task else [{'id':str(i),'state':'done'} for i in range(1,self.serial+1)]}
  raise AssertionError(endpoint)

class InstallerTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.status=Path(self.tmp.name)/'status.json';self.sp=mock.patch.object(m,'STATUS',self.status);self.sp.start()
 def tearDown(self):self.sp.stop();self.tmp.cleanup()
 def run_console(self,c):
  m._simulate_install_for_tests(c,c.post)
 def test_offline_default_never_contacts_console(self):
  with mock.patch.object(m,'WFM',side_effect=AssertionError('network')):m.main()
 def test_live_apply_is_disabled_before_console_contact(self):
  with mock.patch.object(m,'WFM',side_effect=AssertionError('network')):
   with self.assertRaisesRegex(RuntimeError,'Live installation disabled'):m.main(True)
 def test_untrusted_transport_refused(self):
  with self.assertRaisesRegex(RuntimeError,'Offline test transport required'):m._simulate_install_for_tests(object(),None)
 def test_existing_title_refuses_before_mutation(self):
  for path in ('/data/homebrew','/user/app','/user/appmeta'):
   c=FakeConsole(collision=path)
   with self.assertRaisesRegex(RuntimeError,'already exists'):self.run_console(c)
   self.assertEqual(c.events,[]);self.assertFalse(self.status.exists())
 def test_registered_title_refuses_before_mutation(self):
  c=FakeConsole(registered=True)
  with self.assertRaisesRegex(RuntimeError,'already registered'):self.run_console(c)
  self.assertEqual(c.events,[]);self.assertFalse(self.status.exists())
 def test_missing_task_stops_and_preserves_journal(self):
  c=FakeConsole(missing_task=True)
  with self.assertRaisesRegex(RuntimeError,'disappeared'):self.run_console(c)
  self.assertEqual(json.loads(self.status.read_text())['phase'],'failed-inspect-before-retry')
  self.assertFalse(any(e[0] in ('/api/delete','/api/rename') for e in c.events))
 def test_success_publishes_metadata_last(self):
  c=FakeConsole();self.run_console(c)
  self.assertEqual(json.loads(self.status.read_text())['phase'],'published')
  prepared=[e[1]['rels'] for e in c.events if e[0]=='/api/upload/prepare']
  self.assertEqual(prepared[-1],'sce_sys/param.json')
  self.assertTrue(all(e[1]['recursive']=='0' for e in c.events if e[0]=='/api/chmod'))
  self.assertFalse(any(e[0] in ('/api/delete','/api/rename') for e in c.events))
 def test_manifest_pin_refuses_mismatch(self):
  with mock.patch.object(m,'EXPECTED_MANIFEST','0'*64):
   with self.assertRaisesRegex(RuntimeError,'manifest differs'):m.main()
if __name__=='__main__':unittest.main()
