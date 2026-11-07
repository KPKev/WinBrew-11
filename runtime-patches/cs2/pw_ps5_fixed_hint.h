/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef PW_PS5_FIXED_HINT_H
#define PW_PS5_FIXED_HINT_H
#include <stdint.h>
#include <stddef.h>
/* Ordinary guest direct-memory mappings cannot back the PS5 system band.
 * ReserveVirtualRange accepts it, but MapDirectMemory returns EINVAL (FW13.60).
 * Reject a fixed hint before claiming it, allowing callers to retry anywhere.
 * Host PRX mappings do not go through this Wine guest allocation path. */
static inline int pw_ps5_fixed_hint_allowed(uintptr_t base, size_t size)
{
    const uintptr_t system_begin = UINT64_C(0x800000000);
    const uintptr_t user_begin = UINT64_C(0x1000000000);
    if (!size || size > UINTPTR_MAX - base) return 0;
    return base >= user_begin || base + size <= system_begin;
}
#endif
