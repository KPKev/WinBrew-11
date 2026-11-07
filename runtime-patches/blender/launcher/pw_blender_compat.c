/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "pw_blender_compat.h"
#include <string.h>
static unsigned lower(unsigned c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; }
int pw_blender_target(uint32_t app_id, const char *executable)
{
    if (app_id != 365670 || !executable) return 0;
    const char *base = executable;
    for (const char *p=executable; *p; p++) if (*p=='/' || *p=='\\') base=p+1;
    const char *name="blender.exe";
    while (*base && *name && lower((unsigned char)*base)==(unsigned char)*name) { base++; name++; }
    return !*base && !*name;
}
int pw_blender_default_backend(const char **argv, int argc, size_t capacity)
{
    if (!argv || argc < 2 || (size_t)argc >= capacity) return -1;
    int end=argc;
    for (int i=2;i<argc;i++) {
        if (!argv[i]) return -1;
        if (!strcmp(argv[i],"--")) { end=i; break; }
        if (!strcmp(argv[i],"--gpu-backend") || !strncmp(argv[i],"--gpu-backend=",14)) return argc;
    }
    if ((size_t)argc+3 > capacity) return -1;
    memmove(argv+end+2,argv+end,(size_t)(argc-end)*sizeof(*argv));
    argv[end]="--gpu-backend"; argv[end+1]="vulkan"; argv[argc+2]=NULL;
    return argc+2;
}
