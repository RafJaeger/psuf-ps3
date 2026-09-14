#ifndef FPSU_SFO_H
#define FPSU_SFO_H

#include "app.h"

int sfo_read_game(const char *path, fpsu_game *game);
int sfo_parse_game_buffer(const unsigned char *buf, size_t size, fpsu_game *game);

#endif
