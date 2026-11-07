/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "exclusive_publish.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#define TITLE "PPSA77383"
static int leaf(const char *name) {
 if (!name || !*name || !strcmp(name,".") || !strcmp(name,"..") || strchr(name,'/')) { errno=EINVAL; return 0; }
 return 1;
}
int skyrim_root_current(const SkyrimPublishRoot *r) {
 struct stat held, named, parent;
 if (!r || r->root_fd<0 || r->parent_fd<0 || fstat(r->parent_fd,&parent) || !S_ISDIR(parent.st_mode) ||
     fstat(r->root_fd,&held) || fstatat(r->parent_fd,TITLE,&named,AT_SYMLINK_NOFOLLOW)) return -1;
 if (!S_ISDIR(held.st_mode) || !S_ISDIR(named.st_mode) || held.st_dev!=r->dev || held.st_ino!=r->ino ||
     named.st_dev!=r->dev || named.st_ino!=r->ino || held.st_uid!=r->uid || named.st_uid!=r->uid) { errno=ESTALE; return -1; }
 return 0;
}
int skyrim_claim(int parent_fd, SkyrimPublishRoot *out) {
 struct stat p, a, b; int fd=-1, saved;
 if (!out) { errno=EINVAL; return -1; }
 *out=(SkyrimPublishRoot){-1,-1,0,0,0};
 if (fstat(parent_fd,&p) || !S_ISDIR(p.st_mode)) { errno=ENOTDIR; return -1; }
 if (mkdirat(parent_fd,TITLE,0700)) return -1;
 fd=openat(parent_fd,TITLE,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if (fd<0 || fstat(fd,&a) || fstatat(parent_fd,TITLE,&b,AT_SYMLINK_NOFOLLOW) || !S_ISDIR(a.st_mode) ||
     !S_ISDIR(b.st_mode) || a.st_dev!=b.st_dev || a.st_ino!=b.st_ino || a.st_uid!=geteuid()) {
  saved=errno?errno:ESTALE; if(fd>=0)close(fd);errno=saved;return -1;
 }
 out->parent_fd=dup(parent_fd); if(out->parent_fd<0) {saved=errno;close(fd);errno=saved;return -1;}
 out->root_fd=fd;out->dev=a.st_dev;out->ino=a.st_ino;out->uid=a.st_uid;
 if (fchmod(fd,0755) || fsync(fd) || fsync(parent_fd) || skyrim_root_current(out)) {saved=errno;skyrim_close(out);errno=saved;return -1;}
 return 0;
}
int skyrim_make_dir(const SkyrimPublishRoot *r, const char *name, int *out_fd) {
 struct stat a,b;int fd=-1,saved;
 if (!out_fd || !leaf(name) || skyrim_root_current(r)) return -1;
 *out_fd=-1;
 if (mkdirat(r->root_fd,name,0700)) return -1;
 fd=openat(r->root_fd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0 || fstat(fd,&a) || fstatat(r->root_fd,name,&b,AT_SYMLINK_NOFOLLOW) || !S_ISDIR(a.st_mode) ||
    !S_ISDIR(b.st_mode) || a.st_dev!=b.st_dev || a.st_ino!=b.st_ino || a.st_uid!=r->uid || skyrim_root_current(r)) {
  saved=errno?errno:ESTALE;if(fd>=0)close(fd);errno=saved;return -1;
 }
 if(fchmod(fd,0755) || fsync(fd) || fsync(r->root_fd)) {saved=errno;close(fd);errno=saved;return -1;}
 *out_fd=fd;return 0;
}
int skyrim_create_file(const SkyrimPublishRoot *r,const char *name,const void *data,size_t size,mode_t mode) {
 struct stat a,b;int fd,saved;size_t at=0;
 if (!leaf(name) || (!data && size) || (mode!=0644 && mode!=0755) || skyrim_root_current(r)) {errno=EINVAL;return -1;}
 fd=openat(r->root_fd,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
 while(at<size) {ssize_t n=write(fd,(const char *)data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)goto fail;at+=(size_t)n;}
 if(fstat(fd,&a) || fstatat(r->root_fd,name,&b,AT_SYMLINK_NOFOLLOW) || !S_ISREG(a.st_mode) || !S_ISREG(b.st_mode) ||
    a.st_dev!=b.st_dev || a.st_ino!=b.st_ino || a.st_uid!=r->uid || a.st_nlink!=1 || skyrim_root_current(r) ||
    fchmod(fd,mode) || fsync(fd) || fsync(r->root_fd)) goto fail;
 if (close(fd)) return -1;
 return 0;
fail:saved=errno?errno:EIO;close(fd);errno=saved;return -1;
}
void skyrim_close(SkyrimPublishRoot *r) {if(!r)return;if(r->root_fd>=0)close(r->root_fd);if(r->parent_fd>=0)close(r->parent_fd);r->root_fd=r->parent_fd=-1;}
int skyrim_child_current(const SkyrimPublishDir *d) {
 struct stat a,b; int pfd;
 if (!d || d->fd<0 || !d->root || skyrim_root_current(d->root)) return -1;
 if (d->parent && skyrim_child_current(d->parent)) return -1;
 pfd=d->parent?d->parent->fd:d->root->root_fd;
 if(fstat(d->fd,&a)||fstatat(pfd,d->name,&b,AT_SYMLINK_NOFOLLOW))return -1;
 if(!S_ISDIR(a.st_mode)||!S_ISDIR(b.st_mode)||a.st_dev!=d->dev||b.st_dev!=d->dev||a.st_ino!=d->ino||b.st_ino!=d->ino||a.st_uid!=d->uid||b.st_uid!=d->uid){errno=ESTALE;return -1;}
 return 0;
}
int skyrim_child_dir(const SkyrimPublishRoot *root,const SkyrimPublishDir *parent,const char *name,SkyrimPublishDir *out) {
 struct stat a,b;int pfd,fd,saved;
 if(!root||!out||!leaf(name)||strlen(name)>=sizeof(out->name)||skyrim_root_current(root)||(parent&&(parent->root!=root||skyrim_child_current(parent))))return -1;
 pfd=parent?parent->fd:root->root_fd;out->fd=-1;
 if(mkdirat(pfd,name,0700))return -1;
 fd=openat(pfd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0||fstat(fd,&a)||fstatat(pfd,name,&b,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(a.st_mode)||!S_ISDIR(b.st_mode)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||a.st_uid!=root->uid){saved=errno?errno:ESTALE;if(fd>=0)close(fd);errno=saved;return -1;}
 out->root=root;out->parent=parent;out->fd=fd;out->dev=a.st_dev;out->ino=a.st_ino;out->uid=a.st_uid;strcpy(out->name,name);
 if(skyrim_child_current(out)||fchmod(fd,0755)||fsync(fd)||fsync(pfd)){saved=errno;skyrim_child_close(out);errno=saved;return -1;}
 return 0;
}
int skyrim_child_file(const SkyrimPublishDir *d,const char *name,const void *data,size_t size,mode_t mode) {
 struct stat a,b;int fd,saved;size_t at=0;
 if(!leaf(name)||(!data&&size)||(mode!=0644&&mode!=0755)||skyrim_child_current(d)){errno=EINVAL;return -1;}
 fd=openat(d->fd,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
 while(at<size){ssize_t n=write(fd,(const char*)data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)goto fail;at+=(size_t)n;}
 if(fstat(fd,&a)||fstatat(d->fd,name,&b,AT_SYMLINK_NOFOLLOW)||!S_ISREG(a.st_mode)||!S_ISREG(b.st_mode)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||a.st_uid!=d->uid||a.st_nlink!=1||skyrim_child_current(d)||fchmod(fd,mode)||fsync(fd)||fsync(d->fd))goto fail;
 if(close(fd))return -1;
 return 0;
fail:saved=errno?errno:EIO;close(fd);errno=saved;return -1;
}
void skyrim_child_close(SkyrimPublishDir *d){if(d&&d->fd>=0){close(d->fd);d->fd=-1;}}
