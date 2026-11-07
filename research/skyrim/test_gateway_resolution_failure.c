/* Host-only failure-path model; no PS5 syscall is attempted. */
#include "pw_title_publication_ps5_lock.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static int lookups;
int sceKernelDlsym(int module,const char *name,void **out) {
  assert(module == 0x2001 && !strcmp(name,"getpid"));
  ++lookups; *out = 0; return -1;
}
int main(void) {
  errno=0; assert(pw_title_ps5_umask(077)==-1 && errno==ENOSYS);
  errno=0; assert(pw_title_publication_ps5_lock(NULL, 1)==-1 && errno==ENOSYS);
  assert(lookups==2); puts("PASS: unresolved getpid prevents umask and lock without a syscall");
}
