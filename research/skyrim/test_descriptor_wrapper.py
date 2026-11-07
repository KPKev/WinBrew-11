#!/usr/bin/env python3
import hashlib,json,subprocess,sys
from pathlib import Path
script=Path(__file__).with_name('parse_descriptor_wrapper.py')
valid={'schema_version':1,'app_id':489830,'settings_revision':7,'title_id':'FAKE78005',
       'install_id':'install-489830','data_root':'/data/playden/games/install-489830',
       'default_profile_id':'playden-steam-489830','runtime_bundle_id':'wine-wow64',
       'recipe_id':'steam-489830','recipe_version':'1','exit_policy':'exit-title'}
def check(data,expected):
    p=subprocess.run([sys.executable,str(script)],input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
    out=json.loads(p.stdout); assert out['verdict']==expected and out['expected_title']=='FAKE78005',out
    if len(data)<=8192:
        assert out['length']==len(data) and out['sha256']==hashlib.sha256(data).hexdigest()
    assert b'install-489830' not in p.stdout
    assert p.returncode==(0 if expected=='accepted' else 1)
check(json.dumps(valid).encode(),'accepted')
wrong=dict(valid,title_id='FAKE78004');check(json.dumps(wrong).encode(),'rejected')
check(b'{"secret":"do-not-print"}','rejected')
check(b'x'*8193,'oversize')
print('full descriptor wrapper: valid fixture, wrong title, malformed and oversize passed')
