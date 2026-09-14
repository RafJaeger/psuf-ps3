#ifndef FPSU_PATCH_DB_H
#define FPSU_PATCH_DB_H

#include "app.h"

void patch_db_default_options(fpsu_game_result *result);
int patch_db_match_game(const fpsu_game *game, fpsu_game_result *result);
int patch_db_collect_game_options(const fpsu_game *game, fpsu_patch_option *options, int max_options);
int patch_db_collect_graphics_options(const fpsu_game *game, fpsu_patch_option *options, int max_options);
int patch_db_write_ncl(const fpsu_game *game, const fpsu_patch_option *option, char *out_path, size_t out_path_size);
int patch_db_remove_generated(const fpsu_game *game);
int patch_db_cleanup_runtime_artifacts(void);
int patch_db_validate_file(const char *path);
const char *patch_db_active_path(void);
int patch_db_is_native60(const fpsu_game *game);

#endif
