#include "patch_db.h"
#include "backup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return 0;
    }
    return mkdir(path, 0777);
}

static int ensure_dir_recursive(const char *path)
{
    char tmp[FPSU_MAX_PATH];
    char *p;

    if (!path || path[0] == '\0') {
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

static int starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void init_option(fpsu_patch_option *opt, fpsu_patch_kind kind)
{
    memset(opt, 0, sizeof(*opt));
    opt->kind = kind;
    opt->status = FPSU_STATUS_NOT_FOUND;
    opt->method = FPSU_METHOD_NONE;
    opt->delay_seconds = FPSU_DEFAULT_PATCH_DELAY_SECONDS;
}

void patch_db_default_options(fpsu_game_result *result)
{
    if (!result) {
        return;
    }
    init_option(&result->fps60, FPSU_PATCH_60);
    init_option(&result->unlock, FPSU_PATCH_UNLOCK);
    init_option(&result->fps30, FPSU_PATCH_30);
    result->pattern_candidate = 0;
}

static fpsu_patch_status parse_status(const char *value)
{
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
    if (strcmp(value, "update_required") == 0) {
        return FPSU_STATUS_UPDATE_REQUIRED;
    }
    return FPSU_STATUS_NOT_FOUND;
}

static fpsu_patch_method parse_method(const char *value)
{
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

static fpsu_patch_kind parse_kind(const char *value)
{
    if (strcmp(value, "60") == 0) {
        return FPSU_PATCH_60;
    }
    if (strcmp(value, "unlock") == 0) {
        return FPSU_PATCH_UNLOCK;
    }
    if (strcmp(value, "30") == 0) {
        return FPSU_PATCH_30;
    }
    return FPSU_PATCH_NONE;
}

static int kind_index(fpsu_patch_kind kind)
{
    switch (kind) {
    case FPSU_PATCH_60:
        return 0;
    case FPSU_PATCH_UNLOCK:
        return 1;
    case FPSU_PATCH_30:
        return 2;
    default:
        return -1;
    }
}

static fpsu_patch_option *option_for_kind(fpsu_game_result *result, fpsu_patch_kind kind)
{
    if (!result) {
        return NULL;
    }
    switch (kind) {
    case FPSU_PATCH_60:
        return &result->fps60;
    case FPSU_PATCH_UNLOCK:
        return &result->unlock;
    case FPSU_PATCH_30:
        return &result->fps30;
    default:
        return NULL;
    }
}

static int status_rank(fpsu_patch_status status)
{
    switch (status) {
    case FPSU_STATUS_KNOWN:
        return 60;
    case FPSU_STATUS_APPLICABLE:
        return 55;
    case FPSU_STATUS_UPDATE_REQUIRED:
        return 50;
    case FPSU_STATUS_OTHER_VERSION:
        return 40;
    case FPSU_STATUS_UNTESTED:
        return 30;
    case FPSU_STATUS_PC_REQUIRED:
        return 25;
    case FPSU_STATUS_NATIVE_60:
        return 20;
    case FPSU_STATUS_UNAVAILABLE:
        return 10;
    default:
        return 0;
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

static int version_is_unknown(const char *version)
{
    return !version || version[0] == '\0' || strcmp(version, "*") == 0 ||
        strcmp(version, "00.00") == 0;
}

static int version_is_safe_file_suffix(const char *version)
{
    return version &&
        isdigit((unsigned char)version[0]) &&
        isdigit((unsigned char)version[1]) &&
        version[2] == '.' &&
        isdigit((unsigned char)version[3]) &&
        isdigit((unsigned char)version[4]) &&
        version[5] == '\0' &&
        strcmp(version, "00.00") != 0;
}

static int version_matches(const char *db_version, const char *game_version)
{
    if (version_is_unknown(db_version)) {
        return 1;
    }
    if (version_is_unknown(game_version)) {
        return 0;
    }
    return strcmp(db_version, game_version) == 0;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void normalize_key(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    const unsigned char *p = (const unsigned char *)src;

    if (!dst || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }

    while (*p && out + 1 < dst_size) {
        if (isalnum(*p)) {
            dst[out++] = (char)toupper(*p);
        }
        ++p;
    }
    dst[out] = '\0';
}

static void source_name_from_note(char *dst, size_t dst_size, const char *note)
{
    const char *marker = "Source file:";
    const char *start;
    const char *end;
    size_t len;

    if (!dst || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (!note) {
        return;
    }

    start = strstr(note, marker);
    if (!start) {
        return;
    }
    start += strlen(marker);
    while (*start == ' ') {
        ++start;
    }
    end = strchr(start, '.');
    if (!end) {
        end = start + strlen(start);
    }
    len = (size_t)(end - start);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, start, len);
    dst[len] = '\0';
}

static int same_game_name(const fpsu_game *game, const char *line_note, const char *line_label)
{
    char game_key[96];
    char note_name[96];
    char note_key[96];
    char label_key[96];
    size_t game_len;
    size_t note_len;
    size_t label_len;

    if (!game || !line_note) {
        return 0;
    }

    normalize_key(game_key, sizeof(game_key), game->title);
    source_name_from_note(note_name, sizeof(note_name), line_note);
    normalize_key(note_key, sizeof(note_key), note_name);
    normalize_key(label_key, sizeof(label_key), line_label);

    game_len = strlen(game_key);
    note_len = strlen(note_key);
    label_len = strlen(label_key);
    if (game_len < 5) {
        return 0;
    }
    if (note_len >= 5 && (strstr(game_key, note_key) || strstr(note_key, game_key))) {
        return 1;
    }
    if (label_len >= 5 && strstr(game_key, label_key)) {
        return 1;
    }
    return 0;
}

static int append_path(char *out, size_t out_size, const char *base, const char *suffix)
{
    size_t base_len;
    size_t suffix_len;

    if (!out || out_size == 0 || !base || !suffix) {
        return -1;
    }

    base_len = strlen(base);
    suffix_len = strlen(suffix);
    if (base_len + suffix_len >= out_size) {
        return -1;
    }

    memcpy(out, base, base_len);
    memcpy(out + base_len, suffix, suffix_len + 1);
    return 0;
}

static int source_priority(const char *source)
{
    if (!source) {
        return 0;
    }
    if (strstr(source, "PS3 real lab") || strstr(source, "Hardware tested")) {
        return 50;
    }
    if (strstr(source, "PSXPlace Confirmed") || strstr(source, "RPCS3toArtemis Working")) {
        return 40;
    }
    if (strstr(source, "Manual")) {
        return 30;
    }
    if (strstr(source, "RPCS3")) {
        return 20;
    }
    return 10;
}

int patch_db_validate_file(const char *path)
{
    FILE *f;
    char line[FPSU_MAX_PAYLOAD + 512];
    int valid = 0;
    int rows = 0;

    if (!path) {
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    while (rows < 4 && fgets(line, sizeof(line), f)) {
        char *p = line;
        int pipes = 0;
        while (*p && (*p == ' ' || *p == '\t')) {
            ++p;
        }
        if (*p == '#' || *p == '\r' || *p == '\n' || *p == '\0') {
            continue;
        }
        while (*p) {
            if (*p++ == '|') {
                ++pipes;
            }
        }
        if (pipes >= 8) {
            valid = 1;
            ++rows;
        } else {
            valid = 0;
            break;
        }
    }
    fclose(f);
    return valid;
}

const char *patch_db_active_path(void)
{
    if (patch_db_validate_file(FPSU_PATCH_DB_UPDATED)) {
        return FPSU_PATCH_DB_UPDATED;
    }
    return FPSU_PATCH_DB_BUNDLED;
}

static const char *graphics_db_active_path(void)
{
    if (patch_db_validate_file(FPSU_GRAPHICS_DB_UPDATED)) {
        return FPSU_GRAPHICS_DB_UPDATED;
    }
    return FPSU_GRAPHICS_DB;
}

static const char *fix_db_active_path(void)
{
    if (patch_db_validate_file(FPSU_FIX_DB_UPDATED)) {
        return FPSU_FIX_DB_UPDATED;
    }
    return FPSU_FIX_DB;
}

static int native60_db_validate_file(const char *path)
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

static const char *native60_db_active_path(void)
{
    if (native60_db_validate_file(FPSU_NATIVE60_DB_UPDATED)) {
        return FPSU_NATIVE60_DB_UPDATED;
    }
    return FPSU_NATIVE60_DB;
}

int patch_db_is_native60(const fpsu_game *game)
{
    FILE *f;
    char line[256];

    if (!game || game->title_id[0] == '\0') {
        return 0;
    }
    f = fopen(native60_db_active_path(), "rb");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        char *pipe;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        pipe = strchr(line, '|');
        if (pipe) {
            *pipe = '\0';
        }
        if (strcmp(line, game->title_id) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int option_priority(fpsu_patch_status status, const char *source)
{
    if (status == FPSU_STATUS_NOT_FOUND) {
        return -1;
    }
    return status_rank(status) * 100 + source_priority(source);
}

static int option_is_installable_ncl(const fpsu_patch_option *option)
{
    return option &&
        option->kind != FPSU_PATCH_NONE &&
        fpsu_method_is_ncl_write(option->method) &&
        option->payload[0] != '\0' &&
        option->status != FPSU_STATUS_NOT_FOUND &&
        option->status != FPSU_STATUS_UNAVAILABLE &&
        option->status != FPSU_STATUS_NATIVE_60;
}

static int payload_contains_write(const char *payload, const char *write)
{
    if (!payload || !write || write[0] == '\0') {
        return 1;
    }
    return strstr(payload, write) != NULL;
}

static int append_payload_write(char *payload, size_t payload_size, const char *write)
{
    size_t len;
    size_t write_len;

    if (!payload || payload_size == 0 || !write || write[0] == '\0') {
        return 0;
    }
    if (payload_contains_write(payload, write)) {
        return 1;
    }
    len = strlen(payload);
    write_len = strlen(write);
    if (len + (len > 0 ? 1 : 0) + write_len >= payload_size) {
        return 0;
    }
    if (len > 0) {
        payload[len++] = ';';
        payload[len] = '\0';
    }
    memcpy(payload + len, write, write_len + 1);
    return 1;
}

static void append_option_note(fpsu_patch_option *option, const char *label, const char *note)
{
    char extra[160];
    size_t len;
    size_t extra_len;

    if (!option) {
        return;
    }
    if ((!label || label[0] == '\0') && (!note || note[0] == '\0')) {
        return;
    }
    snprintf(extra, sizeof(extra), "%s%s%s%s",
        option->note[0] ? "\n" : "",
        label && label[0] ? label : "Fix complementar",
        note && note[0] ? ": " : "",
        note && note[0] ? note : "");
    len = strlen(option->note);
    extra_len = strlen(extra);
    if (len + extra_len >= sizeof(option->note)) {
        if (len < sizeof(option->note) - 32) {
            snprintf(option->note + len, sizeof(option->note) - len, "\n%s",
                label && label[0] ? label : "Fix complementar");
        } else if (label && label[0]) {
            snprintf(option->note, sizeof(option->note), "%s", label);
        }
        return;
    }
    memcpy(option->note + len, extra, extra_len + 1);
}

static void apply_fix_db_to_option(const fpsu_game *game, fpsu_patch_option *option)
{
    FILE *f;
    static char line[FPSU_MAX_PAYLOAD + 512];

    if (!game || !option || option->kind == FPSU_PATCH_NONE || !option_is_installable_ncl(option)) {
        return;
    }

    f = fopen(fix_db_active_path(), "r");
    if (!f) {
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        char *cursor;
        char *title_id;
        char *version;
        char *kind_s;
        char *status_s;
        char *method_s;
        char *label;
        char *source;
        char *payload;
        char *note;
        char *delay_s;
        fpsu_patch_method method;
        int delay;
        (void)status_s;
        (void)source;

        if (len == 0 || line[0] == '#') {
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        cursor = line;
        title_id = next_field(&cursor);
        version = next_field(&cursor);
        kind_s = next_field(&cursor);
        status_s = next_field(&cursor);
        method_s = next_field(&cursor);
        label = next_field(&cursor);
        source = next_field(&cursor);
        payload = next_field(&cursor);
        note = next_field(&cursor);
        delay_s = next_field(&cursor);

        if (!title_id || !version || !kind_s || !method_s || !label || !payload) {
            continue;
        }
        if (strcmp(title_id, game->title_id) != 0 || !version_matches(version, game->version)) {
            continue;
        }
        if (strcmp(kind_s, "*") != 0 && parse_kind(kind_s) != option->kind) {
            continue;
        }

        method = parse_method(method_s);
        if (fpsu_method_is_ncl_write(method) && payload[0] != '\0') {
            char *write_cursor = payload;
            char *token;
            int appended = 0;
            while ((token = write_cursor) != NULL) {
                char *semi = strchr(write_cursor, ';');
                if (semi) {
                    *semi = '\0';
                    write_cursor = semi + 1;
                } else {
                    write_cursor = NULL;
                }
                if (starts_with(token, "0 ") && append_payload_write(option->payload, sizeof(option->payload), token)) {
                    appended = 1;
                }
            }
            if (appended) {
                append_option_note(option, label, note);
            }
        } else {
            append_option_note(option, label, note);
        }

        if (delay_s && delay_s[0]) {
            delay = atoi(delay_s);
            if (delay < 0) {
                delay = 0;
            } else if (delay > 600) {
                delay = 600;
            }
            if (delay > option->delay_seconds) {
                option->delay_seconds = delay;
            }
        }
    }

    fclose(f);
}

static int fill_force_option_from_line(fpsu_patch_option *opt, const fpsu_game *game, char *line)
{
    char *cursor = line;
    char *title_id = next_field(&cursor);
    char *version = next_field(&cursor);
    char *kind_s = next_field(&cursor);
    char *status_s = next_field(&cursor);
    char *method_s = next_field(&cursor);
    char *label = next_field(&cursor);
    char *source = next_field(&cursor);
    char *payload = next_field(&cursor);
    char *note = next_field(&cursor);
    char *delay_s = next_field(&cursor);
    int exact_title;
    int exact_version;
    int same_named_game;
    int delay;

    if (!opt || !game || !title_id || !version || !kind_s || !status_s ||
        !method_s || !label || !source || !payload) {
        return 0;
    }

    exact_title = strcmp(title_id, game->title_id) == 0;
    same_named_game = same_game_name(game, note, label);
    if (!exact_title && !same_named_game) {
        return 0;
    }

    memset(opt, 0, sizeof(*opt));
    opt->kind = parse_kind(kind_s);
    if (opt->kind == FPSU_PATCH_NONE) {
        return 0;
    }
    opt->method = parse_method(method_s);
    opt->delay_seconds = FPSU_DEFAULT_PATCH_DELAY_SECONDS;
    copy_text(opt->label, sizeof(opt->label), label);
    copy_text(opt->source, sizeof(opt->source), source);
    copy_text(opt->payload, sizeof(opt->payload), payload);

    exact_version = exact_title && version_matches(version, game->version);
    if (exact_version) {
        opt->status = parse_status(status_s);
        copy_text(opt->note, sizeof(opt->note), note);
    } else {
        int installable = fpsu_method_is_ncl_write(opt->method) && opt->payload[0] != '\0';
        opt->status = exact_title && !installable ? FPSU_STATUS_UPDATE_REQUIRED : FPSU_STATUS_OTHER_VERSION;
        snprintf(opt->note, sizeof(opt->note),
            "Patch do banco para %s %s. Atual: %s %s. Forcar e NAO SEGURO.\n%s",
            title_id && title_id[0] ? title_id : "desconhecido",
            version && version[0] ? version : "*",
            game->title_id[0] ? game->title_id : "desconhecido",
            game->version[0] ? game->version : "desconhecida",
            note ? note : "");
    }

    if (delay_s && delay_s[0]) {
        delay = atoi(delay_s);
        if (delay < 0) {
            delay = 0;
        } else if (delay > 600) {
            delay = 600;
        }
        opt->delay_seconds = delay;
    }
    return 1;
}

static int same_option_payload(const fpsu_patch_option *a, const fpsu_patch_option *b)
{
    return a && b && a->kind == b->kind && strcmp(a->payload, b->payload) == 0;
}

static int append_collected_option(fpsu_patch_option *options, int count, int max_options,
    const fpsu_patch_option *candidate)
{
    int i;
    if (!options || !candidate || count >= max_options || !option_is_installable_ncl(candidate)) {
        return count;
    }
    for (i = 0; i < count; ++i) {
        if (same_option_payload(&options[i], candidate)) {
            if (option_priority(candidate->status, candidate->source) >
                option_priority(options[i].status, options[i].source)) {
                options[i] = *candidate;
            }
            return count;
        }
    }
    options[count++] = *candidate;
    return count;
}

static void sort_collected_options(fpsu_patch_option *options, int count)
{
    int i;
    int swapped = 1;
    while (swapped) {
        swapped = 0;
        for (i = 0; i + 1 < count; ++i) {
            int left = option_priority(options[i].status, options[i].source);
            int right = option_priority(options[i + 1].status, options[i + 1].source);
            if (right > left) {
                fpsu_patch_option tmp = options[i];
                options[i] = options[i + 1];
                options[i + 1] = tmp;
                swapped = 1;
            }
        }
    }
}

int patch_db_collect_game_options(const fpsu_game *game, fpsu_patch_option *options, int max_options)
{
    FILE *f;
    static char line[FPSU_MAX_PAYLOAD + 512];
    int count = 0;

    if (!game || game->title_id[0] == '\0') {
        return 0;
    }

    f = fopen(patch_db_active_path(), "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        fpsu_patch_option candidate;
        if (len == 0 || line[0] == '#') {
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (fill_force_option_from_line(&candidate, game, line)) {
            apply_fix_db_to_option(game, &candidate);
            count = append_collected_option(options, count, max_options, &candidate);
        }
    }

    fclose(f);
    sort_collected_options(options, count);
    return count;
}

static int graphics_choice_seen(fpsu_patch_option *options, int count, const fpsu_patch_option *candidate)
{
    int i;
    if (!options || !candidate) {
        return 1;
    }
    for (i = 0; i < count; ++i) {
        if (strcmp(options[i].label, candidate->label) == 0 &&
            strcmp(options[i].payload, candidate->payload) == 0) {
            return 1;
        }
    }
    return 0;
}

static int append_graphics_option(fpsu_patch_option *options, int count, int max_options,
    const fpsu_patch_option *candidate)
{
    if (!options || !candidate || count >= max_options ||
        !fpsu_method_is_ncl_write(candidate->method) || candidate->payload[0] == '\0') {
        return count;
    }
    if (graphics_choice_seen(options, count, candidate)) {
        return count;
    }
    options[count++] = *candidate;
    return count;
}

static int fill_graphics_option_from_line(fpsu_patch_option *opt, const fpsu_game *game, char *line)
{
    char *cursor = line;
    char *title_id = next_field(&cursor);
    char *version = next_field(&cursor);
    char *game_name = next_field(&cursor);
    char *label = next_field(&cursor);
    char *status_s = next_field(&cursor);
    char *method_s = next_field(&cursor);
    char *source = next_field(&cursor);
    char *payload = next_field(&cursor);
    char *note = next_field(&cursor);
    char *delay_s = next_field(&cursor);
    int exact_version;

    if (!opt || !game || !title_id || !version || !game_name || !label ||
        !status_s || !method_s || !source || !payload) {
        return 0;
    }
    if (strcmp(title_id, game->title_id) != 0) {
        return 0;
    }

    memset(opt, 0, sizeof(*opt));
    opt->kind = FPSU_PATCH_NONE;
    exact_version = version_matches(version, game->version);
    opt->status = exact_version ? parse_status(status_s) : FPSU_STATUS_OTHER_VERSION;
    opt->method = parse_method(method_s);
    opt->delay_seconds = FPSU_DEFAULT_PATCH_DELAY_SECONDS;
    copy_text(opt->label, sizeof(opt->label), label);
    copy_text(opt->source, sizeof(opt->source), source);
    copy_text(opt->payload, sizeof(opt->payload), payload);
    if (exact_version) {
        copy_text(opt->note, sizeof(opt->note), note);
    } else {
        snprintf(opt->note, sizeof(opt->note),
            "Patch grafico para versao %s. Versao detectada: %s. Teste manual, NAO SEGURO.",
            version && version[0] ? version : "*",
            game->version[0] ? game->version : "desconhecida");
    }
    if (delay_s && delay_s[0]) {
        int delay = atoi(delay_s);
        if (delay < 0) {
            delay = 0;
        } else if (delay > 600) {
            delay = 600;
        }
        opt->delay_seconds = delay;
    }
    return 1;
}

int patch_db_collect_graphics_options(const fpsu_game *game, fpsu_patch_option *options, int max_options)
{
    FILE *f;
    static char line[FPSU_MAX_PAYLOAD + 512];
    int count = 0;

    if (!game || game->title_id[0] == '\0') {
        return 0;
    }

    f = fopen(graphics_db_active_path(), "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        fpsu_patch_option candidate;
        if (len == 0 || line[0] == '#') {
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (!strstr(line, game->title_id)) {
            continue;
        }
        if (fill_graphics_option_from_line(&candidate, game, line)) {
            if (!options || max_options <= 0) {
                if (fpsu_method_is_ncl_write(candidate.method) && candidate.payload[0] != '\0') {
                    ++count;
                }
                continue;
            }
            count = append_graphics_option(options, count, max_options, &candidate);
        }
    }

    fclose(f);
    if (options && max_options > 0) {
        sort_collected_options(options, count);
    }
    return count;
}

static int apply_db_line(fpsu_game_result *result, char *line)
{
    char *cursor = line;
    char *title_id = next_field(&cursor);
    char *version = next_field(&cursor);
    char *kind_s = next_field(&cursor);
    char *status_s = next_field(&cursor);
    char *method_s = next_field(&cursor);
    char *label = next_field(&cursor);
    char *source = next_field(&cursor);
    char *payload = next_field(&cursor);
    char *note = next_field(&cursor);
    char *delay_s = next_field(&cursor);
    fpsu_patch_kind kind;
    fpsu_patch_option *opt;

    if (!title_id || !version || !kind_s || !status_s || !method_s || !label || !payload) {
        return 0;
    }
    if (strcmp(title_id, result->game.title_id) != 0) {
        return 0;
    }
    if (!version_matches(version, result->game.version)) {
        return 0;
    }

    kind = parse_kind(kind_s);
    opt = option_for_kind(result, kind);
    if (!opt) {
        return 0;
    }

    {
        fpsu_patch_status new_status = parse_status(status_s);
        if (option_priority(opt->status, opt->source) > option_priority(new_status, source)) {
            return 0;
        }
        opt->status = new_status;
    }
    opt->kind = kind;
    opt->method = parse_method(method_s);
    copy_text(opt->label, sizeof(opt->label), label);
    copy_text(opt->source, sizeof(opt->source), source);
    copy_text(opt->payload, sizeof(opt->payload), payload);
    copy_text(opt->note, sizeof(opt->note), note);
    if (delay_s && delay_s[0]) {
        int delay = atoi(delay_s);
        if (delay < 0) {
            delay = 0;
        } else if (delay > 600) {
            delay = 600;
        }
        opt->delay_seconds = delay;
    }
    return 1;
}

static int line_summary(char *line, char *title_id, size_t title_id_size,
    char *version, size_t version_size, char *payload, size_t payload_size,
    char *label, size_t label_size, char *note, size_t note_size,
    fpsu_patch_kind *kind, fpsu_patch_method *method)
{
    char *cursor = line;
    char *line_title_id = next_field(&cursor);
    char *line_version = next_field(&cursor);
    char *kind_s = next_field(&cursor);
    char *status_s = next_field(&cursor);
    char *method_s = next_field(&cursor);
    char *line_label = next_field(&cursor);
    char *source = next_field(&cursor);
    char *line_payload = next_field(&cursor);
    char *line_note = next_field(&cursor);
    (void)status_s;
    (void)source;

    if (!line_title_id || !line_version || !kind_s || !method_s ||
        !title_id || !version ||
        !payload || !label || !note || title_id_size == 0 || version_size == 0 ||
        payload_size == 0 || label_size == 0 || note_size == 0 || !kind || !method) {
        return 0;
    }

    *kind = parse_kind(kind_s);
    *method = parse_method(method_s);
    if (*kind == FPSU_PATCH_NONE) {
        return 0;
    }
    copy_text(title_id, title_id_size, line_title_id);
    copy_text(version, version_size, line_version);
    copy_text(payload, payload_size, line_payload);
    copy_text(label, label_size, line_label);
    copy_text(note, note_size, line_note);
    return 1;
}

static void mark_other_version(fpsu_patch_option *opt, fpsu_patch_kind kind, fpsu_patch_method method,
    const fpsu_game *game, const char *compatible_title_id, const char *version,
    const char *payload, const char *label, int same_title)
{
    int can_replace_update_notice;
    if (!opt) {
        return;
    }
    can_replace_update_notice = same_title &&
        opt->status == FPSU_STATUS_UPDATE_REQUIRED &&
        opt->payload[0] == '\0' &&
        payload && payload[0] != '\0' &&
        fpsu_method_is_ncl_write(method);
    if (opt->status != FPSU_STATUS_NOT_FOUND && !can_replace_update_notice) {
        return;
    }
    opt->kind = kind;
    {
        int installable = payload && payload[0] != '\0' && fpsu_method_is_ncl_write(method);
        opt->status = same_title && !installable ? FPSU_STATUS_UPDATE_REQUIRED : FPSU_STATUS_OTHER_VERSION;
    }
    opt->method = method == FPSU_METHOD_NONE && payload && payload[0] ? FPSU_METHOD_NCL : method;
    copy_text(opt->label, sizeof(opt->label),
        label && label[0] ? label : (same_title ? "Update required" : "Patch exists for another version"));
    copy_text(opt->payload, sizeof(opt->payload), payload);
    if (same_title) {
        snprintf(opt->note, sizeof(opt->note),
            "Versao compativel: %s\nVersao atual: %s\nSe a versao foi lida errado em ISO/pasta, use como teste nao seguro.",
            version && version[0] ? version : "desconhecida",
            game && game->version[0] ? game->version : "desconhecida");
    } else {
        snprintf(opt->note, sizeof(opt->note),
            "Sua versao: %s %s\nPatch disponivel para: %s %s\nUse a versao/regiao compativel ou teste como NAO SEGURO.",
            game && game->title_id[0] ? game->title_id : "desconhecida",
            game && game->version[0] ? game->version : "desconhecida",
            compatible_title_id && compatible_title_id[0] ? compatible_title_id : "desconhecida",
            version && version[0] ? version : "desconhecida");
    }
}

int patch_db_match_game(const fpsu_game *game, fpsu_game_result *result)
{
    FILE *f;
    static char line[FPSU_MAX_PAYLOAD + 512];
    int matches = 0;
    int other_seen[3] = {0, 0, 0};
    int cross_seen[3] = {0, 0, 0};
    fpsu_patch_method other_method[3] = {FPSU_METHOD_NONE, FPSU_METHOD_NONE, FPSU_METHOD_NONE};
    fpsu_patch_method cross_method[3] = {FPSU_METHOD_NONE, FPSU_METHOD_NONE, FPSU_METHOD_NONE};
    char other_version[3][16];
    char cross_title[3][16];
    char cross_version[3][16];
    static char other_payload[3][FPSU_MAX_PAYLOAD];
    static char cross_payload[3][FPSU_MAX_PAYLOAD];
    char other_label[3][64];
    char cross_label[3][64];

    if (!game || !result) {
        return 0;
    }

    memset(result, 0, sizeof(*result));
    result->game = *game;
    patch_db_default_options(result);
    memset(other_version, 0, sizeof(other_version));
    memset(cross_title, 0, sizeof(cross_title));
    memset(cross_version, 0, sizeof(cross_version));
    memset(other_payload, 0, sizeof(other_payload));
    memset(cross_payload, 0, sizeof(cross_payload));
    memset(other_label, 0, sizeof(other_label));
    memset(cross_label, 0, sizeof(cross_label));

    f = fopen(patch_db_active_path(), "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[0] == '#') {
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        {
            static char probe[FPSU_MAX_PAYLOAD + 512];
            char line_title[16];
            char line_version[16];
            static char line_payload[FPSU_MAX_PAYLOAD];
            char line_label[64];
            char line_note[160];
            fpsu_patch_kind line_kind;
            fpsu_patch_method line_method;
            snprintf(probe, sizeof(probe), "%s", line);
            if (line_summary(probe, line_title, sizeof(line_title), line_version, sizeof(line_version),
                    line_payload, sizeof(line_payload), line_label, sizeof(line_label),
                    line_note, sizeof(line_note), &line_kind, &line_method)) {
                int idx = kind_index(line_kind);
                if (idx >= 0 && strcmp(line_title, game->title_id) == 0) {
                    int installable = fpsu_method_is_ncl_write(line_method) && line_payload[0] != '\0';
                    int current_installable = fpsu_method_is_ncl_write(other_method[idx]) &&
                        other_payload[idx][0] != '\0';
                    if ((!other_seen[idx] || (!current_installable && installable)) &&
                        !version_matches(line_version, game->version)) {
                        other_seen[idx] = 1;
                        other_method[idx] = line_method;
                        copy_text(other_version[idx], sizeof(other_version[idx]), line_version);
                        copy_text(other_payload[idx], sizeof(other_payload[idx]), line_payload);
                        copy_text(other_label[idx], sizeof(other_label[idx]), line_label);
                    }
                } else if (idx >= 0 && same_game_name(game, line_note, line_label)) {
                    int installable = fpsu_method_is_ncl_write(line_method) && line_payload[0] != '\0';
                    int current_installable = fpsu_method_is_ncl_write(cross_method[idx]) &&
                        cross_payload[idx][0] != '\0';
                    if (!cross_seen[idx] || (!current_installable && installable)) {
                        cross_seen[idx] = 1;
                        cross_method[idx] = line_method;
                        copy_text(cross_title[idx], sizeof(cross_title[idx]), line_title);
                        copy_text(cross_version[idx], sizeof(cross_version[idx]), line_version);
                        copy_text(cross_payload[idx], sizeof(cross_payload[idx]), line_payload);
                        copy_text(cross_label[idx], sizeof(cross_label[idx]), line_label);
                    }
                }
            }
        }
        if (strstr(line, game->title_id) && apply_db_line(result, line)) {
            matches++;
        }
    }

    fclose(f);
    if (other_seen[0]) {
        mark_other_version(&result->fps60, FPSU_PATCH_60, other_method[0],
            game, game->title_id, other_version[0], other_payload[0], other_label[0], 1);
    } else if (cross_seen[0]) {
        mark_other_version(&result->fps60, FPSU_PATCH_60, cross_method[0],
            game, cross_title[0], cross_version[0], cross_payload[0], cross_label[0], 0);
    }
    if (other_seen[1]) {
        mark_other_version(&result->unlock, FPSU_PATCH_UNLOCK, other_method[1],
            game, game->title_id, other_version[1], other_payload[1], other_label[1], 1);
    } else if (cross_seen[1]) {
        mark_other_version(&result->unlock, FPSU_PATCH_UNLOCK, cross_method[1],
            game, cross_title[1], cross_version[1], cross_payload[1], cross_label[1], 0);
    }
    if (other_seen[2]) {
        mark_other_version(&result->fps30, FPSU_PATCH_30, other_method[2],
            game, game->title_id, other_version[2], other_payload[2], other_label[2], 1);
    } else if (cross_seen[2]) {
        mark_other_version(&result->fps30, FPSU_PATCH_30, cross_method[2],
            game, cross_title[2], cross_version[2], cross_payload[2], cross_label[2], 0);
    }
    apply_fix_db_to_option(game, &result->fps60);
    apply_fix_db_to_option(game, &result->unlock);
    apply_fix_db_to_option(game, &result->fps30);
    return matches;
}

static int write_ncl_file(FILE *out, const fpsu_patch_option *option)
{
    char payload[FPSU_MAX_PAYLOAD];
    char *cursor;
    char *token;

    fprintf(out, "%s\n", option->label[0] ? option->label : "FPS Unlock");
    fprintf(out, "%d\n", option->method == FPSU_METHOD_NCL_CONSTANT ? 1 : 0);
    fprintf(out, "%s\n", option->source[0] ? option->source : "PS3 FPS Unlocker");

    copy_text(payload, sizeof(payload), option->payload);
    cursor = payload;
    while ((token = cursor) != NULL) {
        char *semi = strchr(cursor, ';');
        if (semi) {
            *semi = '\0';
            cursor = semi + 1;
        } else {
            cursor = NULL;
        }
        if (starts_with(token, "0 ")) {
            fprintf(out, "%s\n", token);
        }
    }
    fprintf(out, "#\n");
    return 0;
}

static int write_ncl_path(const fpsu_game *game, const fpsu_patch_option *option,
    const char *final_path, char *out_path, size_t out_path_size)
{
    char backup_path[FPSU_MAX_PATH];
    FILE *out;

    if (!game || !option || !final_path || !out_path || out_path_size == 0) {
        return -1;
    }

    if (backup_path_is_safe_target(final_path)) {
        FILE *existing = fopen(final_path, "rb");
        if (existing) {
            fclose(existing);
            if (backup_copy_file(game->title_id, final_path, backup_path, sizeof(backup_path)) != 0) {
                return -1;
            }
        }
    }

    out = fopen(final_path, "wb");
    if (!out) {
        return -1;
    }
    write_ncl_file(out, option);
    fclose(out);

    snprintf(out_path, out_path_size, "%s", final_path);
    return 0;
}

static int write_ncl_at_root(const fpsu_game *game, const fpsu_patch_option *option,
    const char *root, char *out_path, size_t out_path_size)
{
    char userlist[FPSU_MAX_PATH];
    char final_path[FPSU_MAX_PATH];

    if (!game || !option || !root || !out_path || out_path_size == 0) {
        return -1;
    }

    snprintf(userlist, sizeof(userlist), "%s/USERLIST", root);
    if (ensure_dir_recursive(userlist) != 0) {
        return -1;
    }

    snprintf(final_path, sizeof(final_path), "%s/%s_FPSU.ncl", userlist, game->title_id);
    return write_ncl_path(game, option, final_path, out_path, out_path_size);
}

static int eboot_game_root(const fpsu_game *game, char *root, size_t root_size)
{
    static const char *suffixes[] = {
        "/PS3_GAME/USRDIR/EBOOT.BIN",
        "/USRDIR/EBOOT.BIN"
    };
    size_t len;
    size_t i;

    if (!game || !root || root_size == 0 || game->eboot_path[0] == '\0') {
        return -1;
    }

    len = strlen(game->eboot_path);
    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        size_t suffix_len = strlen(suffixes[i]);
        if (len > suffix_len && strcmp(game->eboot_path + len - suffix_len, suffixes[i]) == 0) {
            size_t root_len = len - suffix_len;
            if (root_len >= root_size) {
                return -1;
            }
            memcpy(root, game->eboot_path, root_len);
            root[root_len] = '\0';
            return 0;
        }
    }
    return -1;
}

static int write_ncl_tmp_artemis(const fpsu_game *game, const fpsu_patch_option *option,
    int with_version, char *out_path, size_t out_path_size)
{
    char file_name[64];
    char final_path[FPSU_MAX_PATH];

    if (!game || !option || game->title_id[0] == '\0' || !out_path || out_path_size == 0) {
        return -1;
    }

    if (ensure_dir_recursive("/dev_hdd0/tmp/artemis") != 0) {
        return -1;
    }

    if (with_version && version_is_safe_file_suffix(game->version)) {
        snprintf(file_name, sizeof(file_name), "%s_%s.ncl", game->title_id, game->version);
    } else {
        snprintf(file_name, sizeof(file_name), "%s.ncl", game->title_id);
    }

    if (append_path(final_path, sizeof(final_path), "/dev_hdd0/tmp/artemis/", file_name) != 0) {
        return -1;
    }
    return write_ncl_path(game, option, final_path, out_path, out_path_size);
}

static const char *path_tail(const char *path)
{
    const char *slash;

    if (!path || path[0] == '\0') {
        return "";
    }
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int strip_iso_split_suffix_text(char *text)
{
    size_t len;
    size_t digits_start;

    if (!text) {
        return 0;
    }
    len = strlen(text);
    digits_start = len;
    while (digits_start > 0 && isdigit((unsigned char)text[digits_start - 1])) {
        --digits_start;
    }
    if (digits_start == len || digits_start < 6 || text[digits_start - 1] != '.') {
        return 0;
    }
    if (text[digits_start - 5] != '.' ||
        tolower((unsigned char)text[digits_start - 4]) != 'i' ||
        tolower((unsigned char)text[digits_start - 3]) != 's' ||
        tolower((unsigned char)text[digits_start - 2]) != 'o') {
        return 0;
    }
    text[digits_start - 1] = '\0';
    return 1;
}

static int strip_iso_suffix_text(char *text)
{
    size_t len;

    if (!text) {
        return 0;
    }
    len = strlen(text);
    if (len < 4 ||
        text[len - 4] != '.' ||
        tolower((unsigned char)text[len - 3]) != 'i' ||
        tolower((unsigned char)text[len - 2]) != 's' ||
        tolower((unsigned char)text[len - 1]) != 'o') {
        return 0;
    }
    text[len - 4] = '\0';
    return 1;
}

static void strip_leading_zeroes(char *value)
{
    char *p;

    if (!value) {
        return;
    }

    p = value;
    while (p[0] == '0' && p[1] != '\0') {
        ++p;
    }
    if (p != value) {
        memmove(value, p, strlen(p) + 1);
    }
}

static void normalize_webman_patch_addr(const char *input, char *short_addr, size_t short_size,
    char *prefixed_addr, size_t prefixed_size)
{
    char clean[24];
    size_t i;
    size_t out = 0;

    if (!input || !short_addr || short_size == 0 || !prefixed_addr || prefixed_size == 0) {
        return;
    }

    short_addr[0] = '\0';
    prefixed_addr[0] = '\0';

    if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
        input += 2;
    }

    for (i = 0; input[i] != '\0' && out + 1 < sizeof(clean); ++i) {
        if (isxdigit((unsigned char)input[i])) {
            clean[out++] = (char)toupper((unsigned char)input[i]);
        }
    }
    clean[out] = '\0';

    if (clean[0] == '\0') {
        return;
    }

    copy_text(short_addr, short_size, clean);
    strip_leading_zeroes(short_addr);
    snprintf(prefixed_addr, prefixed_size, "0x%s", clean);
}

static int script_write_direct_setmem(FILE *out, const fpsu_patch_option *option)
{
    char payload[FPSU_MAX_PAYLOAD];
    char *cursor;
    char *token;
    int written = 0;

    if (!out || !option) {
        return 0;
    }

    copy_text(payload, sizeof(payload), option->payload);
    cursor = payload;
    while ((token = cursor) != NULL) {
        char *semi = strchr(cursor, ';');
        char addr[24];
        char value[520];

        if (semi) {
            *semi = '\0';
            cursor = semi + 1;
        } else {
            cursor = NULL;
        }

        addr[0] = '\0';
        value[0] = '\0';
        if (sscanf(token, "0 %23s %519s", addr, value) == 2) {
            char short_addr[24];
            char prefixed_addr[28];

            normalize_webman_patch_addr(addr, short_addr, sizeof(short_addr),
                prefixed_addr, sizeof(prefixed_addr));
            if (short_addr[0] == '\0' || prefixed_addr[0] == '\0') {
                continue;
            }

            fprintf(out, "log FPSU write setmem addr=%s val=%s\n", short_addr, value);
            fprintf(out, "/setmem.ps3mapi?addr=%s&val=%s\n", short_addr, value);
            fprintf(out, "log FPSU write patch alias addr=%s val=%s\n", prefixed_addr, value);
            fprintf(out, "/patch.ps3?addr=%s&val=%s\n", prefixed_addr, value);
            ++written;
        }
    }

    return written;
}

static void script_write_wait_seconds(FILE *out, int seconds)
{
    if (!out || seconds <= 0) {
        return;
    }
    while (seconds > 0) {
        int chunk = seconds > 9 ? 9 : seconds;
        fprintf(out, "wait %d\n", chunk);
        seconds -= chunk;
    }
}

static int write_ingame_script_for_id(const fpsu_game *game, const char *trigger_id,
    const fpsu_patch_option *option, char *out_path, size_t out_path_size)
{
    char final_path[FPSU_MAX_PATH];
    FILE *out;
    int delay_seconds;
    int direct_lines;

    if (!game || !trigger_id || trigger_id[0] == '\0' || !option || !out_path || out_path_size == 0) {
        return -1;
    }

    if (ensure_dir_recursive(FPSU_CACHE_ROOT) != 0) {
        return -1;
    }

    if (ensure_dir_recursive("/dev_hdd0/tmp/wm_ingame") != 0) {
        return -1;
    }

    snprintf(final_path, sizeof(final_path), "/dev_hdd0/tmp/wm_ingame/%s.bat", trigger_id);
    out = fopen(final_path, "wb");
    if (!out) {
        return -1;
    }

    delay_seconds = option->delay_seconds;
    if (delay_seconds < 0) {
        delay_seconds = 0;
    } else if (delay_seconds > 600) {
        delay_seconds = 600;
    }

    fprintf(out, "# PS3 FPS Unlocker auto attach\n");
    fprintf(out, "logfile " FPSU_CACHE_ROOT "/webman_ingame.log\n");
    fprintf(out, "log FPSU -------- inicio --------\n");
    fprintf(out, "log FPSU %s auto attach title=%s version=%s kind=%d method=%d label=%s\n",
        trigger_id, game->title_id, game->version, option->kind, option->method, option->label);
    fprintf(out, "log FPSU build=%s trigger=%s base=%s sfo=%s eboot=%s iso=%d disc=%d\n",
        FPSU_VERSION_LABEL, trigger_id, game->base_path, game->sfo_path, game->eboot_path,
        game->is_iso, game->is_disc_folder);
    fprintf(out, "log FPSU payload=%s\n", option->payload);
    fprintf(out, "log FPSU log path " FPSU_CACHE_ROOT "/webman_ingame.log\n");
#if FPSU_HEN_DIAG_BUILD
    fprintf(out, "log FPSU se nao aparecer popup no jogo: script webMAN nao disparou\n");
    fprintf(out, "log FPSU se aparecer popup mas FPS nao mudar: suspeita=attach/memoria/patch\n");
    fprintf(out, "popup PSUF: diagnostico iniciou. Aguarde 2 min.\n");
#else
    fprintf(out, "popup PSUF: patch pronto. Aguarde 2 min.\n");
#endif
    fprintf(out, "del /dev_hdd0/tmp/art.txt\n");
    fprintf(out, "del /dev_hdd0/tmp/art.log\n");
    fprintf(out, "log FPSU tentando iniciar PS3MAPI pelo webMAN\n");
    fprintf(out, "/netstatus.ps3?start-ps3mapi\n");
    if (version_is_safe_file_suffix(game->version)) {
        fprintf(out, "log FPSU copiando NCL: /dev_hdd0/tmp/artemis/%s_%s.ncl -> /dev_hdd0/tmp/art.txt\n",
            game->title_id, game->version);
        fprintf(out, "fcopy /dev_hdd0/tmp/artemis/%s_%s.ncl=/dev_hdd0/tmp/art.txt\n",
            game->title_id, game->version);
    } else {
        fprintf(out, "log FPSU copiando NCL: /dev_hdd0/tmp/artemis/%s.ncl -> /dev_hdd0/tmp/art.txt\n",
            game->title_id);
        fprintf(out, "fcopy /dev_hdd0/tmp/artemis/%s.ncl=/dev_hdd0/tmp/art.txt\n",
            game->title_id);
    }
#if FPSU_HEN_DIAG_BUILD
    fprintf(out, "popup PSUF: patch preparado. Espere carregar.\n");
#endif
    script_write_wait_seconds(out, delay_seconds);
#if FPSU_HEN_DIAG_BUILD
    fprintf(out, "popup PSUF: tentando aplicar agora.\n");
#else
    fprintf(out, "popup PSUF: aplicando patch.\n");
#endif
    fprintf(out, "/ps3mapi.ps3?PROCESS%%20GETCURRENTPID\n");
    fprintf(out, "wait 1\n");
    direct_lines = script_write_direct_setmem(out, option);
    if (direct_lines > 0) {
        fprintf(out, "log FPSU %s PS3MAPI direct setmem round 1\n", trigger_id);
    }
    fprintf(out, "log FPSU tentando Artemis attach 1\n");
    fprintf(out, "/artemis.ps3?attach\n");
    fprintf(out, "wait 4\n");
    if (script_write_direct_setmem(out, option) > 0) {
        fprintf(out, "log FPSU %s PS3MAPI direct setmem round 2\n", trigger_id);
    }
    fprintf(out, "wait 2\n");
    fprintf(out, "log FPSU tentando Artemis attach 2\n");
    fprintf(out, "/artemis.ps3?attach\n");
    fprintf(out, "wait 2\n");
    if (script_write_direct_setmem(out, option) > 0) {
        fprintf(out, "log FPSU %s PS3MAPI direct setmem round 3\n", trigger_id);
    }
#if FPSU_HEN_DIAG_BUILD
    fprintf(out, "popup PSUF: se nao mudou, aperte START no jogo para forcar Artemis.\n");
    fprintf(out, "log FPSU diagnostico: mantendo /dev_hdd0/tmp/art.txt para forcar com START pelo Artemis\n");
    fprintf(out, "log FPSU linhas diretas escritas=%d\n", direct_lines);
#else
    fprintf(out, "log FPSU limpando art.txt temporario; mantendo NCL/script para reaplicar depois\n");
    fprintf(out, "del /dev_hdd0/tmp/art.txt\n");
    fprintf(out, "del /dev_hdd0/tmp/art.log\n");
#endif
    fprintf(out, "log FPSU %s finished; send " FPSU_CACHE_ROOT "/webman_ingame.log for support\n",
        trigger_id);
    fprintf(out, "log FPSU se apareceu esta linha e nao mudou FPS, o script rodou; investigar attach, endereco ou metodo\n");
    fprintf(out, "log FPSU -------- fim --------\n");
#if FPSU_HEN_DIAG_BUILD
    fprintf(out, "popup PSUF: mande " FPSU_CACHE_ROOT "/webman_ingame.log\n");
#endif
    fclose(out);

    copy_text(out_path, out_path_size, final_path);
    return 0;
}

static void remember_script_variant(const fpsu_game *game, const fpsu_patch_option *option,
    const char *trigger_id, char *script_path, size_t script_path_size,
    char *out_path, size_t out_path_size, int *ok)
{
    if (!trigger_id || trigger_id[0] == '\0' || strchr(trigger_id, '/') || strchr(trigger_id, '\\')) {
        return;
    }
    if (write_ingame_script_for_id(game, trigger_id, option, script_path, script_path_size) == 0) {
        if (!*ok) {
            copy_text(out_path, out_path_size, script_path);
        }
        *ok = 1;
    }
}

static int write_ingame_scripts(const fpsu_game *game, const fpsu_patch_option *option,
    char *out_path, size_t out_path_size)
{
    char script_path[FPSU_MAX_PATH];
    char trigger[FPSU_MAX_PATH];
    const char *base_id;
    int ok = 0;

    if (!game || !out_path || out_path_size == 0) {
        return -1;
    }

    script_path[0] = '\0';
    remember_script_variant(game, option, game->title_id, script_path, sizeof(script_path),
        out_path, out_path_size, &ok);
    remember_script_variant(game, option, game->title, script_path, sizeof(script_path),
        out_path, out_path_size, &ok);

    base_id = path_tail(game->base_path);
    remember_script_variant(game, option, base_id, script_path, sizeof(script_path),
        out_path, out_path_size, &ok);

    copy_text(trigger, sizeof(trigger), base_id);
    if (strip_iso_split_suffix_text(trigger)) {
        remember_script_variant(game, option, trigger, script_path, sizeof(script_path),
            out_path, out_path_size, &ok);
    }

    copy_text(trigger, sizeof(trigger), base_id);
    if (strip_iso_suffix_text(trigger)) {
        remember_script_variant(game, option, trigger, script_path, sizeof(script_path),
            out_path, out_path_size, &ok);
    }

    return ok ? 0 : -1;
}

static void remember_written_path(int ok, char *preferred_path, size_t preferred_path_size,
    const char *candidate_path, int *written_count)
{
    if (!ok || !preferred_path || preferred_path_size == 0 || !candidate_path || !written_count) {
        return;
    }

    if (*written_count == 0) {
        copy_text(preferred_path, preferred_path_size, candidate_path);
    }
    ++(*written_count);
}

#if FPSU_HEN_DIAG_BUILD
static int copy_file_simple(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    char buf[4096];
    size_t n;

    if (!src || !dst || src[0] == '\0' || dst[0] == '\0') {
        return -1;
    }

    in = fopen(src, "rb");
    if (!in) {
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static int path_exists_simple(const char *path)
{
    struct stat st;
    return path && path[0] != '\0' && stat(path, &st) == 0;
}

static int write_global_ingame_test_script(const char *script_path)
{
    FILE *out;

    if (!script_path || script_path[0] == '\0') {
        return -1;
    }
    if (ensure_dir_recursive(FPSU_CACHE_ROOT) != 0) {
        return -1;
    }

    if (path_exists_simple("/dev_hdd0/ingame.bat") &&
        copy_file_simple("/dev_hdd0/ingame.bat", FPSU_CACHE_ROOT "/ingame_before_psuf.bak") == 0) {
        out = fopen(FPSU_CACHE_ROOT "/global_ingame_note.txt", "wb");
        if (out) {
            fprintf(out, "PSUF HEN test backed up /dev_hdd0/ingame.bat here:\n");
            fprintf(out, FPSU_CACHE_ROOT "/ingame_before_psuf.bak\n");
            fclose(out);
        }
    }

    if (copy_file_simple(script_path, "/dev_hdd0/ingame.bat") != 0) {
        return -1;
    }
    return 0;
}
#endif

static void append_apply_setup_log(const fpsu_game *game, const fpsu_patch_option *option,
    const char *tmp_version_path, const char *tmp_title_path, const char *art_txt_path,
    const char *ingame_path, const char *fallback_path, int written_count)
{
    FILE *log;

    if (ensure_dir_recursive(FPSU_CACHE_ROOT) != 0) {
        return;
    }

    log = fopen(FPSU_CACHE_ROOT "/last_apply_setup.log", "ab");
    if (!log) {
        return;
    }

    fprintf(log, "---- PSUF apply setup ----\n");
    fprintf(log, "build=%s\n", FPSU_VERSION_LABEL);
    fprintf(log, "title_id=%s version=%s title=%s\n",
        game ? game->title_id : "", game ? game->version : "", game ? game->title : "");
    fprintf(log, "base=%s\nsfo=%s\neboot=%s\niso=%d disc=%d\n",
        game ? game->base_path : "", game ? game->sfo_path : "", game ? game->eboot_path : "",
        game ? game->is_iso : 0, game ? game->is_disc_folder : 0);
    fprintf(log, "option=%s kind=%d status=%d method=%d delay=%d source=%s\n",
        option && option->label[0] ? option->label : "",
        option ? option->kind : 0, option ? option->status : 0,
        option ? option->method : 0, option ? option->delay_seconds : 0,
        option && option->source[0] ? option->source : "");
    fprintf(log, "payload=%s\n", option ? option->payload : "");
    fprintf(log, "written_count=%d\n", written_count);
    fprintf(log, "tmp_version=%s\n", tmp_version_path ? tmp_version_path : "");
    fprintf(log, "tmp_title=%s\n", tmp_title_path ? tmp_title_path : "");
    fprintf(log, "art_txt=%s\n", art_txt_path ? art_txt_path : "");
    fprintf(log, "ingame=%s\n", ingame_path ? ingame_path : "");
#if FPSU_HEN_DIAG_BUILD
    fprintf(log, "global_ingame=/dev_hdd0/ingame.bat\n");
    fprintf(log, "global_ingame_backup=" FPSU_CACHE_ROOT "/ingame_before_psuf.bak\n");
#endif
    fprintf(log, "fallback=%s\n", fallback_path ? fallback_path : "");
    fprintf(log, "if webman_ingame.log is missing, webMAN did not run the ingame script.\n");
#if FPSU_HEN_DIAG_BUILD
    fprintf(log, "for HEN test: open the game and press START when Artemis asks to attach.\n");
#else
    fprintf(log, "runtime scripts are kept so webMAN can reapply the selected patch later.\n");
#endif
    fclose(log);
}

int patch_db_write_ncl(const fpsu_game *game, const fpsu_patch_option *option, char *out_path, size_t out_path_size)
{
    char tmp_version_path[FPSU_MAX_PATH];
    char tmp_title_path[FPSU_MAX_PATH];
    char art_txt_path[FPSU_MAX_PATH];
    char ingame_path[FPSU_MAX_PATH];
    char fallback_path[FPSU_MAX_PATH];
    int written_count = 0;

    if (!game || !option || !fpsu_method_is_ncl_write(option->method) || !out_path || out_path_size == 0) {
        return -1;
    }

    tmp_version_path[0] = '\0';
    tmp_title_path[0] = '\0';
    art_txt_path[0] = '\0';
    ingame_path[0] = '\0';
    fallback_path[0] = '\0';
    out_path[0] = '\0';

    remove("/dev_hdd0/tmp/art.txt");
    remove("/dev_hdd0/tmp/art.log");

    remember_written_path(write_ncl_tmp_artemis(game, option, 1,
            tmp_version_path, sizeof(tmp_version_path)) == 0,
        out_path, out_path_size, tmp_version_path, &written_count);
    remember_written_path(write_ncl_tmp_artemis(game, option, 0,
            tmp_title_path, sizeof(tmp_title_path)) == 0,
        out_path, out_path_size, tmp_title_path, &written_count);
#if FPSU_HEN_DIAG_BUILD
    remember_written_path(write_ncl_path(game, option, "/dev_hdd0/tmp/art.txt",
            art_txt_path, sizeof(art_txt_path)) == 0,
        out_path, out_path_size, art_txt_path, &written_count);
#endif
    remember_written_path(write_ingame_scripts(game, option,
            ingame_path, sizeof(ingame_path)) == 0,
        out_path, out_path_size, ingame_path, &written_count);
#if FPSU_HEN_DIAG_BUILD
    remember_written_path(write_global_ingame_test_script(ingame_path) == 0,
        out_path, out_path_size, "/dev_hdd0/ingame.bat", &written_count);
#endif
    remember_written_path(write_ncl_at_root(game, option, FPSU_DATA_ROOT "/patches",
            fallback_path, sizeof(fallback_path)) == 0,
        out_path, out_path_size, fallback_path, &written_count);

    append_apply_setup_log(game, option, tmp_version_path, tmp_title_path,
        art_txt_path, ingame_path, fallback_path, written_count);

    if (written_count > 0) {
        return 0;
    }
    return -1;
}

static int remove_if_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return remove(path) == 0 ? 1 : 0;
}

int patch_db_cleanup_runtime_artifacts(void)
{
    int removed = 0;

    removed += remove_if_exists("/dev_hdd0/tmp/art.txt");
    removed += remove_if_exists("/dev_hdd0/tmp/art.log");
    return removed;
}

static int remove_userlist_entry(const fpsu_game *game, const char *root)
{
    char final_path[FPSU_MAX_PATH];

    if (!game || !root || game->title_id[0] == '\0') {
        return 0;
    }
    snprintf(final_path, sizeof(final_path), "%s/USERLIST/%s_FPSU.ncl", root, game->title_id);
    return remove_if_exists(final_path);
}

static int remove_ingame_script(const char *trigger_id)
{
    char final_path[FPSU_MAX_PATH];

    if (!trigger_id || trigger_id[0] == '\0') {
        return 0;
    }
    snprintf(final_path, sizeof(final_path), "/dev_hdd0/tmp/wm_ingame/%s.bat", trigger_id);
    return remove_if_exists(final_path);
}

static int remove_ingame_script_variants(const fpsu_game *game, const char *trigger_id)
{
    char variant[FPSU_MAX_PATH];
    int removed = 0;

    if (!trigger_id || trigger_id[0] == '\0') {
        return 0;
    }

    removed += remove_ingame_script(trigger_id);
    copy_text(variant, sizeof(variant), trigger_id);
    if (strip_iso_split_suffix_text(variant)) {
        removed += remove_ingame_script(variant);
    }
    if (strip_iso_suffix_text(variant)) {
        removed += remove_ingame_script(variant);
    }
    if (game && game->title[0] != '\0') {
        removed += remove_ingame_script(game->title);
    }
    return removed;
}

int patch_db_remove_generated(const fpsu_game *game)
{
    char eboot_root[FPSU_MAX_PATH];
    char title_root[FPSU_MAX_PATH];
    char path[FPSU_MAX_PATH];
    const char *base_id;
    int removed = 0;

    if (!game) {
        return 0;
    }

    eboot_root[0] = '\0';
    title_root[0] = '\0';

    if (game->base_path[0] != '\0') {
        if (append_path(path, sizeof(path), game->base_path, ".ncl") == 0) {
            removed += remove_if_exists(path);
        }
        base_id = path_tail(game->base_path);
        removed += remove_ingame_script_variants(game, base_id);
    }

    if (eboot_game_root(game, eboot_root, sizeof(eboot_root)) == 0) {
        if (append_path(path, sizeof(path), eboot_root, ".ncl") == 0) {
            removed += remove_if_exists(path);
        }
        if (append_path(path, sizeof(path), eboot_root, "/artemis.ncl") == 0) {
            removed += remove_if_exists(path);
        }
        removed += remove_ingame_script_variants(game, path_tail(eboot_root));
    }

    if (game->title_id[0] != '\0') {
        if (append_path(title_root, sizeof(title_root), "/dev_hdd0/game/", game->title_id) == 0) {
            if (append_path(path, sizeof(path), title_root, "/artemis.ncl") == 0) {
                removed += remove_if_exists(path);
            }
        }
        if (version_is_safe_file_suffix(game->version)) {
            snprintf(path, sizeof(path), "/dev_hdd0/tmp/artemis/%s_%s.ncl",
                game->title_id, game->version);
            removed += remove_if_exists(path);
        }
        snprintf(path, sizeof(path), "/dev_hdd0/tmp/artemis/%s.ncl", game->title_id);
        removed += remove_if_exists(path);
        removed += remove_ingame_script_variants(game, game->title_id);
        removed += remove_userlist_entry(game, "/dev_hdd0/game/ARTZ00001/USRDIR");
        removed += remove_userlist_entry(game, FPSU_DATA_ROOT "/patches");
    }

    return removed;
}
