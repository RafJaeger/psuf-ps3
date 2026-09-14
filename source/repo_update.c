#include "repo_update.h"
#include "app.h"
#include "patch_db.h"

#include <net/net.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef enum {
    DB_KIND_PIPE = 0,
    DB_KIND_NATIVE60 = 1
} db_kind;

typedef struct {
    const char *name;
    const char *tmp_path;
    const char *final_path;
    db_kind kind;
} db_item;

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return 0;
    }
    return mkdir(path, 0777);
}

static int ends_with_ci(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;
    size_t i;

    if (!text || !suffix) {
        return 0;
    }
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return 0;
    }
    text += text_len - suffix_len;
    for (i = 0; i < suffix_len; ++i) {
        if (tolower((unsigned char)text[i]) != tolower((unsigned char)suffix[i])) {
            return 0;
        }
    }
    return 1;
}

static int url_is_safe_for_webman_query(const char *url)
{
    const char *p;

    if (!url || (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) ||
        strstr(url, "REPLACE_ME") != NULL) {
        return 0;
    }
    for (p = url; *p; ++p) {
        if (*p <= ' ' || *p == '&' || *p == '\r' || *p == '\n') {
            return 0;
        }
    }
    return 1;
}

static int read_update_base_url(char *url, size_t url_size)
{
    FILE *f;
    size_t len;
    char *slash;

    if (!url || url_size == 0) {
        return -1;
    }
    url[0] = '\0';
    f = fopen(FPSU_UPDATE_URL_FILE, "rb");
    if (!f) {
        return -1;
    }
    if (!fgets(url, (int)url_size, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    len = strlen(url);
    while (len > 0 && (url[len - 1] == '\r' || url[len - 1] == '\n' || url[len - 1] == ' ')) {
        url[--len] = '\0';
    }
    if (!url_is_safe_for_webman_query(url)) {
        return -1;
    }

    if (ends_with_ci(url, "patches.csv") || ends_with_ci(url, "graphics_patches.csv") ||
        ends_with_ci(url, "native60.csv") || ends_with_ci(url, "fix_patches.csv")) {
        slash = strrchr(url, '/');
        if (!slash || slash < url + 8) {
            return -1;
        }
        slash[1] = '\0';
    } else if (url[len - 1] != '/') {
        if (len + 1 >= url_size) {
            return -1;
        }
        url[len++] = '/';
        url[len] = '\0';
    }
    return 0;
}

static int build_file_url(char *out, size_t out_size, const char *base_url, const char *name)
{
    size_t base_len;
    size_t name_len;

    if (!out || out_size == 0 || !base_url || !name) {
        return -1;
    }
    base_len = strlen(base_url);
    name_len = strlen(name);
    if (base_len + name_len >= out_size) {
        return -1;
    }
    memcpy(out, base_url, base_len);
    memcpy(out + base_len, name, name_len + 1);
    return url_is_safe_for_webman_query(out) ? 0 : -1;
}

static int send_webman_download(const char *url, const char *target)
{
    int sock;
    int ret;
    struct sockaddr_in server;
    char request[1500];
    char response[512];

    if (!url || !target || strlen(url) > 1024 || strlen(target) > 256) {
        return -1;
    }
    if (netInitialize() != 0) {
        return -1;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        netDeinitialize();
        return -1;
    }

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
        "GET /xmb.ps3/download.ps3?to=%s&url=%s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n",
        target, url);
    ret = (int)write(sock, request, strlen(request));
    if (ret <= 0) {
        close(sock);
        netDeinitialize();
        return -1;
    }
    (void)read(sock, response, sizeof(response));
    shutdown(sock, SHUT_RDWR);
    close(sock);
    netDeinitialize();
    return 0;
}

static int wait_for_download(const char *path)
{
    struct stat st;
    off_t previous_size = -1;
    int stable = 0;
    int i;

    for (i = 0; i < 40; ++i) {
        sleep(1);
        if (stat(path, &st) == 0 && st.st_size > 32) {
            if (st.st_size == previous_size) {
                ++stable;
            } else {
                stable = 0;
                previous_size = st.st_size;
            }
            if (stable >= 2) {
                return 0;
            }
        }
    }
    return -1;
}

static int validate_native60_file(const char *path)
{
    FILE *f;
    char line[256];
    int valid = 0;

    if (!path) {
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        int i;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        if (*p == '#' || *p == '\r' || *p == '\n' || *p == '\0') {
            continue;
        }
        for (i = 0; i < 9; ++i) {
            if (!isalnum((unsigned char)p[i])) {
                fclose(f);
                return 0;
            }
        }
        valid = 1;
        break;
    }
    fclose(f);
    return valid;
}

static int validate_db_file(const db_item *item)
{
    if (!item || !item->tmp_path) {
        return 0;
    }
    if (item->kind == DB_KIND_NATIVE60) {
        return validate_native60_file(item->tmp_path);
    }
    return patch_db_validate_file(item->tmp_path);
}

static int replace_file(const char *tmp_path, const char *final_path)
{
    char old_path[FPSU_MAX_PATH];

    if (!tmp_path || !final_path) {
        return -1;
    }
    snprintf(old_path, sizeof(old_path), "%s.old", final_path);
    remove(old_path);
    rename(final_path, old_path);
    if (rename(tmp_path, final_path) != 0) {
        rename(old_path, final_path);
        return -1;
    }
    remove(old_path);
    return 0;
}

int repo_update_databases(char *message, size_t message_size)
{
    static const db_item items[] = {
        { "patches.csv", FPSU_DATA_ROOT "/patches.csv.download", FPSU_PATCH_DB_UPDATED, DB_KIND_PIPE },
        { "graphics_patches.csv", FPSU_DATA_ROOT "/graphics_patches.csv.download", FPSU_GRAPHICS_DB_UPDATED, DB_KIND_PIPE },
        { "native60.csv", FPSU_DATA_ROOT "/native60.csv.download", FPSU_NATIVE60_DB_UPDATED, DB_KIND_NATIVE60 },
        { "fix_patches.csv", FPSU_DATA_ROOT "/fix_patches.csv.download", FPSU_FIX_DB_UPDATED, DB_KIND_PIPE }
    };
    char base_url[1024];
    char file_url[1200];
    unsigned int i;

    if (message && message_size) {
        message[0] = '\0';
    }
    if (read_update_base_url(base_url, sizeof(base_url)) != 0) {
        if (message && message_size) {
            snprintf(message, message_size, "update_url.txt nao esta configurado");
        }
        return -2;
    }
    if (ensure_dir(FPSU_DATA_ROOT) != 0) {
        if (message && message_size) {
            snprintf(message, message_size, "nao consegui criar /dev_hdd0/FPSU");
        }
        return -1;
    }

    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        remove(items[i].tmp_path);
        if (build_file_url(file_url, sizeof(file_url), base_url, items[i].name) != 0 ||
            send_webman_download(file_url, items[i].tmp_path) != 0 ||
            wait_for_download(items[i].tmp_path) != 0 ||
            !validate_db_file(&items[i])) {
            remove(items[i].tmp_path);
            if (message && message_size) {
                snprintf(message, message_size, "nao baixou banco online: %s", items[i].name);
            }
            return -1;
        }
    }

    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        if (replace_file(items[i].tmp_path, items[i].final_path) != 0) {
            if (message && message_size) {
                snprintf(message, message_size, "nao consegui salvar banco: %s", items[i].name);
            }
            return -1;
        }
    }

    if (message && message_size) {
        snprintf(message, message_size,
            "banco online atualizado: FPS, graficos, 60 nativo e correcoes");
    }
    return 0;
}

int repo_update_patches(char *message, size_t message_size)
{
    return repo_update_databases(message, message_size);
}
