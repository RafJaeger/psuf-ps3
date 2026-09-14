#include "webman_control.h"

#include <arpa/inet.h>
#include <net/net.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int webman_http_get(const char *path, char *response, size_t response_size)
{
    int sock;
    int ret;
    size_t used = 0;
    struct sockaddr_in server;
    struct timeval timeout;
    char request[768];

    if (!path || path[0] != '/' || strlen(path) > 512) {
        return -1;
    }
    if (response && response_size > 0) {
        response[0] = '\0';
    }

    if (netInitialize() != 0) {
        return -1;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        netDeinitialize();
        return -1;
    }
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
#ifdef SO_RCVTIMEO
    (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
#ifdef SO_SNDTIMEO
    (void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

    memset(&server, 0, sizeof(server));
#ifdef __PSL1GHT__
    server.sin_len = sizeof(server);
#endif
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        close(sock);
        netDeinitialize();
        return -1;
    }

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", path);
    ret = (int)write(sock, request, strlen(request));
    if (ret <= 0) {
        close(sock);
        netDeinitialize();
        return -1;
    }

    if (response && response_size > 1) {
        while (used + 1 < response_size) {
            ret = (int)read(sock, response + used, response_size - used - 1);
            if (ret <= 0) {
                break;
            }
            used += (size_t)ret;
        }
        response[used] = '\0';
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
    netDeinitialize();
    return 0;
}

static int parse_mhz_value(const char *response, const char *label, int *out)
{
    const char *p;

    if (!response || !label || !out) {
        return -1;
    }
    p = strstr(response, label);
    if (!p) {
        return -1;
    }
    p += strlen(label);
    while (*p == ' ' || *p == ':') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return -1;
    }
    *out = atoi(p);
    return *out > 0 ? 0 : -1;
}

static int response_looks_missing(const char *response)
{
    return response &&
        (strstr(response, "404") != NULL ||
            strstr(response, "Not Found") != NULL ||
            strstr(response, "not found") != NULL);
}

int webman_check_available(char *message, size_t message_size)
{
    char response[2048];

    if (message && message_size > 0) {
        message[0] = '\0';
    }
    if (webman_http_get("/cpursx.ps3", response, sizeof(response)) != 0 ||
        response_looks_missing(response)) {
        if (message && message_size > 0) {
            snprintf(message, message_size,
                "seu PS3 nao tem webMAN disponivel no sistema. instale para verificar novamente");
        }
        return -1;
    }
    if (message && message_size > 0) {
        snprintf(message, message_size, "webMAN OK");
    }
    return 0;
}

int webman_start_ps3mapi(char *message, size_t message_size)
{
    char response[2048];

    if (message && message_size > 0) {
        message[0] = '\0';
    }
    (void)webman_http_get("/netstatus.ps3?start-ps3mapi", NULL, 0);
    sleep(2);
    if (webman_http_get("/ps3mapi.ps3?PROCESS%20GETCURRENTPID", response, sizeof(response)) != 0 ||
        response_looks_missing(response)) {
        if (message && message_size > 0) {
            snprintf(message, message_size,
                "seu PS3 nao tem PS3MAPI disponivel no sistema. instale webMAN MOD com PS3MAPI e verifique novamente");
        }
        return -1;
    }
    if (message && message_size > 0) {
        snprintf(message, message_size, "PS3MAPI OK");
    }
    return 0;
}

int webman_check_artemis(char *message, size_t message_size)
{
    char response[2048];

    if (message && message_size > 0) {
        message[0] = '\0';
    }
    (void)webman_http_get("/netstatus.ps3?start-artemis", NULL, 0);
    sleep(2);
    if (webman_http_get("/artemis.ps3", response, sizeof(response)) != 0 ||
        response_looks_missing(response)) {
        if (message && message_size > 0) {
            snprintf(message, message_size,
                "seu PS3 nao tem Artemis disponivel no sistema. instale para verificar novamente");
        }
        return -1;
    }
    if (message && message_size > 0) {
        snprintf(message, message_size, "Artemis OK");
    }
    return 0;
}

int webman_get_gpu_clock(int *gpu_mhz, int *vram_mhz)
{
    char response[2048];
    int gpu;
    int vram;

    if (webman_http_get("/gpuclock.ps3", response, sizeof(response)) != 0) {
        return -1;
    }
    if (parse_mhz_value(response, "GPU", &gpu) != 0 ||
        parse_mhz_value(response, "VRAM", &vram) != 0) {
        return -1;
    }
    if (gpu_mhz) {
        *gpu_mhz = gpu;
    }
    if (vram_mhz) {
        *vram_mhz = vram;
    }
    return 0;
}

int webman_set_gpu_clock(int gpu_mhz, int vram_mhz, char *message, size_t message_size)
{
    char path[96];
    char response[2048];
    int reported_gpu = 0;
    int reported_vram = 0;

    if (message && message_size > 0) {
        message[0] = '\0';
    }
    if (gpu_mhz < 300 || gpu_mhz > 1200 || vram_mhz < 300 || vram_mhz > 1200) {
        if (message && message_size > 0) {
            snprintf(message, message_size, "Clock fora da faixa do webMAN");
        }
        return -1;
    }

    snprintf(path, sizeof(path), "/gpuclock.ps3?%d|%d", gpu_mhz, vram_mhz);
    if (webman_http_get(path, response, sizeof(response)) != 0) {
        if (message && message_size > 0) {
            snprintf(message, message_size, "webMAN nao respondeu");
        }
        return -1;
    }

    if (parse_mhz_value(response, "GPU", &reported_gpu) == 0 &&
        parse_mhz_value(response, "VRAM", &reported_vram) == 0 &&
        (reported_gpu != gpu_mhz || reported_vram != vram_mhz)) {
        if (message && message_size > 0) {
            snprintf(message, message_size, "webMAN respondeu %d/%d MHz", reported_gpu, reported_vram);
        }
        return -1;
    }

    if (message && message_size > 0) {
        snprintf(message, message_size, "GPU %d MHz | VRAM %d MHz", gpu_mhz, vram_mhz);
    }
    return 0;
}
