/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef WINE11_RETURN_NATIVE_H
#define WINE11_RETURN_NATIVE_H
#include <stdint.h>
/* Preload while the native title is starting, before filesystem elevation.
 * Failure disables return only; it never prevents the game from starting. */
uint32_t wine11_return_native_preload(void);
/* Existing return-intent launch callback signature; fixed target only. */
uint32_t wine11_return_native_launch(void *unused, const char *title);
#endif
