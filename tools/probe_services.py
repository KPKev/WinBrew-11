"""Bounded TCP readiness check of known PS5 homebrew service ports."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import socket

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True)
parser.add_argument('--output', default='build/service-probe.json')
args = parser.parse_args()
ports = [*range(8888, 8896), *range(8080, 8085), 9021, 2120, 2121, 744]


def probe(port):
    try:
        with socket.create_connection((args.host, port), timeout=2):
            return {'port': port, 'open': True}
    except OSError:
        return {'port': port, 'open': False}


with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    results = list(pool.map(probe, ports))
out = Path(args.output)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'host': args.host, 'services': results}, indent=2))
print('Responding ports:', [item['port'] for item in results if item['open']])
