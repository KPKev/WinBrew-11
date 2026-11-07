"""Run the fixed read-only PS5 TLS diagnostic once through an existing ELF loader.

No credentials, files, settings or app packages are sent or changed. A timeout
is not a confirmed stop; never retry automatically or kill the payload.
"""
import argparse, hashlib, json, socket, time, urllib.request
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / 'build/steam-tls-probe/probe.elf'
EVIDENCE = ROOT / 'evidence/steam-tls-isolated-probe-wrong-ca.json'
EXPECTED = '5dc7ad7c018aa5f8f5cd140cc7b08dcef7cc36c19a9bc40dc22476ede2cefc79'
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=9021)
    parser.add_argument('--run', action='store_true')
    args=parser.parse_args()
    data=PROBE.read_bytes(); sha=hashlib.sha256(data).hexdigest()
    if sha != EXPECTED or not data.startswith(b'\x7fELF') or len(data)>3*1024*1024:
        raise RuntimeError('Diagnostic binary differs from the reviewed test; do not send it.')
    print('Read-only TLS diagnostic; no Steam account, app install, settings or game writes.')
    print('Probe SHA-256:',sha,'loader:',args.host,args.port,flush=True)
    if not args.run: print('Plan only.'); return
    if EVIDENCE.exists():
        raise RuntimeError('This diagnostic already has a recorded run; inspect it before any further test.')
    # The stock loader names raw submissions payload.elf. Refuse another send
    # while any such process is listed; never terminate or replace a process.
    with urllib.request.urlopen(f'http://{args.host}:8084/processes_list',timeout=5) as response:
        processes=json.load(response).get('processes',[])
    if any(str(p.get('name','')).lower() == 'payload.elf' for p in processes):
        raise RuntimeError('A loader payload is still listed; no diagnostic was sent.')
    output=bytearray(); error=''; complete=False; observed_eof=False
    try:
        with socket.create_connection((args.host,args.port),timeout=5) as connection:
            connection.settimeout(15); connection.sendall(data); connection.shutdown(socket.SHUT_WR)
            deadline=time.monotonic()+58
            while time.monotonic()<deadline:
                connection.settimeout(min(1.0,max(.01,deadline-time.monotonic())))
                try: chunk=connection.recv(4096)
                except socket.timeout: continue
                if not chunk: observed_eof=True; break
                output.extend(chunk)
                if len(output)>32768: raise RuntimeError('Output limit exceeded; no retry attempted.')
                if b'TLS_PROBE done success=' in output: complete=True; break
        if not complete: error='Completion not observed; no retry or termination attempted.'
    except (OSError,RuntimeError) as exc: error=type(exc).__name__ + ': ' + str(exc)
    text=output.decode('utf-8',errors='replace')
    print(text,end='' if text.endswith('\n') or not text else '\n')
    if error: print(error)
    evidence={'probe_sha256':sha,'console_host':args.host,'loader_port':args.port,
              'completion_observed':complete,'socket_eof_observed':observed_eof,'success_observed':complete and 'TLS_PROBE done success=1' in text,
              'output':text,'error':error,'credentials_used':False,'console_files_written':False,
              'changes_to_installed_app':False,'automatic_retry':False,
              'note':'Isolated diagnostic; does not establish Steam sign-in, game download, or release readiness.'}
    EVIDENCE.write_text(json.dumps(evidence,indent=2)+'\n')
    if error or not evidence['success_observed']: raise SystemExit(1)
if __name__=='__main__': main()
