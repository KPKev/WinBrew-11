/* SPDX-License-Identifier: GPL-3.0-or-later
 * Offline prototype. This does not reserve PS5 registration or authorize use.
 * Input bytes and directory layout must be verified against a complete manifest
 * by the caller before invoking these primitives. Never removes partial output. */
#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
typedef struct SkyrimPublishRoot { int parent_fd; int root_fd; dev_t dev; ino_t ino; uid_t uid; } SkyrimPublishRoot;
int skyrim_claim(int parent_fd, SkyrimPublishRoot *out);
int skyrim_root_current(const SkyrimPublishRoot *root);
int skyrim_make_dir(const SkyrimPublishRoot *root, const char *name, int *out_fd);
int skyrim_create_file(const SkyrimPublishRoot *root, const char *name, const void *data, size_t size, mode_t mode);
void skyrim_close(SkyrimPublishRoot *root);
/* Child descriptors remain valid only while all their ancestors remain open. */
typedef struct SkyrimPublishDir {
 const SkyrimPublishRoot *root;
 const struct SkyrimPublishDir *parent;
 int fd; dev_t dev; ino_t ino; uid_t uid; char name[256];
} SkyrimPublishDir;
int skyrim_child_dir(const SkyrimPublishRoot *root, const SkyrimPublishDir *parent, const char *name, SkyrimPublishDir *out);
int skyrim_child_current(const SkyrimPublishDir *dir);
int skyrim_child_file(const SkyrimPublishDir *dir, const char *name, const void *data, size_t size, mode_t mode);
void skyrim_child_close(SkyrimPublishDir *dir);
