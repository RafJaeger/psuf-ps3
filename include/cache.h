#ifndef FPSU_CACHE_H
#define FPSU_CACHE_H

#include "app.h"

int cache_write_results(const fpsu_game_result *results, int count);
int cache_read_results(fpsu_game_result *results, int max_results);
int cache_write_progress(const char *phase, int index, int count, const fpsu_game *game,
    unsigned int scanned, unsigned int limit, const fpsu_game_result *results, int result_count);

#endif
