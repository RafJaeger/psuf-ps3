#ifndef FPSU_WEBMAN_CONTROL_H
#define FPSU_WEBMAN_CONTROL_H

#include "app.h"

#include <stddef.h>

/* main.c owns PT/ES/EN prose. Failures -4..-10 also return a symbolic diagnostic
   in message (e.g. webMAN [-8:READY_TIMEOUT]); invalid targets return -1. */
#define FPSU_WEBMAN_PLAY_XMB_UNAVAILABLE (-2)
#define FPSU_WEBMAN_PLAY_NET_INIT_FAILED (-4)
#define FPSU_WEBMAN_PLAY_SOCKET_FAILED (-5)
#define FPSU_WEBMAN_PLAY_CONNECT_FAILED (-6)
#define FPSU_WEBMAN_PLAY_SEND_FAILED (-7)
#define FPSU_WEBMAN_PLAY_READY_TIMEOUT (-8)
#define FPSU_WEBMAN_PLAY_ARMED_TIMEOUT (-9)
#define FPSU_WEBMAN_PLAY_IO_FAILED (-10)

int webman_get_gpu_clock(int *gpu_mhz, int *vram_mhz);
int webman_set_gpu_clock(int gpu_mhz, int vram_mhz, char *message, size_t message_size);
int webman_check_available(char *message, size_t message_size);
int webman_start_ps3mapi(char *message, size_t message_size);
int webman_play_game(const fpsu_game *game, char *message, size_t message_size);
int webman_check_artemis(char *message, size_t message_size);
int webman_prepare_ntfs(int clear_cache);
int webman_get_ntfs_index(char *response, size_t response_size);
int webman_toggle_fps_counter(char *message, size_t message_size);
int webman_set_full_video_range(char *message, size_t message_size);

#endif
