"""Plan or run one bounded, read-only loader-context staging verification.
No app launch, elevation, publication, deletion, or console file mutation.
A result applies to loader credentials only, not to a title's credentials.
"""
import argparse,hashlib,json,os,socket,time,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROBE=ROOT/'build/skyrim-publisher-probe/stage-12770a-verify-only.elf'
PLAN=ROOT/'build/skyrim-staging-plans/12770aab2c584af6836d427446926c42.json'
JOURNAL=ROOT/'build/skyrim-staging-plans/12770aab2c584af6836d427446926c42-verify-run.json'
EXPECTED='e726a0f2842ea9019d54cc888254acb2bd0f8654d2b6abe3fb4c091912b92a62'
STAGE='/data/wine11-skyrim-stage-12770aab2c584af6836d427446926c42'
def validate():
 data=PROBE.read_bytes();plan=json.loads(PLAN.read_text())
 if not data.startswith(b'\x7fELF') or len(data)>1024*1024 or hashlib.sha256(data).hexdigest()!=EXPECTED:raise RuntimeError('Probe differs from reviewed binary')
 if plan.get('stage')!=STAGE or plan.get('phase')!='verified-transport-only' or not plan.get('console_written') or plan.get('publication_authorized') is not False:raise RuntimeError('Staging plan is not verified or identity differs')
 if len(plan.get('files',{}))!=27 or sorted(plan.get('verified_files',[]))!=sorted(plan['files']):raise RuntimeError('Staging file evidence is incomplete')
 return data
def main(run=False):
 data=validate()
 print('Stage-specific read-only probe SHA-256:',EXPECTED)
 if not run: print('Plan only; no console operation performed.');return
 if JOURNAL.exists():raise RuntimeError('Existing run journal: inspect it and the console before further action')
 with urllib.request.urlopen('http://192.168.7.26:8084/processes_list',timeout=5) as response:
  body=json.load(response);processes=body.get('processes')
 if not isinstance(processes,list):raise RuntimeError('Process listing unavailable')
 if any(str(p.get('name','')).lower()=='payload.elf' for p in processes):raise RuntimeError('A loader payload is already active')
 state={'phase':'dispatching','probe_sha256':EXPECTED,'completed':False,'automatic_retry':False,'console_file_writes':False,'registration':False}
 with JOURNAL.open('x',encoding='utf-8') as f:json.dump(state,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
 output=bytearray();error=''
 try:
  with socket.create_connection(('192.168.7.26',9021),timeout=5) as conn:
   conn.sendall(data);conn.shutdown(socket.SHUT_WR);deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    conn.settimeout(min(1.0,max(.01,deadline-time.monotonic())))
    try: chunk=conn.recv(4096)
    except socket.timeout:continue
    if not chunk:break
    if len(output)+len(chunk)>4096:raise RuntimeError('Output exceeded bound; inspect before retry')
    output.extend(chunk)
    if b'SKYRIM_STAGE done\n' in output:state['completed']=True;break
  if not state['completed']:error='Completion not observed; inspect the console before any retry'
 except (OSError,RuntimeError) as exc:error=str(exc)
 state.update(phase='completed' if state['completed'] else 'inspect-before-retry',output=output.decode('utf-8','replace'),error=error)
 JOURNAL.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8')
 print(state['output'],end='')
 if error:raise RuntimeError(error)
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');main(parser.parse_args().run)
