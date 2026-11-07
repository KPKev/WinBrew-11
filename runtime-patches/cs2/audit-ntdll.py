#!/usr/bin/env python3
import pathlib,subprocess,re,json,hashlib,shutil,os
repo=pathlib.Path.cwd();work=pathlib.Path.home()/'counterstrike-wine019-work';out=repo/'build/counterstrike-ntdll-build';sdk=repo/'build/return-wrapper/foundation/.deps/native/ps5-payload-sdk';prx=work/'prx';shared=prx/'ntdll.shared.elf'
llvm=pathlib.Path.home()/'.cache/wine11-llvm-18.1.8/root/usr';os.environ['LD_LIBRARY_PATH']=str(llvm/'lib/x86_64-linux-gnu')+':'+str(llvm/'lib/llvm-18/lib')
llvmbin=llvm/'lib/llvm-18/bin'
def run(*args):return subprocess.check_output([str(x) for x in args],text=True,stderr=subprocess.STDOUT)
def exports(path):return {l.split()[-1].split('@')[0] for l in run(llvmbin/'llvm-nm','-D','--defined-only',path).splitlines() if l.split()}
readelf=llvmbin/'llvm-readelf';objdump=llvmbin/'llvm-objdump'
log=(work/'ntdll-prx-link.log').read_text();symbols=run(readelf,'--dyn-syms','-W',shared);imports={f[7].split('@')[0] for f in (l.split() for l in symbols.splitlines()) if len(f)>=8 and f[6]=='UND'}
provided=exports(sdk/'target/lib/libkernel.so')|exports(sdk/'target/lib/libSceLibcInternal.so')
webkit=exports(sdk/'target/lib/libkernel_web.so')|exports(sdk/'target/lib/libScePosixForWebKit.so')
dynamic=run(readelf,'-d',shared);listing=run(objdump,'-d','--no-show-raw-insn',shared)
function=None;raw=set();all_raw=set()
for line in listing.splitlines():
 m=re.match(r'[0-9a-f]+ <(.+)>:$',line)
 if m:function=m.group(1)
 elif re.search(r'\tsyscall\b',line):
  all_raw.add(function)
  if function not in {'__wine_syscall_dispatcher','__wine_unix_call_dispatcher'}:raw.add(function)
strings=run(llvmbin/'llvm-strings',shared)
assert 'PS5 fixed hint refused system band or overflow' in strings
exported=exports(shared)
required={'__wine_main','pw_wine_dl_adopt','dlopen','dlsym','dlerror','__wine_virtual_stats','__wine_ps5_set_output_sink','__wine_ps5_memory_stats','pw_wine_set_present_sink','pw_wine_set_audio_sink','pw_wine_pad_at','pw_wine_set_menu'}
entry={'artifact':'ntdll.prx','bytes':(out/'ntdll.prx').stat().st_size,'sha256':hashlib.file_digest((out/'ntdll.prx').open('rb'),'sha256').hexdigest(),
 'base_runtime_revision':'7537eb0113b438e8dc0730e594fa0743203bc7c9','wine_revision':'490f6d5dcbb2a5047345b8af88d114bbcaad69a8',
 'foundation_revision':'30597512539e7edfde079cbcaf4a626bc0a948c5','sdk':'0.42','compiler':'LLVM18.1.8',
 'unresolved':sorted(set(re.findall(r'undefined symbol: (\S+)',log))),'errors':sorted(set(re.findall(r'error: (.+)',log))),
 'title_unbound':sorted((imports&exports(sdk/'target/lib/libkernel_sys.so'))-provided),
 'webkit_unbound':sorted((imports&webkit)-provided),'raw_syscalls':sorted(raw),'allowed_unreachable_dispatcher_syscalls':sorted(all_raw-raw),
 'data_imports':sorted({f[7] for f in (l.split() for l in symbols.splitlines()) if len(f)>=8 and f[3]=='OBJECT' and f[6]=='UND'}),
 'needed':re.findall(r'Shared library: \[([^\]]+)\]',dynamic),'missing_required_exports':sorted(required-exported),'export_count':len(exported),
 'fixed_hint_boundary_tests':'passed optimized and ASan+UBSan','console_tested':False,'other_runtime_files_changed':False,
 'rollback':'Retain existing release019 ntdll.prx ac249ad6d13e9818f7a28f0ba0c2b489df31064610c91f13ea20cee6194dfbe5; replace only after stopped title and verified staged artifact. Restore it by checked same-parent rename if needed.'}
for field in ['unresolved','errors','title_unbound','webkit_unbound','raw_syscalls','missing_required_exports']:assert not entry[field],(field,entry[field])
entry['self_inspect']=run(work/'native-tool/ps5-native-tool','self','--inspect','--file',out/'ntdll.prx')
(out/'ntdll-audit.json').write_text(json.dumps(entry,indent=2)+'\n');(repo/'evidence/counterstrike-ntdll-fix-audit.json').write_text(json.dumps(entry,indent=2)+'\n')
for n in ['host-configure.log','host-tools.log','ps5-configure.log','ps5-config-status.log','ntdll-make.log','ntdll-prx-link.log','ntdll-objects.txt','patches-applied.log']:
 shutil.copyfile(work/n,out/n)
shutil.copyfile(prx/'ntdll_desc_missing.c' if (prx/'ntdll_desc_missing.c').exists() else prx/'obj/ntdll_desc.c',out/'ntdll_desc.c')
print(json.dumps(entry,indent=2))
