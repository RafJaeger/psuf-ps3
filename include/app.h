#ifndef FPSU_APP_H
#define FPSU_APP_H

#include <stddef.h>
#include <stdint.h>

#define FPSU_APP_ID "FPSU00001"
#define FPSU_VERSION_LABEL "PSUF V2.5.28"
#define FPSU_HEN_DIAG_BUILD 0
#define FPSU_APP_USRDIR "/dev_hdd0/game/FPSU00001/USRDIR"
#define FPSU_DATA_ROOT "/dev_hdd0/FPSU"
#define FPSU_PATCH_DB_BUNDLED FPSU_APP_USRDIR "/patches.csv"
#define FPSU_PATCH_DB_UPDATED FPSU_DATA_ROOT "/patches.csv"
#define FPSU_PATCH_DB FPSU_PATCH_DB_BUNDLED
#define FPSU_GRAPHICS_DB FPSU_APP_USRDIR "/graphics_patches.csv"
#define FPSU_GRAPHICS_DB_UPDATED FPSU_DATA_ROOT "/graphics_patches.csv"
#define FPSU_NATIVE60_DB FPSU_APP_USRDIR "/native60.csv"
#define FPSU_NATIVE60_DB_UPDATED FPSU_DATA_ROOT "/native60.csv"
#define FPSU_FIX_DB FPSU_APP_USRDIR "/fix_patches.csv"
#define FPSU_FIX_DB_UPDATED FPSU_DATA_ROOT "/fix_patches.csv"
#define FPSU_UPDATE_URL_FILE FPSU_APP_USRDIR "/update_url.txt"
#define FPSU_UPDATE_TMP FPSU_DATA_ROOT "/patches.download"
#define FPSU_BACKUP_ROOT FPSU_DATA_ROOT "/backups"
#define FPSU_LEGACY_BACKUP_ROOT FPSU_APP_USRDIR "/backups"
#define FPSU_CACHE_ROOT FPSU_DATA_ROOT "/cache"
#define FPSU_SCAN_CACHE FPSU_CACHE_ROOT "/scan_cache.csv"
#define FPSU_PROGRESS_CACHE FPSU_CACHE_ROOT "/progress.txt"

#define FPSU_MAX_PATH 768
#define FPSU_MAX_TITLE 128
#define FPSU_MAX_GAMES 256
#define FPSU_SCAN_CHUNK 65536
#define FPSU_EBOOT_SCAN_LIMIT (128u * 1024u * 1024u)
#define FPSU_MAX_PAYLOAD 8192
#define FPSU_DEFAULT_PATCH_DELAY_SECONDS 120

typedef enum {
    FPSU_LANG_EN = 0,
    FPSU_LANG_PT = 1,
    FPSU_LANG_ES = 2
} fpsu_lang;

typedef enum {
    FPSU_PATCH_NONE = 0,
    FPSU_PATCH_60 = 1,
    FPSU_PATCH_UNLOCK = 2,
    FPSU_PATCH_30 = 3
} fpsu_patch_kind;

typedef enum {
    FPSU_STATUS_NOT_FOUND = 0,
    FPSU_STATUS_KNOWN = 1,
    FPSU_STATUS_UNTESTED = 2,
    FPSU_STATUS_PC_REQUIRED = 3,
    FPSU_STATUS_OTHER_VERSION = 4,
    FPSU_STATUS_APPLICABLE = 5,
    FPSU_STATUS_UNAVAILABLE = 6,
    FPSU_STATUS_NATIVE_60 = 7,
    FPSU_STATUS_UPDATE_REQUIRED = 8
} fpsu_patch_status;

typedef enum {
    FPSU_METHOD_NONE = 0,
    FPSU_METHOD_NCL = 1,
    FPSU_METHOD_CONFIG = 2,
    FPSU_METHOD_EBOOT_PC = 3,
    FPSU_METHOD_PATTERN_ONLY = 4,
    FPSU_METHOD_NCL_CONSTANT = 5
} fpsu_patch_method;

static inline int fpsu_method_is_ncl_write(fpsu_patch_method method)
{
    return method == FPSU_METHOD_NCL || method == FPSU_METHOD_NCL_CONSTANT;
}

typedef struct {
    char title_id[16];
    char version[16];
    char category[8];
    char title[FPSU_MAX_TITLE];
    char base_path[FPSU_MAX_PATH];
    char sfo_path[FPSU_MAX_PATH];
    char eboot_path[FPSU_MAX_PATH];
    int is_disc_folder;
    int is_iso;
} fpsu_game;

typedef struct {
    fpsu_patch_kind kind;
    fpsu_patch_status status;
    fpsu_patch_method method;
    char label[64];
    char note[160];
    char source[64];
    char payload[FPSU_MAX_PAYLOAD];
    int delay_seconds;
} fpsu_patch_option;

typedef struct {
    fpsu_game game;
    fpsu_patch_option fps60;
    fpsu_patch_option unlock;
    fpsu_patch_option fps30;
    int pattern_candidate;
} fpsu_game_result;

#endif
