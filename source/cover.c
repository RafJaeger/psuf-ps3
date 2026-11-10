#include "cover.h"

#include <arpa/inet.h>
#include <errno.h>
#include <jpgdec/jpgdec.h>
#include <netdb.h>
#include <net/net.h>
#include <pngdec/pngdec.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sysmodule/sysmodule.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FPSU_COVER_MAX_BYTES (2 * 1024 * 1024)
static int (*g_progress_cb)(void *);
static void *g_progress_user;
static volatile int g_dns_state;
static unsigned int g_dns_address;
static int g_network_ready;

static void cover_dns_thread(void *unused)
{
    struct hostent *host;
    (void)unused;
    host = gethostbyname("art.gametdb.com");
    if (host && host->h_addr_list && host->h_addr_list[0]) {
        memcpy(&g_dns_address, host->h_addr_list[0], 4);
        __sync_synchronize(); g_dns_state = 2;
    } else { __sync_synchronize(); g_dns_state = -1; }
    sysThreadExit(0);
}
void cover_set_progress_callback(int (*callback)(void *), void *user)
{
    g_progress_cb = callback; g_progress_user = user;
}
static int cover_canceled(void) { return g_progress_cb && g_progress_cb(g_progress_user); }

static void cover_trace(const char *phase, const char *detail)
{
    FILE *log = fopen(FPSU_CACHE_ROOT "/covers.log", "a");
    if (log) { fprintf(log, "%lu %s %s\n", (unsigned long)time(NULL), phase, detail ? detail : ""); fclose(log); }
}

static int resolve_cover_host(void)
{
    int waited;
    if (!g_network_ready) {
        if (netInitialize() != 0) return -1;
        g_network_ready = 1;
    }
    /* A failed offline lookup must not disable downloads for this session. */
    if (g_dns_state < 0) g_dns_state = 0;
    if (!g_dns_state) {
        sys_ppu_thread_t tid;
        g_dns_state = 1;
        if (sysThreadCreate(&tid, cover_dns_thread, NULL, 1200, 64 * 1024, 0, "psuf-cover-dns") != 0) {
            g_dns_state = -1; return -1;
        }
    }
    for (waited = 0; waited < 3000 && g_dns_state == 1; waited += 50) {
        if (cover_canceled()) return -1;
        usleep(50000); __sync_synchronize();
    }
    return g_dns_state == 2 ? 0 : -1;
}

static int valid_title_id(const char *title_id)
{
    unsigned int i;
    if (!title_id) return 0;
    for (i = 0; i < 9; ++i) {
        if (i < 4) {
            if (title_id[i] < 'A' || title_id[i] > 'Z') return 0;
        } else if (title_id[i] < '0' || title_id[i] > '9') return 0;
    }
    return title_id[9] == '\0';
}

static int ensure_dir(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0') {
        return -1;
    }
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0777);
}

static int ensure_dir_recursive(const char *path)
{
    char tmp[FPSU_MAX_PATH];
    char *p;

    if (!path || path[0] == '\0' || strlen(path) >= sizeof(tmp)) {
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (ensure_dir(tmp) != 0) {
                return -1;
            }
            *p = '/';
        }
    }
    return ensure_dir(tmp);
}

static void join_path(char *out, size_t out_size, const char *a, const char *b)
{
    size_t len;

    if (!out || out_size == 0 || !a || !b) {
        return;
    }
    len = strlen(a);
    if (len > 0 && a[len - 1] == '/') {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s/%s", a, b);
    }
}

static void dirname_copy(char *out, size_t out_size, const char *path)
{
    char *slash;

    if (!out || out_size == 0 || !path) {
        return;
    }
    snprintf(out, out_size, "%s", path);
    slash = strrchr(out, '/');
    if (slash) {
        *slash = '\0';
    }
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
        char a = text[i];
        char b = suffix[i];
        if (a >= 'a' && a <= 'z') {
            a = (char)(a - ('a' - 'A'));
        }
        if (b >= 'a' && b <= 'z') {
            b = (char)(b - ('a' - 'A'));
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int load_image_file(const char *path, fpsu_cover *cover)
{
    pngData png;
    jpgData jpg;
    struct stat st;
    FILE *image;
    unsigned char signature[8], tail[12];
    int is_png;
    int complete;

    if (!path || !cover || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 32) {
        return -1;
    }
    is_png = ends_with_ci(path, ".png");
    if (!is_png && !ends_with_ci(path, ".jpg") && !ends_with_ci(path, ".jpeg")) return -1;
    image = fopen(path, "rb");
    if (!image) return -1;
    complete = fread(signature, 1, sizeof(signature), image) == sizeof(signature) &&
        fseek(image, -(long)sizeof(tail), SEEK_END) == 0 &&
        fread(tail, 1, sizeof(tail), image) == sizeof(tail);
    fclose(image);
    if (!complete || (is_png ?
        (memcmp(signature, "\x89PNG\r\n\x1a\n", 8) != 0 ||
         memcmp(tail, "\0\0\0\0IEND\xae\x42\x60\x82", 12) != 0) :
        (signature[0] != 0xff || signature[1] != 0xd8 || signature[2] != 0xff ||
         tail[10] != 0xff || tail[11] != 0xd9))) return -1;
    memset(&png, 0, sizeof(png));
    memset(&jpg, 0, sizeof(jpg));

    if (is_png) {
        if (sysModuleIsLoaded(SYSMODULE_PNGDEC) != SYSMODULE_LOADED && sysModuleLoad(SYSMODULE_PNGDEC) != 0) return -1;
    } else {
        if (sysModuleIsLoaded(SYSMODULE_JPGDEC) != SYSMODULE_LOADED && sysModuleLoad(SYSMODULE_JPGDEC) != 0) return -1;
    }
    if (is_png && pngLoadFromFile(path, &png) == 0 && png.bmp_out &&
        png.width > 0 && png.width <= 4096 && png.height > 0 && png.height <= 4096 &&
        png.pitch >= png.width * 4 && png.pitch <= 4096 * 4) {
        cover->pixels = png.bmp_out;
        cover->pitch = png.pitch;
        cover->width = png.width;
        cover->height = png.height;
        return 0;
    }
    free(png.bmp_out);
    if (!is_png && jpgLoadFromFile(path, &jpg) == 0 && jpg.bmp_out &&
        jpg.width > 0 && jpg.width <= 4096 && jpg.height > 0 && jpg.height <= 4096 &&
        jpg.pitch >= jpg.width * 4 && jpg.pitch <= 4096 * 4) {
        cover->pixels = jpg.bmp_out;
        cover->pitch = jpg.pitch;
        cover->width = jpg.width;
        cover->height = jpg.height;
        return 0;
    }
    free(jpg.bmp_out);
    return -1;
}

static int load_cached_cover(const char *path, fpsu_cover *cover)
{
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size > FPSU_COVER_MAX_BYTES) return -1;
    return load_image_file(path, cover);
}

static int wait_socket(int sock, int writable, int seconds);
static int write_all(int sock, const char *data, int len)
{
    int sent = 0;

    while (sent < len) {
        if (wait_socket(sock, 1, 3) <= 0) return -1;
        int written = (int)write(sock, data + sent, (size_t)(len - sent));
        if (written < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (written <= 0) {
            return -1;
        }
        sent += written;
    }
    return 0;
}

static int parse_http_headers(const char *header, long *content_length)
{
    const char *field;

    if ((strncmp(header, "HTTP/1.0 200", 12) != 0 &&
         strncmp(header, "HTTP/1.1 200", 12) != 0) ||
        (header[12] != ' ' && header[12] != '\r')) return -1;
    *content_length = -1;
    field = strstr(header, "\r\n");
    if (!field) return -1;
    for (field += 2; *field != '\r'; ) {
        const char *end = strstr(field, "\r\n");
        if (!end || !memchr(field, ':', (size_t)(end - field))) return -1;
        if (strncasecmp(field, "Transfer-Encoding:", 18) == 0) return -1;
        if (strncasecmp(field, "Content-Length:", 15) == 0) {
            const char *value = field + 15;
            long length = 0;
            while (value < end && (*value == ' ' || *value == '\t')) ++value;
            if (value == end || *value < '0' || *value > '9') return -1;
            while (value < end && *value >= '0' && *value <= '9') {
                if (length > (FPSU_COVER_MAX_BYTES - (*value - '0')) / 10) return -1;
                length = length * 10 + (*value++ - '0');
            }
            while (value < end && (*value == ' ' || *value == '\t')) ++value;
            if (value != end || (*content_length >= 0 && *content_length != length)) return -1;
            *content_length = length;
        }
        field = end + 2;
    }
    return 0;
}

static int wait_socket(int sock, int writable, int seconds)
{
    struct pollfd p;
    int elapsed;
    /* PSL1GHT libc sockets carry SOCKET_FD_MASK; native netPoll needs the raw fd.
       libc select is an ENOSYS stub and FD_SET on a masked fd corrupts memory. */
    p.fd = sock & ~SOCKET_FD_MASK;
    p.events = writable ? POLLOUT : POLLIN;
    for (elapsed = 0; elapsed < seconds * 1000; elapsed += 50) {
        int result;
        if (cover_canceled()) return -1;
        p.revents = 0;
        result = netPoll(&p, 1, 50);
        if (result < 0) return -1;
        if (result > 0) return (p.revents & p.events) ||
            (!writable && (p.revents & POLLHUP)) ? 1 : -1;
    }
    return 0;
}

static int download_http_cover(const char *host, const char *path, const char *target)
{
    int sock = -1;
    int ret = 0;
    int header_len = 0;
    int body_bytes = 0;
    int header_done = 0;
    int status_ok = 0;
    int failed = 0;
    int nonblocking = 1;
    time_t started = time(NULL);
    long content_length = -1;
    struct sockaddr_in server;
    struct timeval timeout;
    char request[1024];
    char header[4096];
    char buffer[2048];
    FILE *out = NULL;

    if (!host || !path || !target || path[0] != '/') {
        return -1;
    }
    cover_trace("resolve", path);
    if (resolve_cover_host() != 0) return -1;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return -1;
    }
    timeout.tv_sec = 1;
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
    memcpy(&server.sin_addr, &g_dns_address, sizeof(server.sin_addr));
    cover_trace("connect", path);
    if (setsockopt(sock, SOL_SOCKET, SO_NBIO, &nonblocking, sizeof(nonblocking)) != 0) {
        close(sock); return -1;
    }
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        if (wait_socket(sock, 1, 3) <= 0) {
            close(sock);
            return -1;
        }
    }
    {
        int socket_error = 0;
        socklen_t socket_error_size = sizeof(socket_error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_size) != 0 ||
            socket_error != 0) {
            close(sock);
            return -1;
        }
    }

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: PSUF\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n", path, host);
    if (write_all(sock, request, (int)strlen(request)) != 0) {
        close(sock);
        return -1;
    }

    out = fopen(target, "wb");
    if (!out) {
        close(sock);
        return -1;
    }

    cover_trace("receive", path);
    for (;;) {
        const char *body = buffer;
        int copy;
        if (time(NULL) - started >= 12 || wait_socket(sock, 0, 3) <= 0) {
            ret = -1;
            break;
        }
        ret = (int)read(sock, buffer, sizeof(buffer));
        if (ret < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (ret <= 0) {
            break;
        }
        copy = ret;
        if (!header_done) {
            int consumed = 0;
            while (consumed < ret && !header_done) {
                if (header_len >= (int)sizeof(header) - 1) {
                    failed = 1;
                    break;
                }
                header[header_len++] = buffer[consumed++];
                if (header_len >= 4 && memcmp(header + header_len - 4, "\r\n\r\n", 4) == 0)
                    header_done = 1;
            }
            if (failed) break;
            if (!header_done) continue;
            header[header_len] = '\0';
            status_ok = parse_http_headers(header, &content_length) == 0;
            if (!status_ok) {
                failed = 1;
                break;
            }
            body = buffer + consumed;
            copy = ret - consumed;
        }
        if (body_bytes > FPSU_COVER_MAX_BYTES - copy ||
            (content_length >= 0 && body_bytes + copy > content_length) ||
            fwrite(body, 1, (size_t)copy, out) != (size_t)copy) {
            failed = 1;
            break;
        }
        body_bytes += copy;
        if (content_length >= 0 && body_bytes == content_length) break;
    }

    if (fclose(out) != 0) {
        failed = 1;
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    cover_trace("received", path);
    if (ret < 0 || failed || !header_done || !status_ok || body_bytes <= 32 ||
        (content_length >= 0 && content_length != body_bytes)) {
        remove(target);
        return -1;
    }
    return 0;
}

static int download_remote_cover(const char *title_id, char *out_path, size_t out_size)
{
    const char *regions[2] = {"EN", "US"};
    char cache_dir[FPSU_MAX_PATH];
    char temp_path[FPSU_MAX_PATH];
    char request_path[160];
    fpsu_cover cached = {0};
    unsigned int i;

    if (!out_path || out_size == 0) {
        return -1;
    }
    out_path[0] = '\0';
    if (!valid_title_id(title_id)) return -1;
    if (strncmp(title_id, "BLES", 4) == 0 || strncmp(title_id, "NPEB", 4) == 0) {
        regions[0] = "EN";
        regions[1] = "US";
    } else if (strncmp(title_id, "BLJM", 4) == 0 || strncmp(title_id, "NPJB", 4) == 0) {
        regions[0] = "JA";
        regions[1] = "EN";
    } else if (strncmp(title_id, "BLAS", 4) == 0 || strncmp(title_id, "NPAB", 4) == 0) {
        regions[0] = "EN";
        regions[1] = "US";
    } else if (strncmp(title_id, "BLUS", 4) == 0 || strncmp(title_id, "NPUS", 4) == 0 ||
        strncmp(title_id, "NPUB", 4) == 0 || strncmp(title_id, "NPUA", 4) == 0) {
        regions[0] = "US";
        regions[1] = "EN";
    }
    snprintf(cache_dir, sizeof(cache_dir), "%s/covers", FPSU_CACHE_ROOT);
    if (snprintf(out_path, out_size, "%s/%s.jpg", cache_dir, title_id) >= (int)out_size) {
        out_path[0] = '\0';
        return -1;
    }
    if (load_cached_cover(out_path, &cached) == 0) {
        cover_release(&cached);
        return 0;
    }
    if (ensure_dir_recursive(cache_dir) != 0) {
        out_path[0] = '\0';
        return -1;
    }
    /* Keep the image extension so the decoder can validate before publication. */
    snprintf(temp_path, sizeof(temp_path), "%s/%s.download.jpg", cache_dir, title_id);

    for (i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        if (cover_canceled()) break;
        snprintf(request_path, sizeof(request_path), "/ps3/cover/%s/%s.jpg",
            regions[i], title_id);
        remove(temp_path);
        if (download_http_cover("art.gametdb.com", request_path, temp_path) == 0 &&
            load_cached_cover(temp_path, &cached) == 0) {
            cover_release(&cached);
            if (!cover_canceled() && rename(temp_path, out_path) == 0) return 0;
        }
    }
    remove(temp_path);
    out_path[0] = '\0';
    return -1;
}

static int try_local_icon(const fpsu_game *game, fpsu_cover *cover)
{
    static const char *cover_roots[] = {"/dev_hdd0/game/BLES80608/USRDIR/covers", "/dev_hdd0/tmp/wm_covers", "/dev_hdd0/GAMES/covers"};
    unsigned int i;
    char dir[FPSU_MAX_PATH];
    char candidate[FPSU_MAX_PATH];

    if (!game || !cover) {
        return -1;
    }
    for (i = 0; i < sizeof(cover_roots) / sizeof(cover_roots[0]); ++i) {
        snprintf(candidate, sizeof(candidate), "%s/%s.JPG", cover_roots[i], game->title_id);
        if (load_image_file(candidate, cover) == 0) return 0;
        snprintf(candidate, sizeof(candidate), "%s/%s.jpg", cover_roots[i], game->title_id);
        if (load_image_file(candidate, cover) == 0) return 0;
    }
    if (game->sfo_path[0] != '\0' && strstr(game->sfo_path, "::") == NULL) {
        dirname_copy(dir, sizeof(dir), game->sfo_path);
        join_path(candidate, sizeof(candidate), dir, "ICON0.PNG");
        if (load_image_file(candidate, cover) == 0) return 0;
    }
    if (game->base_path[0] != '\0' && strstr(game->base_path, ".iso") == NULL &&
        strstr(game->base_path, ".ISO") == NULL) {
        join_path(candidate, sizeof(candidate), game->base_path, "ICON0.PNG");
        if (load_image_file(candidate, cover) == 0) return 0;
        join_path(candidate, sizeof(candidate), game->base_path, "PS3_GAME/ICON0.PNG");
        if (load_image_file(candidate, cover) == 0) return 0;
    }
    snprintf(candidate, sizeof(candidate), "/dev_hdd0/game/%s/ICON0.PNG", game->title_id);
    if (load_image_file(candidate, cover) == 0) return 0;
    if (strcmp(game->base_path, "/dev_bdvd") == 0 &&
        load_image_file("/dev_bdvd/PS3_GAME/ICON0.PNG", cover) == 0) return 0;
    return -1;
}

void cover_release(fpsu_cover *cover)
{
    if (!cover) {
        return;
    }
    if (cover->pixels) {
        free(cover->pixels);
    }
    memset(cover, 0, sizeof(*cover));
}

int cover_cache_for_game(const fpsu_game *game)
{
    char path[FPSU_MAX_PATH];
    fpsu_cover cached = {0};
    if (!game || !valid_title_id(game->title_id)) return -1;
    if (cover_load_cached_or_local_for_game(game, &cached) == 0) {
        cover_release(&cached);
        return 0;
    }
    return download_remote_cover(game->title_id, path, sizeof(path));
}

int cover_load_cached_or_local_for_game(const fpsu_game *game, fpsu_cover *cover)
{
    char cached_path[FPSU_MAX_PATH];

    if (!game || !cover || !valid_title_id(game->title_id)) {
        return -1;
    }
    cover_release(cover);

    snprintf(cached_path, sizeof(cached_path), "%s/covers/%s.jpg",
        FPSU_CACHE_ROOT, game->title_id);
    if (load_cached_cover(cached_path, cover) == 0) {
        return 0;
    }
    if (try_local_icon(game, cover) == 0) return 0;
    cover_release(cover);
    return -1;
}

int cover_load_banner_for_game(const fpsu_game *game, fpsu_cover *cover)
{
    char path[FPSU_MAX_PATH];
    char dir[FPSU_MAX_PATH];
    if (!game || !cover || !valid_title_id(game->title_id)) return -1;
    cover_release(cover);
    if (game->sfo_path[0] && !strstr(game->sfo_path, "::")) {
        dirname_copy(dir, sizeof(dir), game->sfo_path);
        join_path(path, sizeof(path), dir, "PIC1.PNG");
        if (load_image_file(path, cover) == 0) return 0;
    }
    snprintf(path, sizeof(path), "/dev_hdd0/game/%s/PIC1.PNG", game->title_id);
    return load_image_file(path, cover);
}

int cover_load_remote_for_game(const fpsu_game *game, fpsu_cover *cover)
{
    char remote_path[FPSU_MAX_PATH];

    if (!game || !cover || !valid_title_id(game->title_id)) {
        return -1;
    }
    cover_release(cover);
    remote_path[0] = '\0';
    if (download_remote_cover(game->title_id, remote_path, sizeof(remote_path)) == 0 &&
        load_image_file(remote_path, cover) == 0) {
        return 0;
    }
    cover_release(cover);
    return -1;
}

int cover_load_for_game(const fpsu_game *game, fpsu_cover *cover)
{
    if (!game || !cover || !valid_title_id(game->title_id)) {
        return -1;
    }
    if (cover_load_cached_or_local_for_game(game, cover) == 0) return 0;
    return cover_load_remote_for_game(game, cover);
}

enum { PREVIEW_IDLE, PREVIEW_LOADING, PREVIEW_READY };
static int g_preview_state;
static struct {
    fpsu_game game;
    fpsu_cover cover;
    fpsu_cover banner;
} g_preview;

static void cover_preview_thread(void *unused)
{
    (void)unused;
    (void)__atomic_load_n(&g_preview_state, __ATOMIC_ACQUIRE);
    cover_load_cached_or_local_for_game(&g_preview.game, &g_preview.cover);
    cover_load_banner_for_game(&g_preview.game, &g_preview.banner);
    __atomic_store_n(&g_preview_state, PREVIEW_READY, __ATOMIC_RELEASE);
    sysThreadExit(0);
}

int cover_poll_preview(const fpsu_game *game, fpsu_cover *cover, fpsu_cover *banner)
{
    int state = __atomic_load_n(&g_preview_state, __ATOMIC_ACQUIRE);
    sys_ppu_thread_t thread;

    if (state == PREVIEW_LOADING) return 0;
    if (state == PREVIEW_READY) {
        __atomic_store_n(&g_preview_state, PREVIEW_IDLE, __ATOMIC_RELAXED);
        if (game && strcmp(game->title_id, g_preview.game.title_id) == 0 &&
            strcmp(game->sfo_path, g_preview.game.sfo_path) == 0 &&
            strcmp(game->base_path, g_preview.game.base_path) == 0) {
            cover_release(cover);
            cover_release(banner);
            *cover = g_preview.cover;
            *banner = g_preview.banner;
            g_preview.cover = (fpsu_cover){0};
            g_preview.banner = (fpsu_cover){0};
            return 1;
        }
        cover_release(&g_preview.cover);
        cover_release(&g_preview.banner);
    }

    if (!game || !valid_title_id(game->title_id)) return 1;

    g_preview.game = *game;
    __atomic_store_n(&g_preview_state, PREVIEW_LOADING, __ATOMIC_RELEASE);
    if (sysThreadCreate(&thread, cover_preview_thread, NULL, 1200, 64 * 1024, 0,
                        "psuf-cover-preview") != 0) {
        __atomic_store_n(&g_preview_state, PREVIEW_IDLE, __ATOMIC_RELAXED);
        return 1;
    }
    return 0;
}
