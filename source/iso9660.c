#include "iso9660.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define ISO_SECTOR_SIZE 2048u
#define ISO_PVD_SECTOR 16u
#define ISO_MAX_DIRECTORY (8u * 1024u * 1024u)
#define ISO_MAX_FILE (2u * 1024u * 1024u)

typedef struct {
    uint32_t extent;
    uint32_t size;
    int is_directory;
} iso_entry;

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int seek_extent(FILE *f, uint32_t extent)
{
    unsigned long long offset = (unsigned long long)extent * ISO_SECTOR_SIZE;
    return fseek(f, (long)offset, SEEK_SET);
}

static int read_extent(FILE *f, uint32_t extent, uint32_t size,
    unsigned char **out, size_t max_size)
{
    unsigned char *data;
    if (!f || !out || size == 0 || size > max_size) {
        return -1;
    }
    if (seek_extent(f, extent) != 0) {
        return -1;
    }
    data = (unsigned char *)malloc((size_t)size);
    if (!data) {
        return -1;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        return -1;
    }
    *out = data;
    return 0;
}

static void normalize_iso_name(char *out, size_t out_size,
    const unsigned char *name, size_t name_len)
{
    size_t i;
    size_t n = 0;
    if (!out || out_size == 0) {
        return;
    }
    for (i = 0; i < name_len && n + 1 < out_size; ++i) {
        unsigned char c = name[i];
        if (c == ';') {
            break;
        }
        out[n++] = (char)toupper(c);
    }
    while (n > 0 && out[n - 1] == '.') {
        --n;
    }
    out[n] = '\0';
}

static int name_equal(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (!a || !b) {
        return 0;
    }
    while (*a || *b) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (toupper(ca) != toupper(cb)) {
            return 0;
        }
    }
    return 1;
}

static int find_in_directory(FILE *f, uint32_t extent, uint32_t dir_size,
    const char *wanted, iso_entry *out)
{
    unsigned char *dir = NULL;
    size_t pos = 0;
    int found = -1;

    if (!wanted || !out || dir_size > ISO_MAX_DIRECTORY) {
        return -1;
    }
    if (read_extent(f, extent, dir_size, &dir, ISO_MAX_DIRECTORY) != 0) {
        return -1;
    }

    while (pos < dir_size) {
        unsigned int record_len = dir[pos];
        const unsigned char *record;
        unsigned int name_len;
        char name[256];

        if (record_len == 0) {
            size_t next_sector = ((pos / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            if (next_sector <= pos) {
                break;
            }
            pos = next_sector;
            continue;
        }
        if (record_len < 34 || pos + record_len > dir_size) {
            break;
        }
        record = dir + pos;
        name_len = record[32];
        if (33u + name_len <= record_len &&
            !(name_len == 1 && (record[33] == 0 || record[33] == 1))) {
            normalize_iso_name(name, sizeof(name), record + 33, name_len);
            if (name_equal(name, wanted)) {
                out->extent = read_le32(record + 2);
                out->size = read_le32(record + 10);
                out->is_directory = (record[25] & 0x02) != 0;
                found = 0;
                break;
            }
        }
        pos += record_len;
    }

    free(dir);
    return found;
}

int iso9660_read_file(const char *iso_path, const char *inner_path,
    unsigned char **out_data, size_t *out_size)
{
    FILE *f;
    unsigned char pvd[ISO_SECTOR_SIZE];
    iso_entry current;
    char path[512];
    char *cursor;
    char *part;

    if (!iso_path || !inner_path || !out_data || !out_size) {
        return -1;
    }
    *out_data = NULL;
    *out_size = 0;

    f = fopen(iso_path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, (long)(ISO_PVD_SECTOR * ISO_SECTOR_SIZE), SEEK_SET) != 0 ||
        fread(pvd, 1, sizeof(pvd), f) != sizeof(pvd) ||
        pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        fclose(f);
        return -1;
    }

    current.extent = read_le32(pvd + 156 + 2);
    current.size = read_le32(pvd + 156 + 10);
    current.is_directory = 1;

    snprintf(path, sizeof(path), "%s", inner_path);
    cursor = path;
    while (*cursor == '/') {
        ++cursor;
    }
    part = cursor;
    while (part && *part) {
        char *slash = strchr(part, '/');
        iso_entry next;
        if (slash) {
            *slash = '\0';
        }
        if (!current.is_directory || find_in_directory(f, current.extent, current.size, part, &next) != 0) {
            fclose(f);
            return -1;
        }
        current = next;
        part = slash ? slash + 1 : NULL;
        while (part && *part == '/') {
            ++part;
        }
    }

    if (current.is_directory || current.size == 0 || current.size > ISO_MAX_FILE ||
        read_extent(f, current.extent, current.size, out_data, ISO_MAX_FILE) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    *out_size = current.size;
    return 0;
}
