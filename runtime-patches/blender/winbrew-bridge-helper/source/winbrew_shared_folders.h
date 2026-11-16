/* SPDX-License-Identifier: MIT */
#ifndef WB_DATA_ROOT
#define WB_DATA_ROOT "/data"
#endif
/* Scoped startup reconciliation for the two owner-approved folders. No content
 * copies, symlink following, UID changes, or unrelated hidden application metadata. Recycle stores are shared explicitly. */
struct wb_undo { char *path; struct stat before; struct wb_undo *previous; };
static struct wb_undo *wb_undo_head;
static unsigned wb_entries;
static int wb_remember(const char *path,const struct stat *before) {
    struct wb_undo *entry=calloc(1,sizeof(*entry));if(!entry)return ENOMEM;
    entry->path=strdup(path);if(!entry->path){free(entry);return ENOMEM;}
    entry->before=*before;entry->previous=wb_undo_head;wb_undo_head=entry;return 0;
}
static int wb_finish(int rollback) {
    int error=0;
    while(wb_undo_head){
        struct wb_undo *entry=wb_undo_head;wb_undo_head=entry->previous;
        if(rollback){
            struct stat named,held;int fd=-1;
            if(seteuid(0)||lstat(entry->path,&named)||named.st_dev!=entry->before.st_dev||named.st_ino!=entry->before.st_ino||named.st_uid!=entry->before.st_uid||
               seteuid(entry->before.st_uid)||(fd=open(entry->path,O_RDONLY|O_NOFOLLOW|(S_ISDIR(entry->before.st_mode)?O_DIRECTORY:0)))<0||
               fstat(fd,&held)||held.st_dev!=entry->before.st_dev||held.st_ino!=entry->before.st_ino||
               fchown(fd,(uid_t)-1,entry->before.st_gid)||fchmod(fd,entry->before.st_mode&07777))error=EIO;
            if(fd>=0)close(fd);
        }
        free(entry->path);free(entry);
    }
    if(seteuid(0))error=EIO;return error;
}
static int wb_share_path(const char *path, unsigned depth, int recycle) {
    struct stat before, opened, named;
    int fd=-1, error=0;
    if(depth>32 || ++wb_entries>100000)return E2BIG;
    if(seteuid(0)||lstat(path,&before))return errno?errno:EIO;
    if(S_ISLNK(before.st_mode))return 0;
    if(!S_ISDIR(before.st_mode)&&!S_ISREG(before.st_mode))return EINVAL;
    if(before.st_uid>1 || (before.st_gid!=0&&before.st_gid!=1&&before.st_gid!=WB_GROUP_ID) ||
       (S_ISREG(before.st_mode)&&before.st_nlink!=1))return EPERM;
    if(seteuid(before.st_uid))return errno;
    fd=open(path,O_RDONLY|O_NOFOLLOW|(S_ISDIR(before.st_mode)?O_DIRECTORY:0));
    if(fd<0){error=errno;goto done;}
    if(fstat(fd,&opened)||opened.st_dev!=before.st_dev||opened.st_ino!=before.st_ino||opened.st_uid!=before.st_uid){error=ESTALE;goto done;}
    unsigned mode=(before.st_mode&0700)|((before.st_mode&0700)>>3);
    if((before.st_gid!=WB_GROUP_ID||(before.st_mode&07777)!=mode)&&(error=wb_remember(path,&before)))goto done;
    if((before.st_gid!=WB_GROUP_ID&&fchown(fd,(uid_t)-1,WB_GROUP_ID))||
       ((before.st_mode&07777)!=mode&&fchmod(fd,mode))){error=errno;goto done;}
    if(lstat(path,&named)||named.st_dev!=before.st_dev||named.st_ino!=before.st_ino){
        error=ESTALE;
        if(fchown(fd,(uid_t)-1,before.st_gid)||fchmod(fd,before.st_mode&07777))error=EIO;
        goto done;
    }
    if(S_ISDIR(before.st_mode)){
        DIR *dir=fdopendir(fd);if(!dir){error=errno;goto done;}fd=-1;
        struct dirent *entry;
        for(;;){
            errno=0;entry=readdir(dir);if(!entry){if(errno)error=errno;break;}
            if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
            int child_recycle=recycle||!strcmp(entry->d_name,".winbrew-trash");
            if(entry->d_name[0]=='.'&&!child_recycle)continue;
            char child[1024];int n=snprintf(child,sizeof child,"%s/%s",path,entry->d_name);
            if(n<0||(size_t)n>=sizeof child){error=ENAMETOOLONG;break;}
            error=wb_share_path(child,depth+1,child_recycle);if(error)break;
        }
        closedir(dir);
    }
done:
    if(fd>=0)close(fd);
    if(seteuid(0)&&!error)error=errno;
    return error;
}
static int wb_profile_ready(void) {
    const char *paths[]={WB_DATA_ROOT,WB_DATA_ROOT "/WinBrew",WB_DATA_ROOT "/WinBrew/Users",WB_DATA_ROOT "/WinBrew/Users/Default"};
    struct stat st;
    for(unsigned i=0;i<4;i++) {
        if(lstat(paths[i],&st))return errno==ENOENT?ENOENT:errno;
        if(!S_ISDIR(st.st_mode)||st.st_uid!=0)return EPERM;
        if(i && st.st_gid!=0 && st.st_gid!=1 && st.st_gid!=WB_GROUP_ID)return EPERM;
        if(i && (st.st_mode&0022))return EPERM;
    }
    static const char marker[]="WINBREW_USER_PROFILE_V1\n" WB_DATA_ROOT "/WinBrew/Users/Default\n";
    int fd=open(WB_DATA_ROOT "/WinBrew/Users/Default/.winbrew-profile-v1",O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return errno;
    char bytes[sizeof marker];ssize_t n=read(fd,bytes,sizeof bytes);
    int valid=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==0&&st.st_nlink==1&&!(st.st_mode&0022)&&n==sizeof(marker)-1&&!memcmp(bytes,marker,sizeof(marker)-1);
    close(fd);return valid?0:EPERM;
}
static int wb_share_directory(const char *path,unsigned mode) {
    struct stat before,held,named;int fd=-1,error=0;
    if(lstat(path,&before)||!S_ISDIR(before.st_mode)||before.st_uid!=0||
       (before.st_gid!=0&&before.st_gid!=1&&before.st_gid!=WB_GROUP_ID))return EPERM;
    fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd<0)return errno;
    if(fstat(fd,&held)||held.st_dev!=before.st_dev||held.st_ino!=before.st_ino){error=ESTALE;goto done;}
    if((before.st_gid!=WB_GROUP_ID||(before.st_mode&07777)!=mode)&&(error=wb_remember(path,&before)))goto done;
    if(fchown(fd,(uid_t)-1,WB_GROUP_ID)||fchmod(fd,mode)){error=errno;goto done;}
    if(lstat(path,&named)||named.st_dev!=held.st_dev||named.st_ino!=held.st_ino){
        error=ESTALE;if(fchown(fd,(uid_t)-1,before.st_gid)||fchmod(fd,before.st_mode&07777))error=EIO;
    }
done:close(fd);return error;
}
static int wb_reconcile_shared(void){
    gid_t old[16],groups[3]={0,1,WB_GROUP_ID};uid_t uid=geteuid();
    int count=getgroups(16,old),error=0;
    if(count<0||seteuid(0))return EACCES;
    if(setgroups(3,groups)){error=errno;goto restore;}
    error=wb_profile_ready();
    /* WinBrew itself creates/migrates the verified profile on its first run. */
    if(error==ENOENT&&!strcmp(TARGET_TITLE,"PPSA77382")){error=0;goto restore_groups;}
    if(error)goto restore_groups;
    const char *roots[]={WB_DATA_ROOT "/WinBrew/Users/Default/Documents",WB_DATA_ROOT "/WinBrew/Users/Default/Desktop"};
    for(unsigned i=0;i<2;i++){
        struct stat st;
        if(lstat(roots[i],&st)||!S_ISDIR(st.st_mode)||st.st_uid!=0||
           (st.st_gid!=0&&st.st_gid!=1&&st.st_gid!=WB_GROUP_ID)){error=EPERM;goto restore_groups;}
    }
    const char *parents[]={WB_DATA_ROOT "/WinBrew",WB_DATA_ROOT "/WinBrew/Users",WB_DATA_ROOT "/WinBrew/Users/Default"};
    for(unsigned i=0;i<3;i++)if((error=wb_share_directory(parents[i],0710)))goto restore_groups;
    wb_entries=0;
    for(unsigned i=0;i<2;i++) {
        if((error=wb_share_directory(roots[i],0770)))break;
        if((error=wb_share_path(roots[i],0,0)))break;
    }
restore_groups:
    if(wb_finish(error!=0))error=EIO;
    if(seteuid(0)||setgroups(count,old))error=EIO;
restore:
    if(seteuid(uid))error=EIO;
    return error;
}

