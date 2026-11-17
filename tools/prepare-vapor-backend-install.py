#!/usr/bin/env python3
"""Prepare a finite console staging plan and native transaction driver.

The native preparation phase copies unchanged app files on-console and overlays
only verified new files. Application of the staged pair uses the same tested
UpdateService transaction as Vapor. Neither generation nor upload installs it.
"""
import hashlib
import json
from pathlib import Path
import uuid
import prepare_cs2_runtime as cs2
import prepare_blender_runtime as blender

root=Path(__file__).resolve().parents[1]
out=root/'build/vapor-backend-upgrade/install';out.mkdir(exist_ok=False)
app=root/'build/vapor-backend-upgrade/package/PPSA77382'
old=json.loads((root/'build/dungeon-dx11/manifest.json').read_text())
new=json.loads((app/'manifest.json').read_text())
baseline=json.loads((root/'build/vapor-backend-upgrade/console-baseline.json').read_text())
kit=json.loads((root/'build/vapor-backend-upgrade/kit-stage-status.json').read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert kit['phase']=='staged-inventory-checked'
assert old['files']['eboot.bin']==baseline['eboot_sha256']
modes={n:0o755 for n in new['files'] if n in ('eboot.bin','lapy.elf','vapor-title-remover.elf') or n.startswith('sce_module/')}
for policy in (cs2,blender):modes.update({policy.PREFIX+e['path']:int(e['mode'],8) for e in policy.entries()})
files=dict(new['files']);files['manifest.json']=sha(app/'manifest.json')
old_files=dict(old['files']);old_files['manifest.json']=baseline['manifest_sha256']
stage='/data/vapor-backend-app-stage-'+uuid.uuid4().hex[:12]
changed=[n for n,h in files.items() if old_files.get(n)!=h]
plan={'stage':stage,'app_source':str(app),'old_manifest_sha256':baseline['manifest_sha256'],'old_eboot_sha256':baseline['eboot_sha256'],
      'old_kit_sha256':'1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc',
      'kit_stage':kit['path']+'/playden-kit','new_kit_sha256':'ef0af8cb8a39ff5c6327d795873705ac79be4fc4109855181839bbbec06a3432',
      'files':files,'changed':changed,'modes':modes,'old_files':old_files,'upload_bytes':sum((app/n).stat().st_size for n in changed)}
(out/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
q=json.dumps
table='\n'.join('{'+','.join([q(n),q(h),str(modes.get(n,0o644)),'true' if n in changed else 'false',q(old_files[n]) if n in old_files else 'nullptr'])+'},' for n,h in sorted(files.items()))
state=json.dumps({'phase':'staged','version':'1.18.0','stage':stage+'/app','kit':plan['kit_stage'],'kit_id':'playden-kit','notes':'Vapor backend 0.11.0, runtime 0.28 and automatic update controls.'})+'\n'
code=r'''
#include "runtime/artifact_hash.h"
#include "update/update_service.h"
#include "update/file_ops.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cerrno>
#include <string>
#include <vector>
struct File { const char *name,*hash; unsigned mode; bool incoming; const char *old; };
static const File files[] = {
@TABLE@
};
const std::string live="/data/homebrew/PPSA77382", stage=@STAGE@, kit=@KIT@;
const std::string work="/data/wine11/self-update/PPSA77382";
void log(const std::string& line){std::puts(line.c_str());std::fflush(stdout);}
bool parents(const std::string& path,bool create=true){for(std::size_t i=1;i<path.size();++i)if(path[i]=='/'){auto p=path.substr(0,i);struct stat s{};if(lstat(p.c_str(),&s)){if(!create||errno!=ENOENT||mkdir(p.c_str(),0755))return false;}else if(!S_ISDIR(s.st_mode))return false;}return true;}
bool hash(const std::string& path,const char* expected){struct stat s{};if(!parents(path,false)||lstat(path.c_str(),&s)||!S_ISREG(s.st_mode)||s.st_nlink!=1)return false;auto got=playden::runtime::HashArtifactFile(path);return got&&*got==expected;}
bool base(){return hash(live+"/manifest.json",@OLD_MANIFEST@)&&hash(live+"/eboot.bin",@OLD_EBOOT@)&&hash("/data/wine11/kits/playden-kit/manifest.json",@OLD_KIT@)&&hash(kit+"/manifest.json",@NEW_KIT@);}
bool copy(const std::string& source,const std::string& target,const File& entry){
 if(!hash(source,entry.hash)||!parents(target))return false;
 int input=open(source.c_str(),O_RDONLY|O_NOFOLLOW);if(input<0)return false;
 int output=open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(output<0){close(input);return false;}
 std::vector<char> buffer(1<<20);bool ok=true;ssize_t got;
 while(ok&&(got=read(input,buffer.data(),buffer.size()))>0){std::size_t sent=0;while(sent<static_cast<std::size_t>(got)){auto count=write(output,buffer.data()+sent,static_cast<std::size_t>(got)-sent);if(count<=0){ok=false;break;}sent+=count;}}
 if(got<0||fchmod(output,entry.mode)||fsync(output))ok=false;close(input);if(close(output))ok=false;
 return ok&&hash(target,entry.hash);
}
int main(){
 if(seteuid(0)||!base()){log("VAPOR_PAIR failed=baseline");return 1;}
#ifndef VAPOR_APPLY_PAIR
 if(mkdir((stage+"/app").c_str(),0755)){log("VAPOR_PAIR failed=existing-stage");return 1;}
 std::size_t count=0;
 for(const auto& entry:files){
  if(entry.old&&!hash(live+"/"+entry.name,entry.old)){log("VAPOR_PAIR failed=changed-baseline file="+std::string(entry.name));return 1;}
  if(!copy((entry.incoming?stage+"/incoming":live)+"/"+entry.name,stage+"/app/"+entry.name,entry)){log("VAPOR_PAIR failed=copy file="+std::string(entry.name));return 1;}
  if(++count%256==0)log("VAPOR_PAIR prepared="+std::to_string(count));
 }
 if(!parents(work+"/state.json")||chmod(work.c_str(),0777)){log("VAPOR_PAIR failed=work-root");return 1;}
 const auto state=std::string(@STATE@);
 int fd=open((work+"/state.json").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0666);
 if(fd<0){log("VAPOR_PAIR failed=existing-state");return 1;}
 const bool saved=write(fd,state.data(),state.size())==static_cast<ssize_t>(state.size())&&fchmod(fd,0666)==0&&fsync(fd)==0;close(fd);
 if(!saved){log("VAPOR_PAIR failed=state");return 1;}
 log("VAPOR_PAIR prepared complete files="+std::to_string(count));return 0;
#else
 for(const auto& entry:files)if(!hash(stage+"/app/"+entry.name,entry.hash)){log("VAPOR_PAIR failed=staged-app");return 1;}
 playden::update::UpdateConfig config;
 config.title_id="PPSA77382";config.current={1,17,0};config.installed_root=live;config.work_root=work;config.kits_root="/data/wine11/kits";
 config.appmeta_root="/user/appmeta/PPSA77382";config.registered_app_root="/user/app/PPSA77382";
 config.as_root=[](const std::function<bool()>& operation){return operation();};
 config.verify_kit=[](const std::string& path){return playden::runtime::HashArtifactFile(path+"/manifest.json");};
 config.log=[](const std::string& line){log(line);};
 playden::update::UpdateService updater(std::move(config));updater.Start();
 if(updater.Snapshot().phase!=playden::update::UpdatePhase::kStaged){log("VAPOR_PAIR failed=staged-state");return 1;}
 updater.RequestRestart();updater.Shutdown();
 if(!updater.ApplyAtQuit()){log("VAPOR_PAIR failed=transaction");return 1;}
 for(const auto& entry:files)if(!hash(live+"/"+entry.name,entry.hash)){log("VAPOR_PAIR failed=installed-app");return 1;}
 if(!hash("/data/wine11/kits/playden-kit/manifest.json",@NEW_KIT@)){log("VAPOR_PAIR failed=installed-kit");return 1;}
 log("VAPOR_PAIR installed verified");return 0;
#endif
}
'''
for key,value in {'TABLE':table,'STAGE':q(stage),'KIT':q(plan['kit_stage']),'OLD_MANIFEST':q(plan['old_manifest_sha256']),'OLD_EBOOT':q(plan['old_eboot_sha256']),'OLD_KIT':q(plan['old_kit_sha256']),'NEW_KIT':q(plan['new_kit_sha256']),'STATE':q(state)}.items():code=code.replace('@'+key+'@',value)
(out/'pair.cpp').write_text(code)
print('Prepared finite plan:',len(files),'app files;',len(changed),'uploads;',plan['upload_bytes'],'bytes')
