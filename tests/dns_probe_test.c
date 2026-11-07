/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../tools/diagnostics/dns_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int init_error, pool_error, create_error, lookup_error, destroy_error, pool_destroy_error;
static int pools, creates, lookups, errors, destroys, pool_destroys, results;
static uint64_t stamp;
static const char *last_phase;
static struct wine11_dns_result observed[12];
static int init(void) { assert(!strcmp(last_phase,"net_init_begin")); return init_error; }
static int pool_create(const char *name, int size, int flags) {
  assert(!strcmp(last_phase,"pool_create_begin"));
  assert(!strcmp(name,"wine11_dns_probe") && size==16384 && flags==0); ++pools;
  return pool_error ? pool_error : 42;
}
static int pool_destroy(int id) { assert(!strcmp(last_phase,"pool_destroy_begin")); assert(id==42); ++pool_destroys; return pool_destroy_error; }
static int create(const char *name,int pool,int flags) {
  assert(!strcmp(last_phase,"resolver_create_begin"));
  assert(!strcmp(name,"wine11_dns_probe") && pool==42 && flags==0); ++creates;
  return create_error && creates==1 ? create_error : 100+creates;
}
static int destroy(int id) { assert(!strcmp(last_phase,"resolver_destroy_begin")); assert(id>=101 && id<=104); ++destroys; return destroy_error; }
static int lookup(int id,const char *host,void *address,int timeout,int retries,int flags) {
  assert(!strcmp(last_phase,"lookup_begin"));
  assert(id>=101 && id<=104);
  assert(!strcmp(host,"accounts.google.com") || !strcmp(host,"example.com"));
  assert(timeout==1000000 && retries==0 && flags==0); ++lookups;
  memset(address,0,16);
  return lookup_error && lookups==1 ? lookup_error : 0;
}
static int error(int id,int *value) { assert(!strcmp(last_phase,"resolver_error_begin")); assert(id>=101 && id<=104); ++errors; *value=77; return 0; }
static const char *format(int family,const void *address,char *out,unsigned size) {
  assert((family==2 || family==28) && address && size>=16); strcpy(out,family==2?"192.0.2.1":"2001:db8::1"); return out;
}
static uint64_t now(void) { stamp+=1000; return stamp; }
static void sink(const struct wine11_dns_result *result) { assert(results<12); observed[results++]=*result; }
static void progress(const char *stage,const char *host,int family) {
 assert(stage); last_phase=stage;
 if(host) assert((!strcmp(host,"accounts.google.com") || !strcmp(host,"example.com")) && (family==2 || family==28));
 else assert(family==0);
}
static const struct wine11_dns_ops ops={init,pool_create,pool_destroy,create,destroy,lookup,lookup,error,format,now,progress};
static void reset(void) {
 init_error=pool_error=create_error=lookup_error=destroy_error=pool_destroy_error=0;
 pools=creates=lookups=errors=destroys=pool_destroys=results=0;stamp=0;last_phase="";memset(observed,0,sizeof(observed));
}
int main(void) {
 reset();wine11_dns_run(&ops,sink);
 assert(pools==1 && creates==4 && lookups==4 && destroys==4 && pool_destroys==1 && results==4);
 for(int i=0;i<4;++i) assert(!strcmp(observed[i].stage,"lookup") && !observed[i].code && observed[i].elapsed_us==1000 && observed[i].address[0]);
 reset();init_error=-17;wine11_dns_run(&ops,sink);
 assert(!pools && !creates && results==1 && !strcmp(observed[0].stage,"net_init") && observed[0].code==-17);
 reset();pool_error=-18;wine11_dns_run(&ops,sink);
 assert(pools==1 && !creates && !pool_destroys && results==1 && !strcmp(observed[0].stage,"pool_create"));
 reset();create_error=-19;wine11_dns_run(&ops,sink);
 assert(creates==4 && lookups==3 && destroys==3 && pool_destroys==1 && results==4 && !strcmp(observed[0].stage,"resolver_create"));
 reset();lookup_error=-20;wine11_dns_run(&ops,sink);
 assert(lookups==4 && errors==1 && destroys==4 && pool_destroys==1 && results==4 && observed[0].code==-20 && observed[0].resolver_error==77);
 reset();destroy_error=-21;wine11_dns_run(&ops,sink);
 assert(creates==1 && destroys==1 && pool_destroys==1 && results==2 && !strcmp(observed[1].stage,"resolver_destroy"));
 reset();pool_destroy_error=-22;wine11_dns_run(&ops,sink);
 assert(destroys==4 && pool_destroys==1 && results==5 && !strcmp(observed[4].stage,"pool_destroy"));
 puts("PASS: fixed-host DNS timeout/retry bounds, exact resource lifecycle and error reporting");
}
