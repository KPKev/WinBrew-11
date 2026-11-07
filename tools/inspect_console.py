"""Read-only inventory through an already-running PS5 Web File Manager.

Only list and download endpoints are called. No payload, launch, upload,
rename, permission or configuration operation is performed.
"""
import argparse,json,urllib.parse,urllib.request
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--host',required=True);p.add_argument('--port',type=int,default=8888);p.add_argument('--output',default='build/console-inventory.json');a=p.parse_args()
base=f'http://{a.host}:{a.port}'
def post(endpoint,params=None,data=None):
    url=base+endpoint+('?' + urllib.parse.urlencode(params) if params else '')
    req=urllib.request.Request(url,data=urllib.parse.urlencode(data or {}).encode(),method='POST')
    with urllib.request.urlopen(req,timeout=8) as response:return json.load(response)
result={'host':a.host,'read_only':True,'targets':{}}
for path in ['/data/homebrew','/data/homebrew/FAKE78003','/data/homebrew/FAKE78003/sce_sys','/data/playden-games/1089130/baseq2']:
    try:
        response=post('/api/list',{'path':path});result['targets'][path]=response
        print(path,':',[(e['name'],e.get('size')) for e in response.get('entries',[]) if path!='/data/playden-games/1089130/baseq2' or e['name'].startswith(('pak','autoexec','q2rtx-controller'))])
    except Exception as e:result['targets'][path]={'error':str(e)};print(path,':',type(e).__name__);break
out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2))
