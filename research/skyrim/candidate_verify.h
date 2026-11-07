#pragma once
/* Verifies exact pinned file bytes under an already-open staging directory.
 * Does not bind later reads; publisher must independently hash exact bytes. */
int skyrim_verify_candidate(int root_fd);

int skyrim_candidate_open(int root_fd, const char *path);
