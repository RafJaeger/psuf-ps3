#include "scanner.h"
#include "sfo.h"
#include "iso9660.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static const char *g_scan_roots[] = {
    "/dev_hdd0/game",
    "/dev_hdd0/GAMES",
    "/dev_hdd0/GAMEZ",
    "/dev_usb000/GAMES",
    "/dev_usb001/GAMES",
    "/dev_usb002/GAMES",
    "/dev_usb003/GAMES",
    "/dev_usb004/GAMES",
    "/dev_usb005/GAMES",
    "/dev_usb006/GAMES",
    "/dev_usb007/GAMES",
    "/dev_usb000/GAMEZ",
    "/dev_usb001/GAMEZ",
    "/dev_usb002/GAMEZ",
    "/dev_usb003/GAMEZ",
    "/dev_usb004/GAMEZ",
    "/dev_usb005/GAMEZ",
    "/dev_usb006/GAMEZ",
    "/dev_usb007/GAMEZ",
    "/dev_usb000/game",
    "/dev_usb001/game",
    "/dev_usb002/game",
    "/dev_usb003/game",
    "/dev_usb004/game",
    "/dev_usb005/game",
    "/dev_usb006/game",
    "/dev_usb007/game",
    "/dev_ntfs/GAMES",
    "/dev_ntfs/GAMEZ",
    "/dev_ntfs/game",
    "/dev_ntfs000/GAMES",
    "/dev_ntfs001/GAMES",
    "/dev_ntfs002/GAMES",
    "/dev_ntfs003/GAMES",
    "/dev_ntfs004/GAMES",
    "/dev_ntfs005/GAMES",
    "/dev_ntfs006/GAMES",
    "/dev_ntfs007/GAMES",
    "/dev_ntfs000/GAMEZ",
    "/dev_ntfs001/GAMEZ",
    "/dev_ntfs002/GAMEZ",
    "/dev_ntfs003/GAMEZ",
    "/dev_ntfs004/GAMEZ",
    "/dev_ntfs005/GAMEZ",
    "/dev_ntfs006/GAMEZ",
    "/dev_ntfs007/GAMEZ",
    "/dev_ntfs000/game",
    "/dev_ntfs001/game",
    "/dev_ntfs002/game",
    "/dev_ntfs003/game",
    "/dev_ntfs004/game",
    "/dev_ntfs005/game",
    "/dev_ntfs006/game",
    "/dev_ntfs007/game",
    "/dev_ntfs0/GAMES",
    "/dev_ntfs1/GAMES",
    "/dev_ntfs2/GAMES",
    "/dev_ntfs3/GAMES",
    "/dev_ntfs4/GAMES",
    "/dev_ntfs5/GAMES",
    "/dev_ntfs6/GAMES",
    "/dev_ntfs7/GAMES",
    "/dev_ntfs0/GAMEZ",
    "/dev_ntfs1/GAMEZ",
    "/dev_ntfs2/GAMEZ",
    "/dev_ntfs3/GAMEZ",
    "/dev_ntfs4/GAMEZ",
    "/dev_ntfs5/GAMEZ",
    "/dev_ntfs6/GAMEZ",
    "/dev_ntfs7/GAMEZ",
    "/dev_ntfs0/game",
    "/dev_ntfs1/game",
    "/dev_ntfs2/game",
    "/dev_ntfs3/game",
    "/dev_ntfs4/game",
    "/dev_ntfs5/game",
    "/dev_ntfs6/game",
    "/dev_ntfs7/game",
    "/dev_ntfs0:/GAMES",
    "/dev_ntfs1:/GAMES",
    "/dev_ntfs2:/GAMES",
    "/dev_ntfs3:/GAMES",
    "/dev_ntfs4:/GAMES",
    "/dev_ntfs5:/GAMES",
    "/dev_ntfs6:/GAMES",
    "/dev_ntfs7:/GAMES",
    "/dev_ntfs0:/GAMEZ",
    "/dev_ntfs1:/GAMEZ",
    "/dev_ntfs2:/GAMEZ",
    "/dev_ntfs3:/GAMEZ",
    "/dev_ntfs4:/GAMEZ",
    "/dev_ntfs5:/GAMEZ",
    "/dev_ntfs6:/GAMEZ",
    "/dev_ntfs7:/GAMEZ",
    "/dev_ntfs0:/game",
    "/dev_ntfs1:/game",
    "/dev_ntfs2:/game",
    "/dev_ntfs3:/game",
    "/dev_ntfs4:/game",
    "/dev_ntfs5:/game",
    "/dev_ntfs6:/game",
    "/dev_ntfs7:/game",
    "/pvd_usb000/GAMES",
    "/pvd_usb001/GAMES",
    "/pvd_usb002/GAMES",
    "/pvd_usb003/GAMES",
    "/pvd_usb000/GAMEZ",
    "/pvd_usb001/GAMEZ",
    "/pvd_usb002/GAMEZ",
    "/pvd_usb003/GAMEZ",
    "/pvd_usb000/game",
    "/pvd_usb001/game",
    "/pvd_usb002/game",
    "/pvd_usb003/game"
};

static const char *g_iso_roots[] = {
    "/dev_hdd0/PS3ISO",
    "/dev_usb000/PS3ISO",
    "/dev_usb001/PS3ISO",
    "/dev_usb002/PS3ISO",
    "/dev_usb003/PS3ISO",
    "/dev_usb004/PS3ISO",
    "/dev_usb005/PS3ISO",
    "/dev_usb006/PS3ISO",
    "/dev_usb007/PS3ISO",
    "/dev_ntfs/PS3ISO",
    "/dev_ntfs000/PS3ISO",
    "/dev_ntfs001/PS3ISO",
    "/dev_ntfs002/PS3ISO",
    "/dev_ntfs003/PS3ISO",
    "/dev_ntfs004/PS3ISO",
    "/dev_ntfs005/PS3ISO",
    "/dev_ntfs006/PS3ISO",
    "/dev_ntfs007/PS3ISO",
    "/dev_ntfs0/PS3ISO",
    "/dev_ntfs1/PS3ISO",
    "/dev_ntfs2/PS3ISO",
    "/dev_ntfs3/PS3ISO",
    "/dev_ntfs4/PS3ISO",
    "/dev_ntfs5/PS3ISO",
    "/dev_ntfs6/PS3ISO",
    "/dev_ntfs7/PS3ISO",
    "/dev_ntfs0:/PS3ISO",
    "/dev_ntfs1:/PS3ISO",
    "/dev_ntfs2:/PS3ISO",
    "/dev_ntfs3:/PS3ISO",
    "/dev_ntfs4:/PS3ISO",
    "/dev_ntfs5:/PS3ISO",
    "/dev_ntfs6:/PS3ISO",
    "/dev_ntfs7:/PS3ISO",
    "/pvd_usb000/PS3ISO",
    "/pvd_usb001/PS3ISO",
    "/pvd_usb002/PS3ISO",
    "/pvd_usb003/PS3ISO"
};

static const char *g_webman_sfo_roots[] = {
    "/dev_hdd0/tmp/wmtmp",
    "/dev_hdd0/tmp/mmtmp"
};

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void join_path(char *out, size_t out_size, const char *a, const char *b)
{
    size_t n;
    if (!out || out_size == 0) {
        return;
    }
    n = strlen(a);
    if (n > 0 && a[n - 1] == '/') {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s/%s", a, b);
    }
}

static void dirname_copy(char *out, size_t out_size, const char *path)
{
    char *slash;
    if (!out || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s", path);
    slash = strrchr(out, '/');
    if (slash) {
        *slash = '\0';
    }
}

static void find_eboot(fpsu_game *game)
{
    char candidate[FPSU_MAX_PATH];
    char base[FPSU_MAX_PATH];

    game->eboot_path[0] = '\0';
    dirname_copy(base, sizeof(base), game->sfo_path);

    join_path(candidate, sizeof(candidate), base, "USRDIR/EBOOT.BIN");
    if (file_exists(candidate)) {
        snprintf(game->eboot_path, sizeof(game->eboot_path), "%s", candidate);
        return;
    }

    join_path(candidate, sizeof(candidate), game->base_path, "PS3_GAME/USRDIR/EBOOT.BIN");
    if (file_exists(candidate)) {
        snprintf(game->eboot_path, sizeof(game->eboot_path), "%s", candidate);
        return;
    }
}

static int same_title_text(const char *a, const char *b)
{
    return a && b && a[0] != '\0' && b[0] != '\0' && strcmp(a, b) == 0;
}

static int version_is_unknown(const char *version)
{
    return !version || version[0] == '\0' || strcmp(version, "*") == 0 ||
        strcmp(version, "00.00") == 0;
}

static int same_text_ci(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    while (*a || *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        if (*a) {
            ++a;
        }
        if (*b) {
            ++b;
        }
    }
    return 1;
}

static int ends_with_ci_suffix(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) {
        return 0;
    }
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return 0;
    }
    return same_text_ci(text + text_len - suffix_len, suffix);
}

static int iso_tail_is_supported(const char *tail)
{
    const char *p;

    if (!tail) {
        return 0;
    }
    if (tail[0] == '\0') {
        return 1;
    }
    if (tail[0] != '.') {
        return 0;
    }

    p = tail + 1;
    if (*p != '\0' && isdigit((unsigned char)*p)) {
        while (*p) {
            if (!isdigit((unsigned char)*p)) {
                return 0;
            }
            ++p;
        }
        return 1;
    }

    return same_text_ci(tail + 1, "ntfs") || same_text_ci(tail + 1, "enc");
}

static int strip_sfo_suffix_path(char *path)
{
    size_t len;

    if (!path) {
        return 0;
    }
    len = strlen(path);
    if (len < 4 || !ends_with_ci_suffix(path, ".sfo")) {
        return 0;
    }
    path[len - 4] = '\0';
    return 1;
}

static void enrich_game_from_installed_update(fpsu_game *game)
{
    char root[FPSU_MAX_PATH];
    char sfo[FPSU_MAX_PATH];
    fpsu_game update;

    if (!game || game->title_id[0] == '\0' || strcmp(game->category, "GD") == 0) {
        return;
    }

    snprintf(root, sizeof(root), "/dev_hdd0/game/%s", game->title_id);
    join_path(sfo, sizeof(sfo), root, "PARAM.SFO");
    if (!file_exists(sfo) || sfo_read_game(sfo, &update) != 0 ||
        !same_title_text(game->title_id, update.title_id)) {
        return;
    }

    snprintf(update.base_path, sizeof(update.base_path), "%s", root);
    find_eboot(&update);
    if (!version_is_unknown(update.version)) {
        snprintf(game->version, sizeof(game->version), "%s", update.version);
    }
    if (game->title[0] == '\0' || strcmp(game->title, "Unknown title") == 0) {
        snprintf(game->title, sizeof(game->title), "%s", update.title);
    }
    if (update.eboot_path[0] != '\0') {
        snprintf(game->eboot_path, sizeof(game->eboot_path), "%s", update.eboot_path);
    }
    if (strcmp(game->category, "DG") == 0) {
        snprintf(game->category, sizeof(game->category), "%s", "DG+GD");
    }
}

static int scanner_game_should_list(const fpsu_game *game)
{
    if (!game || game->title_id[0] == '\0') {
        return 0;
    }
    if (strcmp(game->category, "GD") == 0 ||
        strcmp(game->category, "SD") == 0 ||
        strcmp(game->category, "PP") == 0) {
        return 0;
    }
    return 1;
}

static int ends_with_iso(const char *name)
{
    const char *p;
    if (!name) {
        return 0;
    }
    for (p = name; *p; ++p) {
        if (p[0] == '.' && p[1] && p[2] && p[3] &&
            tolower((unsigned char)p[1]) == 'i' &&
            tolower((unsigned char)p[2]) == 's' &&
            tolower((unsigned char)p[3]) == 'o') {
            if (iso_tail_is_supported(p + 4)) {
                return 1;
            }
        }
    }
    return 0;
}

static int iso_split_part_number(const char *name, int *part)
{
    size_t len;
    size_t digits_start;
    int value = 0;

    if (!name) {
        return 0;
    }
    len = strlen(name);
    digits_start = len;
    while (digits_start > 0 && isdigit((unsigned char)name[digits_start - 1])) {
        --digits_start;
    }
    if (digits_start == len || digits_start < 6 || name[digits_start - 1] != '.') {
        return 0;
    }
    if (tolower((unsigned char)name[digits_start - 4]) != 'i' ||
        tolower((unsigned char)name[digits_start - 3]) != 's' ||
        tolower((unsigned char)name[digits_start - 2]) != 'o' ||
        name[digits_start - 5] != '.') {
        return 0;
    }
    while (digits_start < len) {
        value = (value * 10) + (name[digits_start] - '0');
        ++digits_start;
    }
    if (part) {
        *part = value;
    }
    return 1;
}

static int title_id_from_iso_name(const char *path, char *out, size_t out_size)
{
    const char *p;
    if (!path || !out || out_size < 10) {
        return 0;
    }
    for (p = path; *p; ++p) {
        int i;
        if (!isupper((unsigned char)p[0]) || !isupper((unsigned char)p[1]) ||
            !isupper((unsigned char)p[2]) || !isupper((unsigned char)p[3])) {
            continue;
        }
        for (i = 4; i < 9; ++i) {
            if (!isdigit((unsigned char)p[i])) {
                break;
            }
        }
        if (i == 9) {
            memcpy(out, p, 9);
            out[9] = '\0';
            return 1;
        }
    }
    return 0;
}

static void title_from_iso_name(const char *path, char *out, size_t out_size)
{
    const char *name;
    const char *dot;
    size_t len;
    if (!out || out_size == 0) {
        return;
    }
    name = strrchr(path ? path : "", '/');
    name = name ? name + 1 : (path ? path : "");
    len = strlen(name);
    if (iso_split_part_number(name, NULL)) {
        while (len > 0 && isdigit((unsigned char)name[len - 1])) {
            --len;
        }
        if (len > 0 && name[len - 1] == '.') {
            --len;
        }
    }
    for (dot = name; *dot; ++dot) {
        if (dot[0] == '.' &&
            tolower((unsigned char)dot[1]) == 'i' &&
            tolower((unsigned char)dot[2]) == 's' &&
            tolower((unsigned char)dot[3]) == 'o') {
            len = (size_t)(dot - name);
            break;
        }
    }
    if (!*dot) {
        dot = strrchr(name, '.');
        len = dot && dot > name ? (size_t)(dot - name) : strlen(name);
    }
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

static int scanner_scan_iso(const char *path, fpsu_game *out)
{
    unsigned char *sfo_data = NULL;
    size_t sfo_size = 0;

    if (!path || !out || !ends_with_iso(path)) {
        return -1;
    }
    if (iso9660_read_file(path, "PS3_GAME/PARAM.SFO", &sfo_data, &sfo_size) != 0) {
        memset(out, 0, sizeof(*out));
        if (!title_id_from_iso_name(path, out->title_id, sizeof(out->title_id))) {
            return -1;
        }
        snprintf(out->version, sizeof(out->version), "%s", "*");
        snprintf(out->category, sizeof(out->category), "%s", "DG");
        title_from_iso_name(path, out->title, sizeof(out->title));
        snprintf(out->base_path, sizeof(out->base_path), "%s", path);
        snprintf(out->sfo_path, sizeof(out->sfo_path), "%s::/PS3_GAME/PARAM.SFO", path);
        out->eboot_path[0] = '\0';
        out->is_disc_folder = 1;
        out->is_iso = 1;
        enrich_game_from_installed_update(out);
        return 0;
    }
    if (sfo_parse_game_buffer(sfo_data, sfo_size, out) != 0) {
        free(sfo_data);
        return -1;
    }
    free(sfo_data);

    snprintf(out->base_path, sizeof(out->base_path), "%s", path);
    snprintf(out->sfo_path, sizeof(out->sfo_path), "%s::/PS3_GAME/PARAM.SFO", path);
    out->eboot_path[0] = '\0';
    out->is_disc_folder = 1;
    out->is_iso = 1;
    enrich_game_from_installed_update(out);
    return 0;
}

int scanner_scan_iso_name_only(const char *path, fpsu_game *out)
{
    if (!path || !out || !ends_with_iso(path)) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (!title_id_from_iso_name(path, out->title_id, sizeof(out->title_id))) {
        return -1;
    }

    out->version[0] = '\0';
    snprintf(out->category, sizeof(out->category), "%s", "DG");
    title_from_iso_name(path, out->title, sizeof(out->title));
    snprintf(out->base_path, sizeof(out->base_path), "%s", path);
    snprintf(out->sfo_path, sizeof(out->sfo_path), "%s::/PS3_GAME/PARAM.SFO", path);
    out->eboot_path[0] = '\0';
    out->is_disc_folder = 1;
    out->is_iso = 1;
    enrich_game_from_installed_update(out);
    return 0;
}

int scanner_scan_mounted_iso_metadata(const char *mounted_path, fpsu_game *out)
{
    fpsu_game name_game;
    fpsu_game bdvd_game;
    int have_name;
    int have_bdvd;

    if (!mounted_path || !out) {
        return -1;
    }

    have_name = scanner_scan_iso_name_only(mounted_path, &name_game) == 0;
    have_bdvd = scanner_scan_path("/dev_bdvd", &bdvd_game) == 0;

    if (have_bdvd && (!have_name || same_title_text(name_game.title_id, bdvd_game.title_id))) {
        *out = bdvd_game;
        snprintf(out->base_path, sizeof(out->base_path), "%s", mounted_path);
        out->is_iso = 1;
        enrich_game_from_installed_update(out);
        return 0;
    }

    if (have_name) {
        *out = name_game;
        return 0;
    }

    return -1;
}

static int scanner_scan_cached_sfo(const char *sfo_path, fpsu_game *out)
{
    char base[FPSU_MAX_PATH];

    if (!sfo_path || !out || !ends_with_ci_suffix(sfo_path, ".sfo")) {
        return -1;
    }
    if (sfo_read_game(sfo_path, out) != 0) {
        return -1;
    }

    snprintf(base, sizeof(base), "%s", sfo_path);
    strip_sfo_suffix_path(base);
    snprintf(out->base_path, sizeof(out->base_path), "%s", base);
    snprintf(out->sfo_path, sizeof(out->sfo_path), "%s", sfo_path);
    out->eboot_path[0] = '\0';
    out->is_disc_folder = 1;
    out->is_iso = ends_with_iso(base) || strstr(base, ".iso.") != NULL || strstr(base, ".ISO.") != NULL;
    if (out->category[0] == '\0') {
        snprintf(out->category, sizeof(out->category), "%s", "DG");
    }
    enrich_game_from_installed_update(out);
    return 0;
}

int scanner_scan_path(const char *path, fpsu_game *out)
{
    char sfo[FPSU_MAX_PATH];
    char alt[FPSU_MAX_PATH];

    if (!path || !out) {
        return -1;
    }

    if (ends_with_iso(path)) {
        return scanner_scan_iso(path, out);
    }

    join_path(sfo, sizeof(sfo), path, "PARAM.SFO");
    if (!file_exists(sfo)) {
        join_path(alt, sizeof(alt), path, "PS3_GAME/PARAM.SFO");
        if (!file_exists(alt)) {
            return -1;
        }
        snprintf(sfo, sizeof(sfo), "%s", alt);
    }

    if (sfo_read_game(sfo, out) != 0) {
        return -1;
    }

    snprintf(out->base_path, sizeof(out->base_path), "%s", path);
    out->is_disc_folder = strstr(sfo, "/PS3_GAME/PARAM.SFO") != NULL;
    out->is_iso = 0;
    if (strcmp(path, "/dev_bdvd") == 0) {
        out->eboot_path[0] = '\0';
        enrich_game_from_installed_update(out);
        return 0;
    }
    find_eboot(out);
    enrich_game_from_installed_update(out);
    return 0;
}

int scanner_scan_all(fpsu_game *games, int max_games, fpsu_scan_progress_cb cb, void *user)
{
    int found = 0;
    int cancel = 0;
    unsigned int root_i;

    if (!games || max_games <= 0) {
        return 0;
    }

    for (root_i = 0; root_i < sizeof(g_scan_roots) / sizeof(g_scan_roots[0]); ++root_i) {
        const char *root = g_scan_roots[root_i];
        DIR *dir;
        struct dirent *entry;
        fpsu_game mounted;

        if (cb && cb(root, found, user)) {
            break;
        }

        if (strcmp(root, "/dev_bdvd") == 0 && scanner_scan_path(root, &mounted) == 0) {
            if (found < max_games && scanner_game_should_list(&mounted)) {
                games[found++] = mounted;
            }
            continue;
        }

        dir = opendir(root);
        if (!dir) {
            continue;
        }

        while ((entry = readdir(dir)) != NULL) {
            char candidate[FPSU_MAX_PATH];
            fpsu_game game;

            if (found >= max_games) {
                break;
            }
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            join_path(candidate, sizeof(candidate), root, entry->d_name);
            if (scanner_scan_path(candidate, &game) == 0 && scanner_game_should_list(&game)) {
                games[found++] = game;
                if (cb && cb(candidate, found, user)) {
                    cancel = 1;
                    break;
                }
            }
        }
        closedir(dir);
        if (cancel) {
            break;
        }
    }

    for (root_i = 0; root_i < sizeof(g_iso_roots) / sizeof(g_iso_roots[0]) && !cancel; ++root_i) {
        const char *root = g_iso_roots[root_i];
        DIR *dir;
        struct dirent *entry;

        if (cb && cb(root, found, user)) {
            break;
        }
        dir = opendir(root);
        if (!dir) {
            continue;
        }
        while ((entry = readdir(dir)) != NULL) {
            char candidate[FPSU_MAX_PATH];
            fpsu_game game;
            int split_part = -1;
            if (found >= max_games) {
                break;
            }
            if (entry->d_name[0] == '.' || !ends_with_iso(entry->d_name)) {
                continue;
            }
            if (iso_split_part_number(entry->d_name, &split_part) && split_part > 0) {
                continue;
            }
            join_path(candidate, sizeof(candidate), root, entry->d_name);
            if (scanner_scan_iso(candidate, &game) == 0 && scanner_game_should_list(&game)) {
                games[found++] = game;
                if (cb && cb(candidate, found, user)) {
                    cancel = 1;
                    break;
                }
            }
        }
        closedir(dir);
    }

    for (root_i = 0; root_i < sizeof(g_webman_sfo_roots) / sizeof(g_webman_sfo_roots[0]) && !cancel; ++root_i) {
        const char *root = g_webman_sfo_roots[root_i];
        DIR *dir;
        struct dirent *entry;

        if (cb && cb(root, found, user)) {
            break;
        }
        dir = opendir(root);
        if (!dir) {
            continue;
        }
        while ((entry = readdir(dir)) != NULL) {
            char candidate[FPSU_MAX_PATH];
            fpsu_game game;

            if (found >= max_games) {
                break;
            }
            if (entry->d_name[0] == '.' || !ends_with_ci_suffix(entry->d_name, ".sfo")) {
                continue;
            }
            join_path(candidate, sizeof(candidate), root, entry->d_name);
            if (scanner_scan_cached_sfo(candidate, &game) == 0 && scanner_game_should_list(&game)) {
                games[found++] = game;
                if (cb && cb(candidate, found, user)) {
                    cancel = 1;
                    break;
                }
            }
        }
        closedir(dir);
    }

    return found;
}

static int buffer_has_string(const unsigned char *buf, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    size_t i;
    if (n == 0 || len < n) {
        return 0;
    }
    for (i = 0; i <= len - n; ++i) {
        if (memcmp(buf + i, needle, n) == 0) {
            return 1;
        }
    }
    return 0;
}

static int buffer_has_u32_be(const unsigned char *buf, size_t len, uint32_t value)
{
    unsigned char p[4];
    size_t i;
    p[0] = (unsigned char)((value >> 24) & 0xff);
    p[1] = (unsigned char)((value >> 16) & 0xff);
    p[2] = (unsigned char)((value >> 8) & 0xff);
    p[3] = (unsigned char)(value & 0xff);
    if (len < 4) {
        return 0;
    }
    for (i = 0; i <= len - 4; ++i) {
        if (memcmp(buf + i, p, 4) == 0) {
            return 1;
        }
    }
    return 0;
}

static uint32_t read_u32_be_at(const unsigned char *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
        ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

static int buffer_has_u64_be(const unsigned char *buf, size_t len, uint64_t value)
{
    unsigned char p[8];
    size_t i;
    p[0] = (unsigned char)((value >> 56) & 0xff);
    p[1] = (unsigned char)((value >> 48) & 0xff);
    p[2] = (unsigned char)((value >> 40) & 0xff);
    p[3] = (unsigned char)((value >> 32) & 0xff);
    p[4] = (unsigned char)((value >> 24) & 0xff);
    p[5] = (unsigned char)((value >> 16) & 0xff);
    p[6] = (unsigned char)((value >> 8) & 0xff);
    p[7] = (unsigned char)(value & 0xff);
    if (len < 8) {
        return 0;
    }
    for (i = 0; i <= len - 8; ++i) {
        if (memcmp(buf + i, p, 8) == 0) {
            return 1;
        }
    }
    return 0;
}

static int buffer_has_any_string(const unsigned char *buf, size_t len, const char **needles, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (buffer_has_string(buf, len, needles[i])) {
            return 1;
        }
    }
    return 0;
}

static int scanner_find_patterns_with_limit(const fpsu_game *game, fpsu_pattern_progress_cb cb,
    void *user, unsigned int max_scan)
{
    FILE *f;
    unsigned char *buf;
    size_t read_bytes;
    unsigned int scanned = 0;
    unsigned int limit = max_scan;
    unsigned int last_report = 0;
    int score = 0;
    long file_size;
    const char *fps_strings[] = {
        "MaxFPS",
        "maxfps",
        "sys_maxfps",
        "fps_max",
        "FPSLimit",
        "FrameRateLimit",
        "FramerateLimit",
        "FrameRate",
        "Framerate",
        "FixedFrame",
        "FixedStep",
        "TimeStep"
    };
    const char *sync_strings[] = {
        "VSyncOnFlip",
        "vsync",
        "VSync",
        "vblank",
        "VBlank",
        "cellGcmSetFlipMode",
        "cellVideoOut"
    };
    const char *config_strings[] = {
        "autoexec.cfg",
        "Ps3GameSettings.cfg",
        "ENVIRONMENT_PS3.CFG",
        "sys_spec",
        "r_VSync"
    };

    if (!game || game->eboot_path[0] == '\0') {
        return 0;
    }
    if (limit == 0) {
        limit = 0xffffffffu;
    }

    f = fopen(game->eboot_path, "rb");
    if (!f) {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) == 0) {
        file_size = ftell(f);
        if (file_size > 0 && (unsigned long)file_size < limit) {
            limit = (unsigned int)file_size;
        }
        fseek(f, 0, SEEK_SET);
    }

    if (cb && cb(game, 0, limit, user)) {
        fclose(f);
        return -1;
    }

    buf = (unsigned char *)malloc(FPSU_SCAN_CHUNK);
    if (!buf) {
        fclose(f);
        return 0;
    }

    while (scanned < limit) {
        unsigned int remaining = limit - scanned;
        size_t want = remaining < FPSU_SCAN_CHUNK ? (size_t)remaining : FPSU_SCAN_CHUNK;
        read_bytes = fread(buf, 1, want, f);
        if (read_bytes == 0) {
            break;
        }
        if (buffer_has_any_string(buf, read_bytes, fps_strings,
                (int)(sizeof(fps_strings) / sizeof(fps_strings[0])))) {
            score += 3;
        }
        if (buffer_has_any_string(buf, read_bytes, sync_strings,
                (int)(sizeof(sync_strings) / sizeof(sync_strings[0])))) {
            score += 2;
        }
        if (buffer_has_any_string(buf, read_bytes, config_strings,
                (int)(sizeof(config_strings) / sizeof(config_strings[0])))) {
            score += 2;
        }
        if (buffer_has_u32_be(buf, read_bytes, 0x41f00000u) ||
            buffer_has_u32_be(buf, read_bytes, 0x42700000u) ||
            buffer_has_u32_be(buf, read_bytes, 0x3d088889u) ||
            buffer_has_u32_be(buf, read_bytes, 0x3c888889u) ||
            buffer_has_u32_be(buf, read_bytes, 0x3f888889u) ||
            buffer_has_u32_be(buf, read_bytes, 0x0000001eu) ||
            buffer_has_u32_be(buf, read_bytes, 0x0000003cu) ||
            buffer_has_u32_be(buf, read_bytes, 0x2c030001u) ||
            buffer_has_u32_be(buf, read_bytes, 0x28030001u)) {
            score += 1;
        }
        if (buffer_has_u64_be(buf, read_bytes, 0x403e000000000000ull) ||
            buffer_has_u64_be(buf, read_bytes, 0x404e000000000000ull) ||
            buffer_has_u64_be(buf, read_bytes, 0x3fa1111111111111ull) ||
            buffer_has_u64_be(buf, read_bytes, 0x3f91111111111111ull)) {
            score += 1;
        }
        scanned += (unsigned int)read_bytes;
        if (cb && (scanned - last_report >= (512u * 1024u) || scanned >= limit ||
                read_bytes < FPSU_SCAN_CHUNK)) {
            last_report = scanned;
            if (cb(game, scanned, limit, user)) {
                free(buf);
                fclose(f);
                return -1;
            }
        }
        if (read_bytes < FPSU_SCAN_CHUNK) {
            break;
        }
    }

    free(buf);
    fclose(f);
    return score >= 2;
}

int scanner_find_patterns_ex(const fpsu_game *game, fpsu_pattern_progress_cb cb, void *user)
{
    return scanner_find_patterns_with_limit(game, cb, user, FPSU_EBOOT_SCAN_LIMIT);
}

int scanner_find_patterns_full_ex(const fpsu_game *game, fpsu_pattern_progress_cb cb, void *user)
{
    return scanner_find_patterns_with_limit(game, cb, user, 0);
}

int scanner_find_patterns(const fpsu_game *game)
{
    return scanner_find_patterns_ex(game, NULL, NULL) > 0;
}

typedef struct {
    uint32_t from;
    uint32_t to;
} force_map;

static int force_map_value(fpsu_patch_kind kind, uint32_t value, uint32_t *patched)
{
    static const force_map maps_60[] = {
        {0x0000001eu, 0x0000003cu},
        {0x0000001fu, 0x0000003cu},
        {0x00000020u, 0x0000003cu},
        {0x41f00000u, 0x42700000u},
        {0x41f80000u, 0x42700000u},
        {0x42000000u, 0x42700000u},
        {0x403e0000u, 0x404e0000u},
        {0x3d088889u, 0x3c888889u},
        {0x3fa11111u, 0x3f911111u},
        {0x3f888889u, 0x3f088889u}
    };
    static const force_map maps_unlock[] = {
        {0x0000001eu, 0x00000078u},
        {0x0000001fu, 0x00000078u},
        {0x00000020u, 0x00000078u},
        {0x0000003cu, 0x00000078u},
        {0x41f00000u, 0x42f00000u},
        {0x41f80000u, 0x42f00000u},
        {0x42000000u, 0x42f00000u},
        {0x42700000u, 0x42f00000u},
        {0x403e0000u, 0x405e0000u},
        {0x404e0000u, 0x405e0000u},
        {0x3d088889u, 0x3c088889u},
        {0x3c888889u, 0x3c088889u},
        {0x3fa11111u, 0x3f811111u},
        {0x3f911111u, 0x3f811111u},
        {0x3f888889u, 0x3e888889u},
        {0x3f088889u, 0x3e888889u},
        {0x2c030001u, 0x38600001u},
        {0x28030001u, 0x38600001u},
        {0x2b800000u, 0x38600001u},
        {0x2b090000u, 0x38600001u}
    };
    const force_map *maps;
    int count;
    int i;

    if (!patched) {
        return 0;
    }
    if (kind == FPSU_PATCH_60) {
        maps = maps_60;
        count = (int)(sizeof(maps_60) / sizeof(maps_60[0]));
    } else if (kind == FPSU_PATCH_UNLOCK) {
        maps = maps_unlock;
        count = (int)(sizeof(maps_unlock) / sizeof(maps_unlock[0]));
    } else {
        return 0;
    }
    if ((value & 0xfc000000u) == 0x2c000000u ||
        (value & 0xfc000000u) == 0x28000000u ||
        (value & 0xfc000000u) == 0x38000000u ||
        (value & 0xfc000000u) == 0x60000000u) {
        uint32_t imm = value & 0xffffu;
        if (kind == FPSU_PATCH_60 && (imm == 0x001eu || imm == 0x001fu || imm == 0x0020u)) {
            *patched = (value & 0xffff0000u) | 0x003cu;
            return 1;
        }
        if (kind == FPSU_PATCH_UNLOCK &&
            (imm == 0x001eu || imm == 0x001fu || imm == 0x0020u || imm == 0x003cu)) {
            *patched = (value & 0xffff0000u) | 0x0078u;
            return 1;
        }
    }
    for (i = 0; i < count; ++i) {
        if (maps[i].from == value) {
            *patched = maps[i].to;
            return 1;
        }
    }
    return 0;
}

static int force_map_u64(fpsu_patch_kind kind, uint32_t hi, uint32_t lo,
    uint32_t *patched_hi, uint32_t *patched_lo)
{
    if (!patched_hi || !patched_lo) {
        return 0;
    }

    if (kind == FPSU_PATCH_60) {
        if (hi == 0x403e0000u && lo == 0x00000000u) {
            *patched_hi = 0x404e0000u;
            *patched_lo = 0x00000000u;
            return 1;
        }
        if (hi == 0x3fa11111u && lo == 0x11111111u) {
            *patched_hi = 0x3f911111u;
            *patched_lo = 0x11111111u;
            return 1;
        }
    } else if (kind == FPSU_PATCH_UNLOCK) {
        if ((hi == 0x403e0000u || hi == 0x404e0000u) && lo == 0x00000000u) {
            *patched_hi = 0x405e0000u;
            *patched_lo = 0x00000000u;
            return 1;
        }
        if ((hi == 0x3fa11111u || hi == 0x3f911111u) && lo == 0x11111111u) {
            *patched_hi = 0x3f811111u;
            *patched_lo = 0x11111111u;
            return 1;
        }
    }
    return 0;
}

static int append_force_line(char *out, size_t out_size, unsigned int address, uint32_t value)
{
    char line[40];
    size_t used;
    size_t len;

    if (!out || out_size == 0) {
        return 0;
    }
    used = strlen(out);
    snprintf(line, sizeof(line), "%s0 %08X %08X", used > 0 ? ";" : "", address, value);
    len = strlen(line);
    if (used + len + 1 >= out_size) {
        return 0;
    }
    memcpy(out + used, line, len + 1);
    return 1;
}

int scanner_build_force_payload_ex(const fpsu_game *game, fpsu_patch_kind kind,
    fpsu_pattern_progress_cb cb, void *user, char *out, size_t out_size)
{
    FILE *f;
    unsigned char *buf;
    unsigned int scanned = 0;
    unsigned int limit = 0xffffffffu;
    unsigned int last_report = 0;
    long file_size;
    int patches = 0;
    const int max_patches = 24;

    if (!game || game->eboot_path[0] == '\0' || !out || out_size == 0 ||
        kind == FPSU_PATCH_NONE) {
        return 0;
    }
    out[0] = '\0';

    f = fopen(game->eboot_path, "rb");
    if (!f) {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) == 0) {
        file_size = ftell(f);
        if (file_size > 0) {
            limit = (unsigned int)file_size;
        }
        fseek(f, 0, SEEK_SET);
    }

    if (cb && cb(game, 0, limit, user)) {
        fclose(f);
        return -1;
    }

    buf = (unsigned char *)malloc(FPSU_SCAN_CHUNK + 4);
    if (!buf) {
        fclose(f);
        return 0;
    }

    while (scanned < limit && patches < max_patches) {
        unsigned int remaining = limit - scanned;
        size_t want = remaining < FPSU_SCAN_CHUNK ? (size_t)remaining : FPSU_SCAN_CHUNK;
        size_t read_bytes = fread(buf, 1, want, f);
        size_t i;

        if (read_bytes == 0) {
            break;
        }

        for (i = 0; i + 4 <= read_bytes && patches < max_patches; i += 4) {
            uint32_t patched;
            uint32_t value = read_u32_be_at(buf + i);
            if (i + 8 <= read_bytes) {
                uint32_t lo = read_u32_be_at(buf + i + 4);
                uint32_t patched_lo;
                if (patches + 1 < max_patches &&
                    force_map_u64(kind, value, lo, &patched, &patched_lo)) {
                    if (append_force_line(out, out_size, scanned + (unsigned int)i, patched) &&
                        append_force_line(out, out_size, scanned + (unsigned int)i + 4u, patched_lo)) {
                        patches += 2;
                        i += 4;
                        continue;
                    }
                }
            }
            if (force_map_value(kind, value, &patched)) {
                if (append_force_line(out, out_size, scanned + (unsigned int)i, patched)) {
                    ++patches;
                }
            }
        }

        scanned += (unsigned int)read_bytes;
        if (cb && (scanned - last_report >= (512u * 1024u) || scanned >= limit ||
                read_bytes < FPSU_SCAN_CHUNK)) {
            last_report = scanned;
            if (cb(game, scanned, limit, user)) {
                free(buf);
                fclose(f);
                return -1;
            }
        }
        if (read_bytes < FPSU_SCAN_CHUNK) {
            break;
        }
    }

    free(buf);
    fclose(f);
    return patches;
}
