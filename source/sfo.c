#include "sfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char key_offset[2];
    unsigned char format[2];
    unsigned char length[4];
    unsigned char max_length[4];
    unsigned char data_offset[4];
} sfo_index_entry;

static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static void safe_copy(char *dst, size_t dst_size, const char *src, size_t src_len)
{
    size_t n;
    if (!dst || dst_size == 0) {
        return;
    }
    n = src_len;
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void set_sfo_value(fpsu_game *game, const char *key, const char *value, size_t len)
{
    if (strcmp(key, "TITLE_ID") == 0) {
        safe_copy(game->title_id, sizeof(game->title_id), value, len);
    } else if (strcmp(key, "CATEGORY") == 0) {
        safe_copy(game->category, sizeof(game->category), value, len);
    } else if (strcmp(key, "TITLE") == 0 || strcmp(key, "TITLE_00") == 0) {
        if (game->title[0] == '\0' || strcmp(key, "TITLE") == 0) {
            safe_copy(game->title, sizeof(game->title), value, len);
        }
    }
}

int sfo_parse_game_buffer(const unsigned char *buf, size_t size, fpsu_game *game)
{
    uint32_t key_table;
    uint32_t data_table;
    uint32_t count;
    uint32_t i;
    char app_ver[16];
    char version_value[16];

    if (!buf || !game || size < 20) {
        return -1;
    }
    if (buf[0] != 0x00 || buf[1] != 'P' || buf[2] != 'S' || buf[3] != 'F') {
        return -1;
    }

    key_table = read_le32(buf + 8);
    data_table = read_le32(buf + 12);
    count = read_le32(buf + 16);
    if (count > 256 || key_table < 20 || data_table <= key_table || data_table >= size) {
        return -1;
    }

    memset(game, 0, sizeof(*game));
    app_ver[0] = '\0';
    version_value[0] = '\0';
    for (i = 0; i < count; ++i) {
        size_t entry_offset = 20u + (size_t)i * sizeof(sfo_index_entry);
        const sfo_index_entry *entry;
        uint16_t key_off;
        uint32_t len;
        uint32_t data_off;
        const char *key;
        const char *value;
        size_t key_pos;
        size_t data_pos;

        if (entry_offset + sizeof(sfo_index_entry) > size) {
            break;
        }
        entry = (const sfo_index_entry *)(buf + entry_offset);
        key_off = read_le16(entry->key_offset);
        len = read_le32(entry->length);
        data_off = read_le32(entry->data_offset);
        key_pos = (size_t)key_table + key_off;
        data_pos = (size_t)data_table + data_off;

        if (key_pos >= size || data_pos >= size || len > size - data_pos) {
            continue;
        }
        key = (const char *)(buf + key_pos);
        value = (const char *)(buf + data_pos);
        while (len > 0 && value[len - 1] == '\0') {
            --len;
        }
        if (strcmp(key, "APP_VER") == 0) {
            safe_copy(app_ver, sizeof(app_ver), value, len);
            continue;
        }
        if (strcmp(key, "VERSION") == 0) {
            safe_copy(version_value, sizeof(version_value), value, len);
            continue;
        }
        set_sfo_value(game, key, value, len);
    }

    if (strcmp(game->category, "DG") == 0 && version_value[0] != '\0') {
        safe_copy(game->version, sizeof(game->version), version_value, strlen(version_value));
    } else if (app_ver[0] != '\0') {
        safe_copy(game->version, sizeof(game->version), app_ver, strlen(app_ver));
    } else if (version_value[0] != '\0') {
        safe_copy(game->version, sizeof(game->version), version_value, strlen(version_value));
    }
    if (game->title[0] == '\0') {
        safe_copy(game->title, sizeof(game->title), "Unknown title", 13);
    }
    if (game->version[0] == '\0') {
        safe_copy(game->version, sizeof(game->version), "00.00", 5);
    }
    return game->title_id[0] ? 0 : -1;
}

int sfo_read_game(const char *path, fpsu_game *game)
{
    FILE *f;
    unsigned char *buf;
    long size;
    int result;

    if (!path || !game) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    size = ftell(f);
    if (size <= 0 || size > 1024 * 1024 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    result = sfo_parse_game_buffer(buf, (size_t)size, game);
    free(buf);
    if (result == 0) {
        safe_copy(game->sfo_path, sizeof(game->sfo_path), path, strlen(path));
    }
    return result;
}
