import importlib.util,io,json,socket,tempfile,unittest
from pathlib import Path
from unittest import mock
spec=importlib.util.spec_from_file_location('verify',Path(__file__).resolve().parents[1]/'tools/run_skyrim_stage_verify.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Conn:
 def __init__(self,content):self.content=content;self.sent=0
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def sendall(self,data):self.sent+=1
 def shutdown(self,*a):pass
 def settimeout(self,*a):pass
 def recv(self,*a):v=self.content;self.content=b'';return v
class Tests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.journal=Path(self.tmp.name)/'run.json';self.patch=mock.patch.object(m,'JOURNAL',self.journal);self.patch.start()
 def tearDown(self):self.patch.stop();self.tmp.cleanup()
 def test_default_offline(self):
  with mock.patch.object(m.urllib.request,'urlopen',side_effect=AssertionError('network')):m.main()
 def test_running_payload_refuses(self):
  with mock.patch.object(m.urllib.request,'urlopen',return_value=io.BytesIO(b'{"processes":[{"name":"payload.elf"}]}')),mock.patch.object(m.socket,'create_connection',side_effect=AssertionError('send')):
   with self.assertRaisesRegex(RuntimeError,'already active'):m.main(True)
  self.assertFalse(self.journal.exists())
 def test_done_single_send(self):
  conn=Conn(b'SKYRIM_STAGE candidate=passed errno=0\nSKYRIM_STAGE done\n')
  with mock.patch.object(m.urllib.request,'urlopen',return_value=io.BytesIO(b'{"processes":[]}')),mock.patch.object(m.socket,'create_connection',return_value=conn):m.main(True)
  self.assertEqual(conn.sent,1);self.assertTrue(json.loads(self.journal.read_text())['completed'])
  with self.assertRaisesRegex(RuntimeError,'Existing run journal'):m.main(True)
 def test_incomplete_never_retry(self):
  with mock.patch.object(m.urllib.request,'urlopen',return_value=io.BytesIO(b'{"processes":[]}')),mock.patch.object(m.socket,'create_connection',return_value=Conn(b'partial')):
   with self.assertRaisesRegex(RuntimeError,'Completion not observed'):m.main(True)
  self.assertEqual(json.loads(self.journal.read_text())['phase'],'inspect-before-retry')
if __name__=='__main__':unittest.main()
