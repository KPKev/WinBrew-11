"""Compare a second rebuild with the preserved console-tested PS5 archives."""
from pathlib import Path
import hashlib, json
root=Path(__file__).resolve().parents[1]
pin=json.loads((root/'evidence/steam-network-toolchain-check.json').read_text())['archives']
current=root/'vendor/playden-source/scripts/network-deps/.deps/ps5/lib'
saved=root/'build/steam-network-tested-snapshot/lib'
result={}
for name,item in pin.items():
    old=hashlib.sha256((saved/name).read_bytes()).hexdigest()
    new=hashlib.sha256((current/name).read_bytes()).hexdigest()
    result[name]={'tested_sha256':old,'rebuilt_sha256':new,'matches_tested':old==new,'saved_matches_evidence':old==item['actual']}
passed=all(x['matches_tested'] and x['saved_matches_evidence'] for x in result.values())
receipt={'status':'matched' if passed else 'mismatch','archives':result,
         'scope':'Reproducibility only; no assertion of Playden pin equivalence or release readiness.'}
(root/'evidence/steam-network-rebuild-comparison.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Rebuild comparison:',receipt['status'])
if not passed: raise SystemExit(1)
