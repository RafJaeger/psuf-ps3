#include "backup.h"
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
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

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int backup_path_is_safe_target(const char *path)
{
    if (!path) {
        return 0;
    }
    if (strncmp(path, "/dev_flash", 10) == 0 || strncmp(path, "/dev_blind", 10) == 0) {
        return 0;
    }
    if (strncmp(path, "/dev_hdd0/game/", 15) == 0 ||
        strncmp(path, "/dev_hdd0/GAMES/", 16) == 0 ||
        strncmp(path, "/dev_usb", 8) == 0) {
        return 1;
    }
    return 0;
}

static int copy_stream(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    static unsigned char buffer[8192];
    size_t n;

    in = fopen(src, "rb");
    if (!in) {
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            fclose(out);
            fclose(in);
            return -1;
        }
    }

    fclose(out);
    fclose(in);
    return 0;
}

int backup_copy_file(const char *title_id, const char *source_path, char *backup_path, size_t backup_path_size)
{
    char title_dir[FPSU_MAX_PATH];
    char manifest[FPSU_MAX_PATH];
    FILE *m;
    unsigned long stamp = (unsigned long)time(NULL);

    if (!title_id || !source_path || !backup_path || backup_path_size == 0) {
        return -1;
    }
    if (!backup_path_is_safe_target(source_path)) {
        return -1;
    }
    if (ensure_dir(FPSU_DATA_ROOT) != 0) {
        return -1;
    }
    if (ensure_dir(FPSU_BACKUP_ROOT) != 0) {
        return -1;
    }

    snprintf(title_dir, sizeof(title_dir), "%s/%s", FPSU_BACKUP_ROOT, title_id);
    if (ensure_dir(title_dir) != 0) {
        return -1;
    }

    snprintf(backup_path, backup_path_size, "%s/%lu_%s.bak", title_dir, stamp, base_name(source_path));
    if (copy_stream(source_path, backup_path) != 0) {
        return -1;
    }

    snprintf(manifest, sizeof(manifest), "%s/manifest.txt", title_dir);
    m = fopen(manifest, "a");
    if (m) {
        fprintf(m, "%lu|%s|%s\n", stamp, source_path, backup_path);
        fclose(m);
    }
    return 0;
}

int backup_restore_file(const char *backup_path, const char *target_path)
{
    if (!backup_path || !target_path || !backup_path_is_safe_target(target_path)) {
        return -1;
    }
    return copy_stream(backup_path, target_path);
}

static int target_already_restored(char targets[][FPSU_MAX_PATH], int count, const char *target_path)
{
    int i;

    for (i = 0; i < count; ++i) {
        if (strcmp(targets[i], target_path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int read_manifest_latest(const char *manifest_path, char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size)
{
    FILE *m;
    char line[FPSU_MAX_PATH * 2];
    unsigned long best_stamp = 0;
    int found = 0;

    m = fopen(manifest_path, "r");
    if (!m) {
        return -1;
    }

    while (fgets(line, sizeof(line), m)) {
        char *p1;
        char *p2;
        char *end;
        unsigned long stamp;
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        p1 = strchr(line, '|');
        if (!p1) {
            continue;
        }
        *p1++ = '\0';
        p2 = strchr(p1, '|');
        if (!p2) {
            continue;
        }
        *p2++ = '\0';

        stamp = strtoul(line, &end, 10);
        if (end == line || stamp < best_stamp) {
            continue;
        }
        if (!backup_path_is_safe_target(p1)) {
            continue;
        }

        best_stamp = stamp;
        snprintf(target_path, target_path_size, "%s", p1);
        snprintf(backup_path, backup_path_size, "%s", p2);
        found = 1;
    }

    fclose(m);
    return found ? 0 : -1;
}

static int restore_manifest_first_backups(const char *manifest_path, int *restored_count)
{
    FILE *m;
    char line[FPSU_MAX_PATH * 2];
    char (*restored_targets)[FPSU_MAX_PATH];
    int restored = 0;
    int tracked = 0;
    const int max_targets = 32;

    if (!manifest_path || !restored_count) {
        return -1;
    }

    m = fopen(manifest_path, "r");
    if (!m) {
        return -1;
    }

    restored_targets = (char (*)[FPSU_MAX_PATH])calloc(max_targets, FPSU_MAX_PATH);
    if (!restored_targets) {
        fclose(m);
        return -1;
    }

    while (fgets(line, sizeof(line), m)) {
        char *p1;
        char *p2;
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        p1 = strchr(line, '|');
        if (!p1) {
            continue;
        }
        *p1++ = '\0';
        p2 = strchr(p1, '|');
        if (!p2) {
            continue;
        }
        *p2++ = '\0';

        if (!backup_path_is_safe_target(p1) || target_already_restored(restored_targets, tracked, p1)) {
            continue;
        }
        if (backup_restore_file(p2, p1) == 0) {
            if (tracked < max_targets) {
                snprintf(restored_targets[tracked], FPSU_MAX_PATH, "%s", p1);
                ++tracked;
            }
            ++restored;
        }
    }

    free(restored_targets);
    fclose(m);
    *restored_count += restored;
    return restored > 0 ? 0 : -1;
}

static int find_latest_for_title_in_root(const char *root_path, const char *title_id,
    char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size)
{
    char manifest[FPSU_MAX_PATH];

    if (!root_path || !title_id || !target_path || !backup_path ||
        target_path_size == 0 || backup_path_size == 0) {
        return -1;
    }

    snprintf(manifest, sizeof(manifest), "%s/%s/manifest.txt", root_path, title_id);
    return read_manifest_latest(manifest, target_path, target_path_size, backup_path, backup_path_size);
}

int backup_find_latest_for_title(const char *title_id, char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size)
{
    if (find_latest_for_title_in_root(FPSU_BACKUP_ROOT, title_id,
            target_path, target_path_size, backup_path, backup_path_size) == 0) {
        return 0;
    }

    return find_latest_for_title_in_root(FPSU_LEGACY_BACKUP_ROOT, title_id,
        target_path, target_path_size, backup_path, backup_path_size);
}

int backup_restore_all_for_title(const char *title_id, int *restored_count)
{
    char manifest[FPSU_MAX_PATH];
    int restored = 0;

    if (!title_id || !restored_count) {
        return -1;
    }

    snprintf(manifest, sizeof(manifest), "%s/%s/manifest.txt", FPSU_BACKUP_ROOT, title_id);
    restore_manifest_first_backups(manifest, &restored);

    snprintf(manifest, sizeof(manifest), "%s/%s/manifest.txt", FPSU_LEGACY_BACKUP_ROOT, title_id);
    restore_manifest_first_backups(manifest, &restored);

    *restored_count = restored;
    return restored > 0 ? 0 : -1;
}

static int find_latest_in_root(const char *root_path, char *title_id, size_t title_id_size,
    char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size,
    unsigned long *best_stamp, int *found)
{
    DIR *root;
    struct dirent *entry;

    if (!root_path || !title_id || !target_path || !backup_path || !best_stamp || !found ||
        title_id_size == 0 || target_path_size == 0 || backup_path_size == 0) {
        return -1;
    }

    root = opendir(root_path);
    if (!root) {
        return -1;
    }

    while ((entry = readdir(root)) != NULL) {
        char manifest[FPSU_MAX_PATH];
        char current_target[FPSU_MAX_PATH];
        char current_backup[FPSU_MAX_PATH];
        struct stat st;
        unsigned long current_stamp = 0;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strlen(entry->d_name) > 31) {
            continue;
        }

        snprintf(manifest, sizeof(manifest), "%s/%.31s/manifest.txt", root_path, entry->d_name);
        if (read_manifest_latest(manifest, current_target, sizeof(current_target), current_backup, sizeof(current_backup)) != 0) {
            continue;
        }
        if (stat(current_backup, &st) == 0) {
            current_stamp = (unsigned long)st.st_mtime;
        }
        if (!*found || current_stamp >= *best_stamp) {
            *best_stamp = current_stamp;
            snprintf(title_id, title_id_size, "%s", entry->d_name);
            snprintf(target_path, target_path_size, "%s", current_target);
            snprintf(backup_path, backup_path_size, "%s", current_backup);
            *found = 1;
        }
    }

    closedir(root);
    return 0;
}

int backup_find_latest(char *title_id, size_t title_id_size, char *target_path, size_t target_path_size, char *backup_path, size_t backup_path_size)
{
    unsigned long best_stamp = 0;
    int found = 0;

    find_latest_in_root(FPSU_BACKUP_ROOT, title_id, title_id_size,
        target_path, target_path_size, backup_path, backup_path_size, &best_stamp, &found);
    find_latest_in_root(FPSU_LEGACY_BACKUP_ROOT, title_id, title_id_size,
        target_path, target_path_size, backup_path, backup_path_size, &best_stamp, &found);

    return found ? 0 : -1;
}
