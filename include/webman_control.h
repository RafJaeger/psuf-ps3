#ifndef FPSU_WEBMAN_CONTROL_H
#define FPSU_WEBMAN_CONTROL_H

#include <stddef.h>

int webman_get_gpu_clock(int *gpu_mhz, int *vram_mhz);
int webman_set_gpu_clock(int gpu_mhz, int vram_mhz, char *message, size_t message_size);
int webman_check_available(char *message, size_t message_size);
int webman_start_ps3mapi(char *message, size_t message_size);
int webman_check_artemis(char *message, size_t message_size);

#endif
