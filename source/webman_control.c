#include "webman_control.h"

#include <arpa/inet.h>
#include <net/net.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define FPSU_LAUNCH_DIR "/dev_hdd0/tmp/wm_combo"
#define FPSU_LAUNCH_LOG FPSU_CACHE_ROOT "/launch.log"
#define WEBMAN_REQUEST_FILE "/dev_hdd0/tmp/wm_request"
#define FPSU_LAUNCH_READY_POLLS 160
#define FPSU_LAUNCH_RETRY_POLLS 40

static int launch_diagnostics;

static void launch_log_stage(const char *stage, int result, int detail)
{
    FILE *log;
    if (!launch_diagnostics) return;
    log = fopen(FPSU_LAUNCH_LOG, "ab");
    if (!log) return;
    fprintf(log, "PSUF_HOST %s result=%d detail=%d\n", stage, result, detail);
    fclose(log);
}

static int webman_connect_loopback(unsigned short port)
{
    int sock;
    int ret;
    struct sockaddr_in server;
    struct timeval timeout;
    ret = netInitialize();
    launch_log_stage("NET_INIT", ret, errno);
    if (ret != 0) {
        return FPSU_WEBMAN_PLAY_NET_INIT_FAILED;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        launch_log_stage("SOCKET_FAILED", sock, errno);
        return FPSU_WEBMAN_PLAY_SOCKET_FAILED;
    }
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
#ifdef SO_RCVTIMEO
    ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    launch_log_stage("RCVTIMEO", ret, errno);
#endif
#ifdef SO_SNDTIMEO
    ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    launch_log_stage("SNDTIMEO", ret, errno);
#endif

    memset(&server, 0, sizeof(server));
#ifdef __PSL1GHT__
    server.sin_len = sizeof(server);
#endif
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        launch_log_stage("CONNECT_FAILED", -1, errno);
        close(sock);
        return FPSU_WEBMAN_PLAY_CONNECT_FAILED;
    }

    return sock;
}

static int webman_http_send(const char *path)
{
    int sock, ret;
    char request[768];

    if (!path || path[0] != '/' || strlen(path) > 512) return -1;
    sock = webman_connect_loopback(80);
    if (sock < 0) return sock;
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", path);
    {
        size_t sent = 0, len = strlen(request);
        while (sent < len) {
            ret = (int)write(sock, request + sent, len - sent);
            if (ret <= 0) {
                launch_log_stage("SEND_FAILED", ret, errno);
                close(sock);
                return FPSU_WEBMAN_PLAY_SEND_FAILED;
            }
            sent += (size_t)ret;
        }
    }
    launch_log_stage("HTTP_SENT", 0, (int)strlen(request));
    return sock;
}

static int webman_http_get(const char *path, char *response, size_t response_size)
{
    int sock;
    int ret;
    size_t used = 0;

    if (response && response_size > 0) {
        response[0] = '\0';
    }
    sock = webman_http_send(path);
    if (sock < 0) return sock;
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
    /* libnet initialization is process-wide, without reference counting.
       cover.c keeps it initialized for its DNS worker and future downloads. */
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

static int ps3mapi_http_core_ready(const char *response)
{
    const char *body, *value;
    char *end;
    unsigned long version;
    int status, quoted;

    if (sscanf(response, "HTTP/%*s %d", &status) != 1 || status != 200) return 0;
    body = strstr(response, "\r\n\r\n");
    if (!body) return 0;
    body += 4;
    while (isspace((unsigned char)*body)) ++body;
    if (*body++ != '{') return 0;
    while (isspace((unsigned char)*body)) ++body;
    if (strncmp(body, "\"response\"", 10) != 0) return 0;
    value = body + 10;
    while (isspace((unsigned char)*value)) ++value;
    if (*value++ != ':') return 0;
    while (isspace((unsigned char)*value)) ++value;
    quoted = *value == '"';
    if (quoted) ++value;
    if (!isdigit((unsigned char)*value)) return 0;
    version = strtoul(value, &end, 0);
    if (version == 0 || version > 0xffff || (quoted && *end++ != '"')) return 0;
    while (isspace((unsigned char)*end)) ++end;
    return *end == '}';
}

static int ps3mapi_read_line(int sock, char *line, size_t size)
{
    size_t i;
    for (i = 0; i + 1 < size; ++i) {
        if (read(sock, line + i, 1) != 1) return -1;
        if (line[i] == '\n') {
            line[i + 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static int ps3mapi_tcp_core_ready(void)
{
    static const char command[] = "CORE GETVERSION\r\n";
    char line[160], extra;
    unsigned int version;
    int ready;
    int sock = webman_connect_loopback(7887);
    if (sock < 0) return 0;
    ready = ps3mapi_read_line(sock, line, sizeof(line)) == 0 &&
        strncmp(line, "220 ", 4) == 0 &&
        ps3mapi_read_line(sock, line, sizeof(line)) == 0 &&
        strncmp(line, "230 ", 4) == 0 &&
        write(sock, command, sizeof(command) - 1) == (int)sizeof(command) - 1 &&
        ps3mapi_read_line(sock, line, sizeof(line)) == 0 &&
        strncmp(line, "200 ", 4) == 0 &&
        sscanf(line + 4, "%u %c", &version, &extra) == 1 &&
        version > 0 && version <= 0xffff;
    close(sock);
    return ready;
}

static int append_url_path_encoded(char *out, size_t out_size, size_t *used, const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *p;

    if (!out || out_size == 0 || !used || !text) {
        return -1;
    }
    for (p = (const unsigned char *)text; *p; ++p) {
        unsigned char c = *p;
        int safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/';
        size_t needed = safe ? 1u : 3u;
        if (*used + needed + 1 >= out_size) {
            return -1;
        }
        if (safe) {
            out[(*used)++] = (char)c;
        } else {
            out[(*used)++] = '%';
            out[(*used)++] = hex[(c >> 4) & 0x0f];
            out[(*used)++] = hex[c & 0x0f];
        }
    }
    out[*used] = '\0';
    return 0;
}

static int launch_script_confirmed(const char *expected)
{
    FILE *log = fopen(FPSU_LAUNCH_LOG, "rb");
    char marker[128];
    int confirmed = 0;
    if (!log) return 0;
    while (fgets(marker, sizeof(marker), log)) {
        marker[strcspn(marker, "\r\n")] = '\0';
        if (strcmp(marker, expected) == 0) { confirmed = 1; break; }
    }
    fclose(log);
    return confirmed;
}

static int launch_has_request_queue(void)
{
    char *response = malloc(32768);
    char *option, *end, *checked;
    int enabled = 0;
    if (!response) return 0;
    if (webman_http_get("/setup.ps3", response, 32768) == 0) {
        option = strstr(response, "name=\"lx\"");
        if (option) {
            end = strchr(option, '>');
            checked = strstr(option, "checked");
            enabled = end && checked && checked < end &&
                strstr(end + 1, "Poll wm_request") != NULL;
        }
    }
    free(response);
    return enabled;
}

static int launch_publish_request(const char *stage_path, const char *request)
{
    FILE *file = fopen(stage_path, "wb");
    int written, result;
    if (!file) return -1;
    written = fputs(request, file);
    result = fclose(file);
    if (written != EOF && result == 0) {
        /* Link the complete request without replacing another client's job. */
        result = sysLv2FsLink(stage_path, WEBMAN_REQUEST_FILE);
    } else {
        result = -1;
    }
    remove(stage_path);
    return result;
}

int webman_play_game(const fpsu_game *game, char *message, size_t message_size)
{
    static const char installed_root[] = "/dev_hdd0/game/";
    char command[1301] = "";
    char focus[513] = "";
    char target[FPSU_MAX_PATH + sizeof("/PS3_GAME/USRDIR/EBOOT.BIN")];
    char go_path[128] = "";
    char script_path[128], request_path[144], stage_path[128];
    char ready[80], armed[80];
    const char *play = "/play.ps3?col=game&seg=seg_device";
    const char *reset = "/mount.ps3/unmount";
    const char *verify = "/dev_bdvd/PS3_GAME/USRDIR/EBOOT.BIN";
    const char *base;
    size_t used;
    struct stat st;
    struct timeval now;
    FILE *file;
    int attempt, written, use_queue;
    int trigger_socket = -1;
    int script_created = 0;
    int result = -1;

    launch_diagnostics = 0;
    if (message && message_size) message[0] = '\0';
    if (!game || !memchr(game->base_path, '\0', sizeof(game->base_path)) ||
        game->base_path[0] != '/' || strpbrk(game->base_path, "\r\n;")) goto failed;
    base = game->base_path;
    if (strncmp(base, installed_root, sizeof(installed_root) - 1) == 0) {
        const char *folder = base + sizeof(installed_root) - 1;
        char query[256];
        if (strlen(folder) != 9 || strspn(folder, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 9 ||
            stat(base, &st) != 0 || !S_ISDIR(st.st_mode)) goto failed;
        snprintf(target, sizeof(target), "%s%.9s/USRDIR/EBOOT.BIN", installed_root, folder);
        if (stat(target, &st) != 0 || !S_ISREG(st.st_mode)) goto failed;
        verify = target;
        /* Resolve the installed XMB entry by its directory name. Do not reject
           the console based on a private category_game.xml layout: HEN and
           webMAN versions can expose installed packages under different XMB
           categories (including HG). The filename is the stable identifier. */
        snprintf(query, sizeof(query),
            "focus_segment_hit_first gameDir2 xcb://localhost/query?table=MMS_MEDIA_TYPE_HDD"
            "&cond=Ae+Game:Game.broken%%200+Ae+Game:Common.fileName%%20%.9s", folder);
        strcpy(focus, "/xmb.ps3$");
        used = strlen(focus);
        if (append_url_path_encoded(focus, sizeof(focus), &used, query) != 0) goto failed;
        play = "/xmb.ps3$exec_app";
    } else if (strcmp(base, "/dev_bdvd") != 0) {
        if (strncmp(base, "/dev_hdd0/tmp/", 14) == 0 ||
            (stat(base, &st) != 0 && strncmp(base, "/dev_ntfs", 9) != 0 &&
                strncmp(base, "/net", 4) != 0)) goto failed;
        if (!game->is_iso) {
            int length = snprintf(target, sizeof(target), "%s/PS3_GAME/USRDIR/EBOOT.BIN", base);
            if (length < 0 || (size_t)length >= sizeof(target) ||
                (stat(target, &st) != 0 && strncmp(base, "/dev_ntfs", 9) != 0 &&
                    strncmp(base, "/net", 4) != 0)) goto failed;
        }
        strcpy(command, "/mount.ps3");
    } else if (stat(verify, &st) != 0 || !S_ISREG(st.st_mode)) {
        goto failed;
    }
    used = strlen(command);
    if (used && append_url_path_encoded(command, sizeof(command), &used, base) != 0) goto failed;

    if (gettimeofday(&now, NULL) != 0) goto failed;
    snprintf(go_path, sizeof(go_path), FPSU_CACHE_ROOT "/launch-%lx-%lx.go",
        (unsigned long)now.tv_sec, (unsigned long)now.tv_usec);
    snprintf(script_path, sizeof(script_path), FPSU_LAUNCH_DIR "/wm_custom_psuf-launch-%lx-%lx.bat",
        (unsigned long)now.tv_sec, (unsigned long)now.tv_usec);
    snprintf(request_path, sizeof(request_path), "/play.ps3%s", script_path);
    snprintf(stage_path, sizeof(stage_path), FPSU_CACHE_ROOT "/launch-%lx-%lx.req",
        (unsigned long)now.tv_sec, (unsigned long)now.tv_usec);
    snprintf(ready, sizeof(ready), "PSUF_READY %lx-%lx",
        (unsigned long)now.tv_sec, (unsigned long)now.tv_usec);
    snprintf(armed, sizeof(armed), "PSUF_ARMED %lx-%lx",
        (unsigned long)now.tv_sec, (unsigned long)now.tv_usec);

    /* The combo is acknowledged while PSUF is open. Only mount and select
       the explicit XMB segment after it exits. Do not use /play.ps3<path> or
       bare /play.ps3: pad_combo_play.h passes empty col/seg to exec_xmb_item().
       Official source: aldostools/webMAN-MOD, commit 48dfd3f14614a4baaa1dfccf1ba99a168a74b192,
       include/cmd/pad_combo_play.h and include/init/vsh.h. */
    result = FPSU_WEBMAN_PLAY_IO_FAILED;
    if (mkdir(FPSU_LAUNCH_DIR, 0777) != 0 &&
        (stat(FPSU_LAUNCH_DIR, &st) != 0 || !S_ISDIR(st.st_mode))) goto failed;
    if (mkdir(FPSU_DATA_ROOT, 0777) != 0 &&
        (stat(FPSU_DATA_ROOT, &st) != 0 || !S_ISDIR(st.st_mode))) goto failed;
    if (mkdir(FPSU_CACHE_ROOT, 0777) != 0 &&
        (stat(FPSU_CACHE_ROOT, &st) != 0 || !S_ISDIR(st.st_mode))) goto failed;
    remove(go_path);
    if (stat(go_path, &st) == 0) goto failed;
    /* Keep earlier attempts: an empty/truncated log cannot distinguish a local
       HTTP failure from a plugin script guard rejecting the request. */
    file = fopen(FPSU_LAUNCH_LOG, "ab");
    if (!file) goto failed;
    written = fprintf(file, "PSUF_REQUEST %s target=%s\n", ready, base);
    if (fclose(file) != 0 || written < 0) goto failed;
    launch_diagnostics = 1;
    /* Prefer the direct local webMAN HTTP runner. The wm_request polling path
       can accept the request but never execute the script on some HEN setups;
       the direct route gives us an actual READY/ARMED handshake. Keep the
       queue implementation below as a compatibility fallback for future use. */
    use_queue = 0;
    file = fopen(script_path, "wb");
    if (!file) goto failed;
    script_created = 1;
    written = fprintf(file, "logfile " FPSU_LAUNCH_LOG "\n");
    if (written >= 0 && use_queue)
        written = fprintf(file, "abort if not exist %s\n", go_path);
    if (written >= 0)
        written = fprintf(file, "log %s\nwait %s\nabort if not exist %s\nlog %s\nwait xmb\n",
            ready, go_path, go_path, armed);
    if (written >= 0) {
        /* explore_plugin can be visible before the game process has quit.
           Wait for the XMB running mode before touching the mounted disc. */
        int check;
        for (check = 0; check < 12 && written >= 0; ++check)
            written = fprintf(file, "if not xmb\nwait 1\nend if\n");
    }
    if (written >= 0) {
        written = fprintf(file,
            "abort if not xmb\nwait 3\n"
            "abort if not xmb\n"
            "abort if not exist %s\n"
            "del %s\n",
            go_path, go_path);
    }
    if (written >= 0 && !use_queue)
        written = fprintf(file, "del %s\n", script_path);
    if (written >= 0)
        written = fprintf(file, "log PSUF_START\n");
    if (written >= 0 && command[0]) {
        /* Clear the previous target first: a failed mount/map must not launch
           a stale disc or app_home mapping. */
        int check;
        written = fprintf(file, "%s\nlog PSUF_WAIT_UNMOUNT\n", reset);
        for (check = 0; check < 10 && written >= 0; ++check)
            written = fprintf(file, "if exist %s\nwait 1\nend if\n", verify);
        if (written >= 0)
            written = fprintf(file, "abort if exist %s\nlog PSUF_MOUNT\n%s\n", verify, command);
    }
    if (written >= 0 && focus[0]) {
        /* Leave the previous game's icon before searching. If the title is
           missing from the XMB, the game data utility cannot start a game. */
        written = fprintf(file, "abort if not exist %s\n"
            "/xmb.ps3$close_all_list\n"
            "/xmb.ps3$focus_category%%20game\nwait 1\n"
            "/xmb.ps3$focus_segment_index%%20seg_gamedata\nwait 1\n"
            "log PSUF_FOCUS_INSTALLED\n%s\nwait 2\n", verify, focus);
    }
    if (written >= 0) {
        written = fprintf(file, "lwait %s\nabort if not exist %s\nwait 2\n"
            "abort if not xmb\nlog PSUF_PLAY\n%s\n\n", verify, verify, play);
    }
    if (fclose(file) != 0 || written < 0) goto failed;
    if (use_queue) {
        char queue_request[768];
        /* A busy script runner can skip an invocation. Repeat inside the same
           queued request. The script consumes its token before any launch. */
        snprintf(queue_request, sizeof(queue_request),
            "%s;/wait.ps3?2;%s;/wait.ps3?4;%s;/delete_ps3%s;/delete_ps3%s",
            request_path, request_path, request_path, go_path, script_path);
        file = fopen(go_path, "wb");
        if (!file || fclose(file) != 0) goto failed;
        if (launch_publish_request(stage_path, queue_request) != 0) goto failed;
        launch_log_stage("QUEUED_FOR_XMB", 0, 0);
        /* Polling mode is asynchronous. Do not report success merely because
           wm_request accepted the file; wait for the same READY/ARMED markers
           used by the direct HTTP route. */
        result = FPSU_WEBMAN_PLAY_READY_TIMEOUT;
        launch_log_stage("QUEUE_READY_WAIT", 0, FPSU_LAUNCH_READY_POLLS * 50);
        for (attempt = 0; attempt < FPSU_LAUNCH_READY_POLLS; ++attempt) {
            if (launch_script_confirmed(ready)) break;
            usleep(50000);
        }
        if (attempt == FPSU_LAUNCH_READY_POLLS) goto failed;
        result = FPSU_WEBMAN_PLAY_ARMED_TIMEOUT;
        launch_log_stage("QUEUE_ARMED_WAIT", 0, FPSU_LAUNCH_READY_POLLS * 50);
        for (attempt = 0; attempt < FPSU_LAUNCH_READY_POLLS; ++attempt) {
            if (launch_script_confirmed(armed)) {
                launch_diagnostics = 0;
                return 0;
            }
            usleep(50000);
        }
        goto failed;
    }
    /* Keep the connection open until webMAN acknowledges the script. */
    trigger_socket = webman_http_send(request_path);
    if (trigger_socket < 0) { result = trigger_socket; goto failed; }

    result = FPSU_WEBMAN_PLAY_READY_TIMEOUT;
    launch_log_stage("READY_WAIT", 0, FPSU_LAUNCH_READY_POLLS * 50);
    for (attempt = 0; attempt < FPSU_LAUNCH_READY_POLLS; ++attempt) {
        if (launch_script_confirmed(ready)) break;
        /* The combo runner can ignore a trigger while another script is
           active. Retrying before arming cannot start a second game. */
        if (attempt > 0 && attempt % FPSU_LAUNCH_RETRY_POLLS == 0) {
            if (trigger_socket >= 0) close(trigger_socket);
            trigger_socket = webman_http_send(request_path);
            launch_log_stage("READY_RETRY", trigger_socket < 0 ? trigger_socket : 0, attempt * 50);
        }
        usleep(50000);
    }
    if (attempt == FPSU_LAUNCH_READY_POLLS) goto failed;
    launch_log_stage("READY_CONFIRMED", 0, 0);
    result = FPSU_WEBMAN_PLAY_IO_FAILED;
    file = fopen(go_path, "wb");
    if (!file || fclose(file) != 0) goto failed;
    result = FPSU_WEBMAN_PLAY_ARMED_TIMEOUT;
    launch_log_stage("ARMED_WAIT", 0, 3000);
    for (attempt = 0; attempt < 60; ++attempt) {
        if (launch_script_confirmed(armed)) {
            if (trigger_socket >= 0) close(trigger_socket);
            launch_log_stage("ARMED_CONFIRMED", 0, 0);
            launch_diagnostics = 0;
            return 0;
        }
        usleep(50000);
    }

failed:
    if (trigger_socket >= 0) close(trigger_socket);
    if (go_path[0]) remove(go_path);
    if (script_created) remove(script_path);
    launch_log_stage("FAILED", result, errno);
    launch_diagnostics = 0;
    /* Symbolic diagnostics only; main.c owns translated user-facing prose. */
    if (message && message_size && result <= FPSU_WEBMAN_PLAY_NET_INIT_FAILED) {
        const char *stage = "IO";
        switch (result) {
        case FPSU_WEBMAN_PLAY_NET_INIT_FAILED: stage = "NET_INIT"; break;
        case FPSU_WEBMAN_PLAY_SOCKET_FAILED: stage = "SOCKET"; break;
        case FPSU_WEBMAN_PLAY_CONNECT_FAILED: stage = "CONNECT"; break;
        case FPSU_WEBMAN_PLAY_SEND_FAILED: stage = "SEND"; break;
        case FPSU_WEBMAN_PLAY_READY_TIMEOUT: stage = "READY_TIMEOUT"; break;
        case FPSU_WEBMAN_PLAY_ARMED_TIMEOUT: stage = "ARMED_TIMEOUT"; break;
        }
        snprintf(message, message_size, "webMAN [%d:%s]", result, stage);
    }
    return result;
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
    if ((webman_http_get("/ps3mapi.ps3?CORE%20GETVERSION", response, sizeof(response)) != 0 ||
        !ps3mapi_http_core_ready(response)) && !ps3mapi_tcp_core_ready()) {
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

int webman_prepare_ntfs(int clear_cache)
{
    const char *path = clear_cache ? "/refresh.ps3?prepntfs" : "/refresh.ps3?prepntfs(0)";
    int sock = webman_http_send(path);

    if (sock < 0) {
        return -1;
    }
    /* prepNTFS can keep the HTTP response open while webMAN indexes a large
       external drive. The command has already been accepted after the request
       is sent, so do not wait for the full reindex inside the PSUF UI. */
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return 0;
}

int webman_get_ntfs_index(char *response, size_t response_size)
{
    if (!response || response_size < 2) {
        return -1;
    }
    return webman_http_get("/index.ps3?ntfs", response, response_size);
}

int webman_toggle_fps_counter(char *message, size_t message_size)
{
    char response[2048];
    if (message && message_size > 0) message[0] = '\0';
    if (webman_http_get("/fps.ps3", response, sizeof(response)) != 0) {
        if (message && message_size > 0)
            snprintf(message, message_size, "webMAN nao respondeu ao comando do contador");
        return -1;
    }
    if (response_looks_missing(response)) {
        if (message && message_size > 0)
            snprintf(message, message_size, "Comando indisponivel. Atualize o webMAN MOD para uma versao com /fps.ps3.");
        return -1;
    }
    if (message && message_size > 0)
        snprintf(message, message_size, "VshFpsCounter alternado pelo webMAN local");
    return 0;
}

int webman_set_full_video_range(char *message, size_t message_size)
{
    char response[2048];
    if (message && message_size > 0) message[0] = '\0';
    if (webman_http_get("/xmb.ps3$xregistry(/setting/display/rgbOutputRange)=1",
            response, sizeof(response)) != 0 || response_looks_missing(response)) {
        if (message && message_size > 0)
            snprintf(message, message_size, "webMAN nao conseguiu alterar RGB Full Range");
        return -1;
    }
    if (webman_http_get("/xmb.ps3$xregistry(/setting/display/yuvSuperWhite)=1",
            response, sizeof(response)) != 0 || response_looks_missing(response)) {
        if (message && message_size > 0)
            snprintf(message, message_size, "RGB foi alterado, mas Super-White nao confirmou");
        return -1;
    }
    if (message && message_size > 0)
        snprintf(message, message_size, "RGB Full Range e Super-White ativados. Reinicie o PS3 para aplicar.");
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
