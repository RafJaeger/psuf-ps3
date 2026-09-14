#ifndef FPSU_SCANNER_H
#define FPSU_SCANNER_H

#include "app.h"

typedef int (*fpsu_scan_progress_cb)(const char *path, int found, void *user);
typedef int (*fpsu_pattern_progress_cb)(const fpsu_game *game, unsigned int scanned, unsigned int limit, void *user);

int scanner_scan_all(fpsu_game *games, int max_games, fpsu_scan_progress_cb cb, void *user);
int scanner_scan_path(const char *path, fpsu_game *out);
int scanner_scan_iso_name_only(const char *path, fpsu_game *out);
int scanner_scan_mounted_iso_metadata(const char *mounted_path, fpsu_game *out);
int scanner_find_patterns(const fpsu_game *game);
int scanner_find_patterns_ex(const fpsu_game *game, fpsu_pattern_progress_cb cb, void *user);
int scanner_find_patterns_full_ex(const fpsu_game *game, fpsu_pattern_progress_cb cb, void *user);
int scanner_build_force_payload_ex(const fpsu_game *game, fpsu_patch_kind kind,
    fpsu_pattern_progress_cb cb, void *user, char *out, size_t out_size);

#endif
