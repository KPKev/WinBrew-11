#include "pw_ps5_fixed_hint.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 const uintptr_t lo=UINT64_C(0x800000000),hi=UINT64_C(0x1000000000);
 assert(pw_ps5_fixed_hint_allowed(lo-0x4000,0x4000));
 assert(!pw_ps5_fixed_hint_allowed(lo-0x4000,0x8000));
 assert(!pw_ps5_fixed_hint_allowed(lo,0x4000));
 assert(!pw_ps5_fixed_hint_allowed(UINT64_C(0xd0d840000),0x40000));
 assert(!pw_ps5_fixed_hint_allowed(UINT64_C(0xb7a3c0000),0x40000));
 assert(!pw_ps5_fixed_hint_allowed(hi-0x4000,0x4000));
 assert(!pw_ps5_fixed_hint_allowed(hi-0x4000,0x8000));
 assert(pw_ps5_fixed_hint_allowed(hi,0x40000));
 assert(pw_ps5_fixed_hint_allowed(UINT64_C(0x3000000000),0x40000));
 assert(!pw_ps5_fixed_hint_allowed(UINTPTR_MAX-0x3fff,0x4000));
 assert(pw_ps5_fixed_hint_allowed(UINTPTR_MAX-0x4000,0x4000));
 assert(!pw_ps5_fixed_hint_allowed(hi,0));
 puts("fixed-hint boundary, overlap, overflow and captured-failure cases passed");
 return 0;
}
