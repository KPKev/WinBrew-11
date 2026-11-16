/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef PW_WINBREW_GROUP_H
#define PW_WINBREW_GROUP_H
/* Call only during launcher initialization, before Wine or application code.
 * Does not change filesystem permissions or the private Wine umask. */
int pw_winbrew_group_enter(void);
int pw_winbrew_prefix_connect(const char *prefix);
#endif
