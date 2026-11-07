"""Prepare a unique, local-only staging plan for the Skyrim diagnostic.
Default is offline. Explicit --transfer uses a new staging directory only.
Never deletes prior plans or console data.
"""
import argparse,hashlib,json,os,re,uuid
from pathlib import Path,PurePosixPath

ROOT=Path(__file__).resolve().parents[1]
PACKAGE=ROOT/'build/skyrim-admission-isolated-6/boilerplate/dist/PPSA77383'
PIN='0948259b752242d1f10317744fed0654fb9d86a7b16d5c04e5e67c58cfad5b51'
PLAN_DIR=ROOT/'build/skyrim-staging-plans'

def digest(data): return hashlib.sha256(data).hexdigest()
def package():
 raw=(PACKAGE/'manifest.json').read_bytes()
 if digest(raw)!=PIN:raise ValueError('Manifest differs from pinned candidate')
 manifest=json.loads(raw);expected=manifest['files']
 actual={p.relative_to(PACKAGE).as_posix() for p in PACKAGE.rglob('*') if p.is_file() and p.name!='manifest.json'}
 if any(p.is_symlink() for p in PACKAGE.rglob('*')) or actual!=set(expected):raise ValueError('Package inventory differs')
 for n,h in expected.items():
  p=PurePosixPath(n)
  if p.is_absolute() or any(c in ('.','..') for c in p.parts) or '\\' in n or digest((PACKAGE/n).read_bytes())!=h:raise ValueError('Package path or digest differs')
 if manifest.get('title_id')!='PPSA77383' or manifest.get('not_install_ready') is not True:raise ValueError('Candidate identity differs')
 return {**expected,'manifest.json':PIN}

def plan():
 files=package();stage='/data/wine11-skyrim-stage-'+uuid.uuid4().hex
 if not re.fullmatch(r'/data/wine11-skyrim-stage-[0-9a-f]{32}',stage):raise ValueError('Invalid staging identity')
 return {'stage':stage,'title':'PPSA77383','files':files,'manifest_sha256':PIN,'console_written':False,'publication_authorized':False,'requires_new_stage_specific_payload':True}

def save(value):
 PLAN_DIR.mkdir(parents=True,exist_ok=True)
 dest=PLAN_DIR/(value['stage'].rsplit('-',1)[1]+'.json')
 with dest.open('x',encoding='utf-8') as f:json.dump(value,f,indent=2,sort_keys=True);f.write('\n');f.flush();os.fsync(f.fileno())
 return dest

def simulate_stage(value, transport):
 """Exercise upload protocol with an explicitly offline fake, never a console."""
 if not getattr(transport,'offline_test_transport',False):raise ValueError('Offline test transport required')
 expected=package()
 if value.get('files')!=expected or not re.fullmatch(r'/data/wine11-skyrim-stage-[0-9a-f]{32}',value.get('stage','')):raise ValueError('Plan differs from candidate')
 stage=value['stage']
 transport.mkdir_exclusive(stage)
 for name, expected_hash in sorted(expected.items()):
  data=(PACKAGE/name).read_bytes()
  if digest(data)!=expected_hash:raise ValueError('Candidate changed during staging')
  transport.put_exclusive(stage,name,data)
  if digest(transport.read(stage,name))!=expected_hash:raise ValueError('Staged bytes differ')
 return {'stage':stage,'verified':True,'published':False}

class WfmStagingTransport:
 """New staging directory only; no deletion or writes to title directories."""
 def __init__(self):
  from install_console import WFM
  from probe_ps5_kit_extraction import post,wait
  self.reader=WFM('192.168.7.26',8888);self.post=post;self.wait=wait
 def mkdir_exclusive(self,path):
  parent, name=path.rsplit('/',1)
  if parent!='/data' or not re.fullmatch(r'wine11-skyrim-stage-[0-9a-f]{32}',name):raise ValueError('Unexpected staging destination')
  if any(e.get('name')==name for e in self.reader.listing(parent)):raise FileExistsError(path)
  self.post('/api/mkdir',params={'path':parent,'name':name})
 def mkdir_child(self,stage,name):
  parent,leaf=name.rsplit('/',1) if '/' in name else ('',name)
  self.post('/api/mkdir',params={'path':stage+('/'+parent if parent else ''),'name':leaf})
 def put_exclusive(self,stage,name,data):
  import urllib.parse
  task=self.post('/api/upload/prepare',{'path':stage,'src':name,'total':str(len(data)),'count':'1','rels':name,'sizes':str(len(data)),'overwrite':'0'})['task_id']
  self.post('/api/upload-file',data,headers={'Content-Type':'application/octet-stream','X-WFM-Task-ID':str(task),'X-WFM-Path':urllib.parse.quote(stage,safe=''),'X-WFM-Rel':urllib.parse.quote(name,safe=''),'X-WFM-Size':str(len(data)),'X-WFM-Overwrite':'0'})
  self.post('/api/upload/finish',params={'task_id':task});self.wait(task)
 def read(self,stage,name):return self.reader.read(stage+'/'+name)

def stage_to_transport(value,transport,journal):
 """Bounded new-directory transfer. Leaves every partial result for inspection."""
 expected=package();stage=value.get('stage','')
 if value.get('files')!=expected or not re.fullmatch(r'/data/wine11-skyrim-stage-[0-9a-f]{32}',stage):raise ValueError('Plan differs from pinned candidate')
 if value.get('phase') not in (None,'planned'):raise ValueError('Inspect prior stage; automatic retry refused')
 value['phase']='starting';journal(value)
 try:
  transport.mkdir_exclusive(stage);value['phase']='created';journal(value)
  directories=sorted({str(p) for n in expected for p in PurePosixPath(n).parents if str(p)!='.'},key=lambda p:(p.count('/'),p))
  for d in directories:transport.mkdir_child(stage,d)
  value['phase']='uploading';value['verified_files']=[];journal(value)
  for name, expected_hash in sorted(expected.items()):
   data=(PACKAGE/name).read_bytes()
   if digest(data)!=expected_hash:raise ValueError('Candidate changed during transfer')
   transport.put_exclusive(stage,name,data)
   if digest(transport.read(stage,name))!=expected_hash:raise ValueError('Staged bytes differ')
   value['verified_files'].append(name);journal(value)
  value['phase']='verified-transport-only';value['console_written']=True;journal(value)
 except Exception:
  value['phase']='inspect-before-retry';value['console_written']=True;journal(value);raise
 return value

def transfer(plan_path):
 path=Path(plan_path).resolve();root=PLAN_DIR.resolve()
 if path.parent!=root or not re.fullmatch(r'[0-9a-f]{32}\.json',path.name):raise ValueError('Use a plan from the staging plan directory')
 value=json.loads(path.read_text(encoding='utf-8'))
 if value.get('stage','').rsplit('-',1)[-1]!=path.stem:raise ValueError('Plan identity differs')
 def journal(state):
  temporary=root/(path.stem+'.'+uuid.uuid4().hex+'.tmp')
  with temporary.open('x',encoding='utf-8') as f:
   json.dump(state,f,indent=2,sort_keys=True);f.write('\n');f.flush();os.fsync(f.fileno())
  os.replace(temporary,path)
 return stage_to_transport(value,WfmStagingTransport(),journal)

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--transfer',metavar='PLAN',help='explicitly transfer a prepared plan to its new console staging directory')
 args=parser.parse_args()
 if args.transfer:
  result=transfer(args.transfer);print('Transport verification:',result['phase']);print('No publication or registration was attempted.')
 else:
  value=plan();dest=save(value);print('Local plan:',dest);print('No console files changed. Transfer requires explicit --transfer.')
if __name__=='__main__':main()
