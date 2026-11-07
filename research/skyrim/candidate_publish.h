#pragma once
/* Writes only a newly claimed PPSA77383 directory. Not an app registration
 * reservation. Failure leaves partial output and is never retried automatically. */
int skyrim_publish_candidate(int parent_fd, int staging_fd);
