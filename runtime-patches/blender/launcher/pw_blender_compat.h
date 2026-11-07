/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef PW_BLENDER_COMPAT_H
#define PW_BLENDER_COMPAT_H
#include <stddef.h>
#include <stdint.h>
/* Steam identity plus executable basename; independent of drive and title ID. */
int pw_blender_target(uint32_t app_id, const char *executable);
/* Append the Vulkan default before script arguments. Explicit backend wins.
 * argc excludes the trailing NULL; capacity includes it. -1 leaves argv intact. */
int pw_blender_default_backend(const char **argv, int argc, size_t capacity);
#endif
