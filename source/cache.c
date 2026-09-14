#include "cache.h"
#include "patch_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return 0;
    }
    return mkdir(path, 0777);
}

static const char *status_name(fpsu_patch_status status)
{
    switch (status) {
    case FPSU_STATUS_KNOWN:
        return "known";
    case FPSU_STATUS_UNTESTED:
        return "untested";
    case FPSU_STATUS_PC_REQUIRED:
        return "pc";
    case FPSU_STATUS_OTHER_VERSION:
        return "other_version";
    case FPSU_STATUS_APPLICABLE:
        return "applicable";
    case FPSU_STATUS_UNAVAILABLE:
        return "unavailable";
    case FPSU_STATUS_NATIVE_60:
        return "native60";
    case FPSU_STATUS_UPDATE_REQUIRED:
        return "update_required";
    default:
        return "not_found";
    }
}

static const char *method_name(fpsu_patch_method method)
{
    switch (method) {
    case FPSU_METHOD_NCL:
        return "ncl";
    case FPSU_METHOD_NCL_CONSTANT:
        return "ncl_constant";
    case FPSU_METHOD_CONFIG:
        return "config";
    case FPSU_METHOD_EBOOT_PC:
        return "eboot_pc";
    case FPSU_METHOD_PATTERN_ONLY:
        return "pattern";
    default:
        return "none";
    }
}

static fpsu_patch_status parse_status(const char *value)
{
    if (!value) {
        return FPSU_STATUS_NOT_FOUND;
    }
    if (strcmp(value, "known") == 0) {
        return FPSU_STATUS_KNOWN;
    }
    if (strcmp(value, "untested") == 0) {
        return FPSU_STATUS_UNTESTED;
    }
    if (strcmp(value, "pc") == 0) {
        return FPSU_STATUS_PC_REQUIRED;
    }
    if (strcmp(value, "other_version") == 0) {
        return FPSU_STATUS_OTHER_VERSION;
    }
    if (strcmp(value, "applicable") == 0) {
        return FPSU_STATUS_APPLICABLE;
    }
    if (strcmp(value, "unavailable") == 0) {
        return FPSU_STATUS_UNAVAILABLE;
    }
    if (strcmp(value, "native60") == 0) {
        return FPSU_STATUS_NATIVE_60;
    }
    if (strcmp(value, "update_required") == 0) {
        return FPSU_STATUS_UPDATE_REQUIRED;
    }
    return FPSU_STATUS_NOT_FOUND;
}

static fpsu_patch_method parse_method(const char *value)
{
    if (!value) {
        return FPSU_METHOD_NONE;
    }
    if (strcmp(value, "ncl") == 0) {
        return FPSU_METHOD_NCL;
    }
    if (strcmp(value, "ncl_constant") == 0 ||
        strcmp(value, "ncl_const") == 0 ||
        strcmp(value, "ncl_cwrite") == 0) {
        return FPSU_METHOD_NCL_CONSTANT;
    }
    if (strcmp(value, "config") == 0) {
        return FPSU_METHOD_CONFIG;
    }
    if (strcmp(value, "eboot_pc") == 0) {
        return FPSU_METHOD_EBOOT_PC;
    }
    if (strcmp(value, "pattern") == 0) {
        return FPSU_METHOD_PATTERN_ONLY;
    }
    return FPSU_METHOD_NONE;
}

static void write_field(FILE *out, const char *value)
{
    const char *p = value ? value : "";
    while (*p) {
        char c = *p++;
        if (c == '|' || c == '\r' || c == '\n') {
            fputc(' ', out);
        } else {
            fputc(c, out);
        }
    }
}

static char *next_field(char **cursor)
{
    char *start;
    char *pipe;
    if (!cursor || !*cursor) {
        return NULL;
    }
    start = *cursor;
    pipe = strchr(start, '|');
    if (pipe) {
        *pipe = '\0';
        *cursor = pipe + 1;
    } else {
        *cursor = NULL;
    }
    return start;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static int parse_cache_line(char *line, fpsu_game_result *result)
{
    char *fields[18];
    char *cursor = line;
    int count = 0;
    int has_category;
    int idx;
    fpsu_game game;

    while (count < 18 && (fields[count] = next_field(&cursor)) != NULL) {
        ++count;
    }
    if (count < 12 || !result) {
        return 0;
    }

    has_category = count >= 13;
    memset(&game, 0, sizeof(game));
    copy_text(game.title_id, sizeof(game.title_id), fields[1]);
    copy_text(game.version, sizeof(game.version), fields[2]);
    copy_text(game.category, sizeof(game.category), has_category ? fields[3] : "");
    copy_text(game.title, sizeof(game.title), has_category ? fields[4] : fields[3]);
    copy_text(game.base_path, sizeof(game.base_path), has_category ? fields[5] : fields[4]);
    copy_text(game.sfo_path, sizeof(game.sfo_path), has_category ? fields[6] : fields[5]);
    copy_text(game.eboot_path, sizeof(game.eboot_path), has_category ? fields[7] : fields[6]);
    game.is_disc_folder = strstr(game.sfo_path, "/PS3_GAME/PARAM.SFO") != NULL;
    game.is_iso = strstr(game.sfo_path, "::/PS3_GAME/PARAM.SFO") != NULL;

    memset(result, 0, sizeof(*result));
    result->game = game;
    patch_db_default_options(result);

    idx = has_category ? 8 : 7;
    result->fps60.kind = FPSU_PATCH_60;
    result->fps60.status = parse_status(fields[idx++]);
    result->fps60.method = parse_method(fields[idx++]);
    result->unlock.kind = FPSU_PATCH_UNLOCK;
    result->unlock.status = parse_status(fields[idx++]);
    result->unlock.method = parse_method(fields[idx++]);
    result->fps30.kind = FPSU_PATCH_30;
    if (idx + 2 < count) {
        result->fps30.status = parse_status(fields[idx++]);
        result->fps30.method = parse_method(fields[idx++]);
    }
    result->pattern_candidate = idx < count && atoi(fields[idx]) ? 1 : 0;
    return game.title_id[0] != '\0';
}

int cache_read_results(fpsu_game_result *results, int max_results)
{
    FILE *in;
    char line[2048];
    int count = 0;

    if (!results || max_results <= 0) {
        return 0;
    }

    in = fopen(FPSU_SCAN_CACHE, "rb");
    if (!in) {
        return 0;
    }

    while (count < max_results && fgets(line, sizeof(line), in)) {
        size_t len = strlen(line);
        if (len == 0) {
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strncmp(line, "# app_version=", 14) == 0) {
            continue;
        }
        if (line[0] == '#') {
            continue;
        }
        if (parse_cache_line(line, &results[count])) {
            ++count;
        }
    }

    fclose(in);
    return count;
}

int cache_write_results(const fpsu_game_result *results, int count)
{
    FILE *out;
    int i;
    unsigned long stamp = (unsigned long)time(NULL);

    if (!results || count < 0) {
        return -1;
    }
    if (ensure_dir(FPSU_DATA_ROOT) != 0) {
        return -1;
    }
    if (ensure_dir(FPSU_CACHE_ROOT) != 0) {
        return -1;
    }

    out = fopen(FPSU_SCAN_CACHE, "wb");
    if (!out) {
        return -1;
    }

    fprintf(out, "# PS3 FPS Unlocker scan cache\n");
    fprintf(out, "# Project by RafJaeger\n");
    fprintf(out, "# app_version=%s\n", FPSU_VERSION_LABEL);
    fprintf(out, "# stamp|title_id|version|category|title|base_path|sfo_path|eboot_path|fps60_status|fps60_method|unlock_status|unlock_method|fps30_status|fps30_method|pattern_candidate\n");

    for (i = 0; i < count; ++i) {
        const fpsu_game_result *r = &results[i];
        fprintf(out, "%lu|", stamp);
        write_field(out, r->game.title_id);
        fputc('|', out);
        write_field(out, r->game.version);
        fputc('|', out);
        write_field(out, r->game.category);
        fputc('|', out);
        write_field(out, r->game.title);
        fputc('|', out);
        write_field(out, r->game.base_path);
        fputc('|', out);
        write_field(out, r->game.sfo_path);
        fputc('|', out);
        write_field(out, r->game.eboot_path);
        fprintf(out, "|%s|%s|%s|%s|%s|%s|%d\n",
            status_name(r->fps60.status),
            method_name(r->fps60.method),
            status_name(r->unlock.status),
            method_name(r->unlock.method),
            status_name(r->fps30.status),
            method_name(r->fps30.method),
            r->pattern_candidate ? 1 : 0);
    }

    fclose(out);
    return 0;
}

static int count_result_status(const fpsu_game_result *results, int count, fpsu_patch_status status)
{
    int i;
    int total = 0;
    if (!results || count <= 0) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (results[i].unlock.status == status ||
            results[i].fps60.status == status ||
            results[i].fps30.status == status) {
            ++total;
        }
    }
    return total;
}

int cache_write_progress(const char *phase, int index, int count, const fpsu_game *game,
    unsigned int scanned, unsigned int limit, const fpsu_game_result *results, int result_count)
{
    FILE *out;
    unsigned long stamp = (unsigned long)time(NULL);

    if (ensure_dir(FPSU_DATA_ROOT) != 0) {
        return -1;
    }
    if (ensure_dir(FPSU_CACHE_ROOT) != 0) {
        return -1;
    }

    out = fopen(FPSU_PROGRESS_CACHE, "wb");
    if (!out) {
        return -1;
    }

    fprintf(out, "stamp=%lu\n", stamp);
    fprintf(out, "app_version=");
    write_field(out, FPSU_VERSION_LABEL);
    fprintf(out, "\n");
    fprintf(out, "phase=");
    write_field(out, phase ? phase : "");
    fprintf(out, "\nindex=%d\ncount=%d\n", index, count);
    fprintf(out, "scanned=%u\nlimit=%u\n", scanned, limit);
    if (game) {
        fprintf(out, "title_id=");
        write_field(out, game->title_id);
        fprintf(out, "\nversion=");
        write_field(out, game->version);
        fprintf(out, "\ncategory=");
        write_field(out, game->category);
        fprintf(out, "\ntitle=");
        write_field(out, game->title);
        fprintf(out, "\neboot=");
        write_field(out, game->eboot_path);
        fprintf(out, "\n");
    }
    fprintf(out, "known=%d\n", count_result_status(results, result_count, FPSU_STATUS_KNOWN));
    fprintf(out, "untested=%d\n", count_result_status(results, result_count, FPSU_STATUS_UNTESTED));
    fprintf(out, "other_version=%d\n", count_result_status(results, result_count, FPSU_STATUS_OTHER_VERSION));
    fprintf(out, "applicable=%d\n", count_result_status(results, result_count, FPSU_STATUS_APPLICABLE));
    fprintf(out, "unavailable=%d\n", count_result_status(results, result_count, FPSU_STATUS_UNAVAILABLE));
    fprintf(out, "pc=%d\n", count_result_status(results, result_count, FPSU_STATUS_PC_REQUIRED));
    fprintf(out, "not_found=%d\n", count_result_status(results, result_count, FPSU_STATUS_NOT_FOUND));

    fclose(out);
    return 0;
}
